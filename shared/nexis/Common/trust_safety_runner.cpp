#include "trust_safety_runner.h"

#include <QtConcurrent>

TrustSafetyRunner::TrustSafetyRunner(TrustSafetyActionProvider *provider, QObject *parent)
    : QObject(parent), mProvider(provider)
{
    qRegisterMetaType<TrustSafetyActionItem>("TrustSafetyActionItem");
    qRegisterMetaType<TrustSafetyActionResult>("TrustSafetyActionResult");
    qRegisterMetaType<QList<TrustSafetyActionItem>>("QList<TrustSafetyActionItem>");
    qRegisterMetaType<TrustSafetyRunSummary>("TrustSafetyRunSummary");
}

TrustSafetyRunner::~TrustSafetyRunner()
{
    // Wait for workers before our `this` capture (used inside the
    // QtConcurrent lambdas below) goes dangling — same rationale as
    // DirSizeScanner's destructor.
    if (mScanFuture.isRunning()) {
        mScanCancelled.storeRelaxed(1);
        mScanFuture.waitForFinished();
    }
    if (mExecFuture.isRunning()) {
        mExecCancelled.storeRelaxed(1);
        mExecFuture.waitForFinished();
    }
}

void TrustSafetyRunner::startScan()
{
    if (mScanFuture.isRunning())
        return;
    mScanCancelled.storeRelaxed(0);

    mScanFuture = QtConcurrent::run([this]() {
        auto itemFoundCb = [this](const TrustSafetyActionItem &item) {
            emit itemDiscovered(item);
        };
        QList<TrustSafetyActionItem> items = scanSynchronous(mProvider, &mScanCancelled, itemFoundCb);
        if (mScanCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit scanFinished(items);
    });
}

void TrustSafetyRunner::cancelScan()
{
    mScanCancelled.storeRelaxed(1);
}

bool TrustSafetyRunner::isScanning() const
{
    return mScanFuture.isRunning();
}

void TrustSafetyRunner::startExecution(const QList<TrustSafetyActionItem> &items, bool dryRun)
{
    if (mExecFuture.isRunning())
        return;
    mExecCancelled.storeRelaxed(0);

    mExecFuture = QtConcurrent::run([this, items, dryRun]() {
        auto itemDoneCb = [this](const TrustSafetyActionResult &result, int done, int total, qint64 bytesFreedSoFar) {
            emit itemProcessed(result);
            emit executionProgress(done, total, bytesFreedSoFar);
        };
        TrustSafetyRunSummary summary =
            executeSynchronous(mProvider, items, dryRun, &mExecCancelled, itemDoneCb);
        emit executionFinished(summary);
    });
}

void TrustSafetyRunner::cancelExecution()
{
    mExecCancelled.storeRelaxed(1);
}

bool TrustSafetyRunner::isExecuting() const
{
    return mExecFuture.isRunning();
}

QList<TrustSafetyActionItem> TrustSafetyRunner::scanSynchronous(
    TrustSafetyActionProvider *provider,
    QAtomicInt *cancelled,
    const std::function<void(const TrustSafetyActionItem &)> &itemFoundCb)
{
    QList<TrustSafetyActionItem> items;
    if (!provider)
        return items;

    provider->scan(cancelled, [&](const TrustSafetyActionItem &item) {
        items.append(item);
        if (itemFoundCb)
            itemFoundCb(item);
    });
    return items;
}

TrustSafetyRunSummary TrustSafetyRunner::executeSynchronous(
    TrustSafetyActionProvider *provider,
    const QList<TrustSafetyActionItem> &items,
    bool dryRun,
    QAtomicInt *cancelled,
    const std::function<void(const TrustSafetyActionResult &, int, int, qint64)> &itemDoneCb)
{
    TrustSafetyRunSummary summary;
    summary.dryRun = dryRun;
    summary.totalItemsRequested = items.size();

    if (!provider)
        return summary;

    int done = 0;
    for (const TrustSafetyActionItem &item : items) {
        // Checked between items (not preempted mid-item) — matches the
        // DiskTools/DirSizeScanner cancel granularity already used
        // elsewhere in this codebase, and is enough for Cancel to actually
        // stop the operation rather than just dismissing the UI.
        if (cancelled && cancelled->loadRelaxed()) {
            summary.cancelled = true;
            break;
        }

        TrustSafetyActionResult result = provider->performItem(item, dryRun);
        result.itemId = item.id;
        summary.results.append(result);
        if (result.succeeded) {
            summary.totalBytesFreed += result.bytesFreed;
            summary.totalItemsSucceeded += 1;
        }
        done += 1;

        if (itemDoneCb)
            itemDoneCb(result, done, items.size(), summary.totalBytesFreed);
    }

    return summary;
}
