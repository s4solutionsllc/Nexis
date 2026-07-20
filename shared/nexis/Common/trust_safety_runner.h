// SSO-15380: Trust & Safety shared component — async scan/execute orchestration.
//
// Mirrors the DirSizeScanner pattern (shared/nexis/Managers/dir_size_scanner.h):
// a QObject worker that runs a provider off the UI thread via QtConcurrent,
// polls a QAtomicInt cancel flag between units of work, and exposes the pure
// logic as static synchronous helpers so it's unit-testable without spinning
// up a thread or a QApplication.

#ifndef TRUST_SAFETY_RUNNER_H
#define TRUST_SAFETY_RUNNER_H

#include "trust_safety_types.h"

#include <QFuture>
#include <QObject>

class TrustSafetyRunner : public QObject
{
    Q_OBJECT

public:
    explicit TrustSafetyRunner(TrustSafetyActionProvider *provider, QObject *parent = nullptr);
    ~TrustSafetyRunner() override;

    // Scan phase. No-op if a scan is already running.
    void startScan();
    void cancelScan();
    bool isScanning() const;

    // Execution phase (real run when dryRun is false, zero-side-effect
    // simulation when true — same code path either way). No-op if an
    // execution is already running.
    void startExecution(const QList<TrustSafetyActionItem> &items, bool dryRun);
    void cancelExecution();
    bool isExecuting() const;

    // Pure, synchronous versions of the above — used internally by
    // start*() and directly by unit tests. `cancelled` may be null.
    static QList<TrustSafetyActionItem> scanSynchronous(
        TrustSafetyActionProvider *provider,
        QAtomicInt *cancelled,
        const std::function<void(const TrustSafetyActionItem &)> &itemFoundCb = {});

    static TrustSafetyRunSummary executeSynchronous(
        TrustSafetyActionProvider *provider,
        const QList<TrustSafetyActionItem> &items,
        bool dryRun,
        QAtomicInt *cancelled,
        const std::function<void(const TrustSafetyActionResult &result, int itemsDone, int itemsTotal, qint64 bytesFreedSoFar)> &itemDoneCb = {});

signals:
    void itemDiscovered(TrustSafetyActionItem item);
    void scanFinished(QList<TrustSafetyActionItem> items);
    void scanCancelled();

    void itemProcessed(TrustSafetyActionResult result);
    void executionProgress(int itemsDone, int itemsTotal, qint64 bytesFreedSoFar);
    void executionFinished(TrustSafetyRunSummary summary);

private:
    TrustSafetyActionProvider *mProvider;
    QAtomicInt mScanCancelled{0};
    QAtomicInt mExecCancelled{0};
    QFuture<void> mScanFuture;
    QFuture<void> mExecFuture;
};

#endif // TRUST_SAFETY_RUNNER_H
