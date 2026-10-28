#include "resource_history_service.h"

#include "Managers/data_refresh_service.h"
#include "Managers/info_manager.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"

#include <QCoreApplication>
#include <QDateTime>

ResourceHistoryService *ResourceHistoryService::instance = nullptr;

ResourceHistoryService::ResourceHistoryService(InfoManager *infoManager,
                                                 DataRefreshService *refreshService,
                                                 SettingManager *settingManager,
                                                 QObject *parent)
    : QObject(parent),
      im(infoManager ? infoManager : InfoManager::ins()),
      mRefresh(refreshService ? refreshService : DataRefreshService::ins()),
      mSettings(settingManager ? settingManager : SettingManager::ins()),
      mHistoryTimer(new QTimer(this)),
      mFlushTimer(new QTimer(this)),
      mPruneTimer(new QTimer(this))
{
    connect(mHistoryTimer, &QTimer::timeout, this, &ResourceHistoryService::onHistoryTick);
    connect(mFlushTimer, &QTimer::timeout, this, &ResourceHistoryService::onFlushTick);
    connect(mPruneTimer, &QTimer::timeout, this, &ResourceHistoryService::onPruneTick);

    // DataRefreshService's own sigAppVisibilityChanged handler is connected
    // in its constructor, which runs the first time anyone calls
    // DataRefreshService::ins() — well before this service is constructed in
    // App's startup sequence. Qt invokes directly-connected slots in
    // connection order, so by the time onVisibilityChanged() below runs,
    // mRefresh->isPaused() already reflects the new state.
    connect(SignalMapper::ins(), &SignalMapper::sigAppVisibilityChanged,
            this, &ResourceHistoryService::onVisibilityChanged);

    connect(qApp, &QCoreApplication::aboutToQuit, this, &ResourceHistoryService::flush);
}

ResourceHistoryService *ResourceHistoryService::ins()
{
    if (!instance)
        instance = new ResourceHistoryService;
    return instance;
}

QString ResourceHistoryService::dbPath() const
{
    return mSettings->getConfigPath() + QStringLiteral("/resource_history.db");
}

void ResourceHistoryService::setEnabled(bool enabled)
{
    if (enabled == mEnabled)
        return;
    mEnabled = enabled;

    if (mEnabled) {
        if (!mStore.isOpen())
            mStore.open(dbPath());
        updateLiveSubscription();
        onPruneTick(); // B3: prune once at start, then hourly
        mHistoryTimer->start(10000);
        mFlushTimer->start(60000);
        mPruneTimer->start(3600000);
    } else {
        mHistoryTimer->stop();
        mFlushTimer->stop();
        mPruneTimer->stop();
        flush();

        if (mSubscribedLive) {
            disconnect(mRefresh, &DataRefreshService::cpuUpdated, this, &ResourceHistoryService::onCpuUpdated);
            disconnect(mRefresh, &DataRefreshService::memoryUpdated, this, &ResourceHistoryService::onMemoryUpdated);
            mRefresh->unsubscribe(DataRefreshService::Signal::Cpu);
            mRefresh->unsubscribe(DataRefreshService::Signal::Memory);
            mSubscribedLive = false;
        }

        // B1: OFF means no timer, no subscriptions, no DB open.
        mStore.close();
        resetLiveAccumulators();
        mHasMemSnapshot = false;
    }
}

void ResourceHistoryService::updateLiveSubscription()
{
    const bool shouldSubscribe = mEnabled && !mRefresh->isPaused();
    if (shouldSubscribe == mSubscribedLive)
        return;

    if (shouldSubscribe) {
        mRefresh->subscribe(DataRefreshService::Signal::Cpu);
        mRefresh->subscribe(DataRefreshService::Signal::Memory);
        connect(mRefresh, &DataRefreshService::cpuUpdated,
                this, &ResourceHistoryService::onCpuUpdated);
        connect(mRefresh, &DataRefreshService::memoryUpdated,
                this, &ResourceHistoryService::onMemoryUpdated);
        resetLiveAccumulators();
    } else {
        disconnect(mRefresh, &DataRefreshService::cpuUpdated, this, &ResourceHistoryService::onCpuUpdated);
        disconnect(mRefresh, &DataRefreshService::memoryUpdated, this, &ResourceHistoryService::onMemoryUpdated);
        mRefresh->unsubscribe(DataRefreshService::Signal::Cpu);
        mRefresh->unsubscribe(DataRefreshService::Signal::Memory);
    }
    mSubscribedLive = shouldSubscribe;
}

void ResourceHistoryService::onVisibilityChanged(bool visible)
{
    Q_UNUSED(visible)
    if (mEnabled)
        updateLiveSubscription();
}

void ResourceHistoryService::onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)
    if (percents.isEmpty())
        return;

    const double pct = percents.at(0);
    mLiveCpuSum += pct;
    mLiveCpuMax = qMax(mLiveCpuMax, pct);
    ++mLiveCpuCount;
    mLastLoadAvgs = loadAvgs;
}

void ResourceHistoryService::onMemoryUpdated(const MemorySnapshot &snap)
{
    mLastMemSnapshot = snap;
    mHasMemSnapshot = true;
}

void ResourceHistoryService::resetLiveAccumulators()
{
    mLiveCpuSum = 0.0;
    mLiveCpuMax = 0.0;
    mLiveCpuCount = 0;
    // mLastLoadAvgs / mLastMemSnapshot / mHasMemSnapshot are deliberately
    // NOT reset here — they're last-known-value caches. A 10s window with no
    // fresh memoryUpdated tick should still report the last known reading
    // rather than going NULL.
}

