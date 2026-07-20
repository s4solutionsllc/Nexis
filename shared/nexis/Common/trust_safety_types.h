// SSO-15380: Trust & Safety shared component — data model.
//
// Any maintenance/cleaning surface (System Cleaner, Uninstaller, Disk Tools,
// Helpers, ...) that wants explain-before-run + itemized preview + working
// cancel + dry-run + risky-category warnings for free adopts this by:
//
//   1. Implementing TrustSafetyActionProvider::scan() / performItem() against
//      its own domain (files, packages, services, whatever).
//   2. Handing that provider to a TrustSafetyPreviewDialog
//      (see trust_safety_preview_dialog.h).
//
// See shared/nexis/Common/README.md for the full adoption guide.

#ifndef TRUST_SAFETY_TYPES_H
#define TRUST_SAFETY_TYPES_H

#include <QAtomicInt>
#include <QList>
#include <QMetaType>
#include <QString>

#include <functional>

// One item a Trust & Safety surface could act on: a single cache file, a
// package, a service, etc. Adopters build these from their own domain data.
struct TrustSafetyActionItem {
    enum class RiskTier {
        Standard,   // regular item; no extra confirmation gate
        Risky,      // e.g. system caches vs. browser cookies — extra confirm step
    };

    QString id;                 // stable within one preview session
    QString label;               // e.g. "com.example.app cache"
    QString description;         // plain-English, one sentence: what this item is / why it's listed
    QString command;             // exact underlying command/operation, e.g. "rm -rf ~/Library/Caches/com.example.app"
    QString categoryId;          // groups items in the preview tree
    QString categoryLabel;
    RiskTier riskTier = RiskTier::Standard;
    qint64 estimatedSizeBytes = -1; // -1 = unknown/not yet measured
};

// Outcome of running (or dry-running) a single item.
struct TrustSafetyActionResult {
    QString itemId;
    bool succeeded = false;
    qint64 bytesFreed = 0;
    QString error;       // empty when succeeded
};

// Aggregate outcome of one execution pass (real or dry-run).
struct TrustSafetyRunSummary {
    QList<TrustSafetyActionResult> results;
    qint64 totalBytesFreed = 0;
    int totalItemsSucceeded = 0;
    int totalItemsRequested = 0;
    bool cancelled = false;   // true if Stop was hit before all items ran —
                              // `results` still holds every item that DID
                              // complete, so partial progress is reportable.
    bool dryRun = false;
};

// Per-surface domain glue. Implementations own the actual filesystem/package/
// service access; the shared component only orchestrates + presents.
class TrustSafetyActionProvider
{
public:
    virtual ~TrustSafetyActionProvider() = default;

    // Discover candidate items. MUST call itemFound() incrementally as items
    // are found (not buffered until the end) so the preview can render sizes
    // as they arrive rather than all-or-nothing. MUST poll `cancelled`
    // between units of work (files, directories, packages, ...) and return
    // promptly once it is set — this is what makes Cancel/Stop real instead
    // of a UI-only dismiss. `cancelled` may be null (synchronous callers that
    // don't support cancelling a scan).
    virtual void scan(QAtomicInt *cancelled,
                       const std::function<void(const TrustSafetyActionItem &)> &itemFound) = 0;

    // Perform the underlying operation for a single item, or simulate it when
    // dryRun is true. dryRun MUST NOT cause any filesystem/registry/service
    // side effects — it still does enough work (e.g. stat a path) to report
    // an accurate result. Called once per selected item; the runner checks
    // the cancel flag between calls, so keep a single call reasonably short.
    virtual TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) = 0;
};

Q_DECLARE_METATYPE(TrustSafetyActionItem)
Q_DECLARE_METATYPE(TrustSafetyActionResult)
Q_DECLARE_METATYPE(TrustSafetyRunSummary)

#endif // TRUST_SAFETY_TYPES_H
