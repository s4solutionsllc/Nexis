#ifndef RESOURCE_HISTORY_SERVICE_H
#define RESOURCE_HISTORY_SERVICE_H

#include <QObject>
#include <QTimer>
#include <QList>

#include <Info/memory_info.h>
#include <Tools/resource_history_store.h>

class InfoManager;
class DataRefreshService;
class SettingManager;

// SSO-10930 / GH#220: persistent, cross-session CPU/load/memory history,
// opt-in and off by default. Unlike every other DataRefreshService consumer
// (TrayHealthMonitor, ResourcesPage, ...), this one must keep sampling while
// the window is hidden/minimized — that's the whole point of the feature,
// since GH#220 asks to diagnose issues "tied to specific games", which is
// exactly when Nexis is minimized.
//
// To do that without reversing DataRefreshService's deliberate hidden-pause
// behaviour (docs/ARCHITECTURE_REVIEW.md), this owns its own low-rate (10s)
// QTimer that runs independently of window visibility:
//  - While DataRefreshService is running (window visible), this subscribes
//    to its cpuUpdated/memoryUpdated signals and aggregates (avg/max CPU%
//    over the 10s window) rather than calling CpuInfo itself — Linux's
//    CpuInfo keeps delta state in function-local statics, so a second
//    direct caller would skew the Resources page's live CPU%.
//  - While DataRefreshService::isPaused() (window hidden or in tray), this
//    samples CpuInfo/MemoryInfo directly on its own 10s tick. Nothing else
//    is reading those statics in that state, so it's safe.
// Each row is tagged with its source so the data stays honest about which
// path produced it.
class ResourceHistoryService : public QObject
{
    Q_OBJECT

public:
    static ResourceHistoryService *ins();

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    // Read access for the Resources page's historical view + CSV export.
    QList<ResourceHistoryStore::Bucket> queryBuckets(qint64 fromTs, qint64 toTs, qint64 bucketSecs) const;
    QList<ResourceHistoryStore::Sample> queryRaw(qint64 fromTs, qint64 toTs) const;
    qint64 rowCount() const;
    bool clearAll();

    // Pure aggregation helpers — exposed so tests can exercise the avg/max
    // and NULL-tolerance math (F2) without a running timer or live signals.
    static ResourceHistoryStore::Sample makeLiveSample(
        qint64 ts, double cpuSum, double cpuMax, int cpuSampleCount,
        const QList<double> &loadAvgs, bool hasMemSnapshot, const MemorySnapshot &mem);
    static ResourceHistoryStore::Sample makeBackgroundSample(
        qint64 ts, double cpuPct, const QList<double> &loadAvgs, const MemorySnapshot &mem);

signals:
    void historyChanged();

private slots:
    void onHistoryTick();
    void onFlushTick();
    void onPruneTick();
    void onVisibilityChanged(bool visible);
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);

private:
    explicit ResourceHistoryService(InfoManager *infoManager = nullptr,
                                     DataRefreshService *refreshService = nullptr,
                                     SettingManager *settingManager = nullptr,
                                     QObject *parent = nullptr);

    static ResourceHistoryService *instance;

    void updateLiveSubscription();
    void flush();
    void resetLiveAccumulators();
    QString dbPath() const;

    InfoManager *im;
    DataRefreshService *mRefresh;
    SettingManager *mSettings;

    ResourceHistoryStore mStore;

    QTimer *mHistoryTimer;
    QTimer *mFlushTimer;
    QTimer *mPruneTimer;

    bool mEnabled = false;
    bool mSubscribedLive = false;

    QList<ResourceHistoryStore::Sample> mPendingBuffer;

    // Live-aggregation accumulators, reset every 10s tick.
    double mLiveCpuSum = 0.0;
    double mLiveCpuMax = 0.0;
    int mLiveCpuCount = 0;
    QList<double> mLastLoadAvgs;
    bool mHasMemSnapshot = false;
    MemorySnapshot mLastMemSnapshot;
};

#endif // RESOURCE_HISTORY_SERVICE_H