void ResourceHistoryService::onHistoryTick()
{
    if (!mEnabled)
        return;

    const qint64 ts = QDateTime::currentSecsSinceEpoch();

    if (mRefresh->isPaused()) {
        // Window hidden/minimized: DataRefreshService isn't running, so
        // nothing else is reading CpuInfo/MemoryInfo right now — safe to
        // sample them directly.
        const QList<int> percents = im->getCpuPercents();
        if (!DataRefreshService::isCpuPayloadEmittable(percents))
            return; // transient read failure; skip this tick, same as DataRefreshService

        const QList<double> loadAvgs = im->getCpuLoadAvgs();

        im->updateMemoryInfo();
        MemorySnapshot snap;
        snap.used = im->getMemUsed();
        snap.total = im->getMemTotal();
        snap.swapUsed = im->getSwapUsed();
        snap.swapTotal = im->getSwapTotal();

        mPendingBuffer.append(makeBackgroundSample(ts, percents.at(0), loadAvgs, snap));
    } else {
        mPendingBuffer.append(makeLiveSample(ts, mLiveCpuSum, mLiveCpuMax, mLiveCpuCount,
                                              mLastLoadAvgs, mHasMemSnapshot, mLastMemSnapshot));
        resetLiveAccumulators();
    }
}

void ResourceHistoryService::onFlushTick()
{
    flush();
}

void ResourceHistoryService::flush()
{
    if (mPendingBuffer.isEmpty())
        return;

    if (mStore.isOpen() && mStore.appendBatch(mPendingBuffer))
        emit historyChanged();

    // Drop the buffer either way — a store that's failing (locked, disk
    // full) shouldn't grow this list without bound; the store already
    // logged the failure.
    mPendingBuffer.clear();
}

void ResourceHistoryService::onPruneTick()
{
    if (!mStore.isOpen())
        return;

    const int days = mSettings->getResourceHistoryRetentionDays();
    const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(days) * 86400;
    mStore.pruneOlderThan(cutoff);
}

QList<ResourceHistoryStore::Bucket> ResourceHistoryService::queryBuckets(qint64 fromTs, qint64 toTs, qint64 bucketSecs) const
{
    if (mStore.isOpen())
        return mStore.queryBuckets(fromTs, toTs, bucketSecs);

    ResourceHistoryStore temp;
    if (!temp.open(dbPath()))
        return {};
    return temp.queryBuckets(fromTs, toTs, bucketSecs);
}

QList<ResourceHistoryStore::Sample> ResourceHistoryService::queryRaw(qint64 fromTs, qint64 toTs) const
{
    if (mStore.isOpen())
        return mStore.queryRaw(fromTs, toTs);

    ResourceHistoryStore temp;
    if (!temp.open(dbPath()))
        return {};
    return temp.queryRaw(fromTs, toTs);
}

qint64 ResourceHistoryService::rowCount() const
{
    if (mStore.isOpen())
        return mStore.rowCount();

    ResourceHistoryStore temp;
    if (!temp.open(dbPath()))
        return 0;
    return temp.rowCount();
}

bool ResourceHistoryService::clearAll()
{
    bool ok;
    if (mStore.isOpen()) {
        ok = mStore.clearAll();
    } else {
        ResourceHistoryStore temp;
        ok = temp.open(dbPath()) && temp.clearAll();
    }
    if (ok)
        emit historyChanged();
    return ok;
}

ResourceHistoryStore::Sample ResourceHistoryService::makeLiveSample(
    qint64 ts, double cpuSum, double cpuMax, int cpuSampleCount,
    const QList<double> &loadAvgs, bool hasMemSnapshot, const MemorySnapshot &mem)
{
    ResourceHistoryStore::Sample s;
    s.ts = ts;
    s.source = ResourceHistoryStore::SampleSource::LiveAggregated;

    if (cpuSampleCount > 0) {
        s.cpuPct = cpuSum / cpuSampleCount;
        s.cpuMaxPct = cpuMax;
    }

    if (loadAvgs.size() >= 3) {
        s.load1 = loadAvgs.at(0);
        s.load5 = loadAvgs.at(1);
        s.load15 = loadAvgs.at(2);
    }

    if (hasMemSnapshot) {
        s.memUsedBytes = mem.used;
        s.memTotalBytes = mem.total;
        s.swapUsedBytes = mem.swapUsed;
        s.swapTotalBytes = mem.swapTotal;
    }

    return s;
}

ResourceHistoryStore::Sample ResourceHistoryService::makeBackgroundSample(
    qint64 ts, double cpuPct, const QList<double> &loadAvgs, const MemorySnapshot &mem)
{
    ResourceHistoryStore::Sample s;
    s.ts = ts;
    s.source = ResourceHistoryStore::SampleSource::BackgroundDirect;
    s.cpuPct = cpuPct;
    // cpu_max_pct intentionally left NULL — a single direct sample has no
    // "max over the window" to report (spec Decision 1).

    if (loadAvgs.size() >= 3) {
        s.load1 = loadAvgs.at(0);
        s.load5 = loadAvgs.at(1);
        s.load15 = loadAvgs.at(2);
    }

    s.memUsedBytes = mem.used;
    s.memTotalBytes = mem.total;
    s.swapUsedBytes = mem.swapUsed;
    s.swapTotalBytes = mem.swapTotal;

    return s;
}
