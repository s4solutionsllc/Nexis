// SSO-15480: TrustSafetyActionProvider implementation for System Cleaner.
//
// Wraps the already-scanned file lists (mRetained*) and routes execution
// through CleanerService (for user/elevated file deletion) and ToolManager
// (for snap/flatpak package-manager commands).

#ifndef SYSTEM_CLEANER_PROVIDER_H
#define SYSTEM_CLEANER_PROVIDER_H

#include <Common/trust_safety_types.h>
#include <Tools/package_tool_shared.h>

#include <QFileInfoList>
#include <QStringList>

class CleanerService;
class ToolManager;

class SystemCleanerProvider : public TrustSafetyActionProvider
{
public:
    // Category IDs used in TrustSafetyActionItem::categoryId.
    // String constants so callers can filter by category without casting.
    static constexpr const char *CAT_PACKAGE_CACHE      = "package_cache";
    static constexpr const char *CAT_CRASH_REPORTS      = "crash_reports";
    static constexpr const char *CAT_APP_LOGS           = "app_logs";
    static constexpr const char *CAT_APP_CACHES         = "app_caches";
    static constexpr const char *CAT_DEV_TOOL_CACHES    = "dev_tool_caches";
    static constexpr const char *CAT_BROKEN_SYMLINKS    = "broken_symlinks";
    static constexpr const char *CAT_BROWSER_PRIVACY    = "browser_privacy";
    static constexpr const char *CAT_TRASH              = "trash";
    static constexpr const char *CAT_SNAP_FLATPAK       = "snap_flatpak";

    // Item ID prefixes — used in performItem() to dispatch the correct operation.
    static constexpr const char *ID_PREFIX_TRASH        = "trash::";
    static constexpr const char *ID_PREFIX_SNAP         = "snap::";
    static constexpr const char *ID_FLATPAK_ALL         = "flatpak::all";

    struct Config {
        QFileInfoList packageCaches;
        QFileInfoList crashReports;
        QFileInfoList appLogs;
        QFileInfoList appCaches;
        QFileInfoList devToolCaches;
        QFileInfoList brokenSymlinks;
        QFileInfoList browserPrivacy;
        QStringList   trashRoots;
        QList<StaleSnapRevision> snapRevisions;
        QStringList   unusedFlatpakRefs;
    };

    explicit SystemCleanerProvider(Config config,
                                    CleanerService *cleanerService,
                                    ToolManager    *toolManager = nullptr);

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override;

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override;

private:
    Config         mConfig;
    CleanerService *mCleanerService;
    ToolManager    *mToolManager;
};

#endif // SYSTEM_CLEANER_PROVIDER_H
