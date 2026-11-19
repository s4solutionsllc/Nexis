#ifndef CRUMBS_SCANNER_H
#define CRUMBS_SCANNER_H

#include <QAtomicInt>
#include <QFuture>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

#include "nexis-core_global.h"

// FR-123: find residual files left behind in ~/Library/* after an app
// (.app bundle or Homebrew cask) has been uninstalled. macOS only.
//
// Matching policy is conservative: bundle-id prefix match against the
// leaf filename/dirname. Deep content inspection is intentionally out
// of scope to avoid false positives.
namespace CrumbsScanner {

struct NEXISCORESHARED_EXPORT CrumbCandidate {
    QString path;
    qint64  sizeBytes = 0;
    QString matchedBundleId;   // which id in the input produced this hit
    QString sharedWith;        // SSO-27151: still-installed sibling that also claims it
    QString sharedReason;      // SSO-27151: user-facing reason; empty = not shared
};

// Walks the standard ~/Library/* roots and returns every entry whose
// leaf name begins with any of the supplied bundle ids. Results are
// returned sorted by sizeBytes desc so the UI can show the largest
// crumbs first.
//
// SSO-27151: `installedBundleIds` lists the apps still installed. A match that
// one of them also claims (a longer bundle id sharing the prefix, or the
// unsuffixed profile of a channel variant still installed — see
// SharedDataGuard) is returned with sharedWith / sharedReason set so the
// review dialog can exclude it by default.
//
// `cancelled` (if non-null) is polled between scan roots and between
// entries within a root, so an async caller can abort a long-running walk.
// `itemFoundCb` (if set) is invoked once per match, in discovery order, as
// each scan-location/root is walked — before the final sort. This is the
// hook async callers (see CrumbsScanRunner below) use to stream results
// instead of waiting for the whole walk to finish.
NEXISCORESHARED_EXPORT QList<CrumbCandidate> scanCrumbs(
    const QStringList &bundleIds,
    QAtomicInt *cancelled = nullptr,
    const std::function<void(const CrumbCandidate &)> &itemFoundCb = {},
    const QStringList &installedBundleIds = {});

// Test seam: identical walk to scanCrumbs(), but rooted at an explicit
// "home" directory instead of the real QStandardPaths::HomeLocation, so
// unit tests can point it at a QTemporaryDir-backed fake ~/Library tree
// (mirrors DirSizeScanner::scanSynchronous() taking an explicit rootPath).
// scanCrumbs() is a thin wrapper around this with homeDir = the real home.
NEXISCORESHARED_EXPORT QList<CrumbCandidate> scanCrumbsUnderHome(
    const QString &homeDir,
    const QStringList &bundleIds,
    QAtomicInt *cancelled = nullptr,
    const std::function<void(const CrumbCandidate &)> &itemFoundCb = {},
    const QStringList &installedBundleIds = {});

} // namespace CrumbsScanner

Q_DECLARE_METATYPE(CrumbsScanner::CrumbCandidate)

// Async wrapper around CrumbsScanner::scanCrumbs(), mirroring the
// TrustSafetyRunner incremental-emission pattern (SSO-15380,
// shared/nexis/Common/trust_safety_runner.h) so the app's Trust & Safety
// review surfaces behave consistently: a QObject worker that runs the scan
// off the UI thread via QtConcurrent and streams itemFound() as matches are
// discovered, rather than blocking until the whole walk completes.
class NEXISCORESHARED_EXPORT CrumbsScanRunner : public QObject
{
    Q_OBJECT

public:
    explicit CrumbsScanRunner(QObject *parent = nullptr);
    ~CrumbsScanRunner() override;

    // No-op if a scan is already running.
    void startScan(const QStringList &bundleIds, const QStringList &installedBundleIds = {});
    void cancelScan();
    bool isScanning() const;

signals:
    void itemFound(CrumbsScanner::CrumbCandidate item);
    void scanFinished(QList<CrumbsScanner::CrumbCandidate> items);
    void scanCancelled();

private:
    QAtomicInt mCancelled{0};
    QFuture<void> mScanFuture;
};

#endif // CRUMBS_SCANNER_H
