// SSO-15480

#include "system_cleaner_provider.h"
#include <Managers/cleaner_service.h>
#include <Managers/tool_manager.h>
#include <Utils/file_util.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

// ─── Construction ──────────────────────────────────────────────────────────────

SystemCleanerProvider::SystemCleanerProvider(Config config,
                                              CleanerService *cleanerService,
                                              ToolManager    *toolManager)
    : mConfig(std::move(config))
    , mCleanerService(cleanerService)
    , mToolManager(toolManager)
{
}

// ─── scan ─────────────────────────────────────────────────────────────────────

void SystemCleanerProvider::scan(
    QAtomicInt *cancelled,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    struct CatSpec {
        const char *catId;
        const char *catLabel;
        const char *description;
        TrustSafetyActionItem::RiskTier riskTier;
        const QFileInfoList *files;
    };

    const QList<CatSpec> specs = {
        { CAT_PACKAGE_CACHE,
          QT_TR_NOOP("Package Caches"),
          QT_TR_NOOP("Package manager cache file; can be re-downloaded if needed."),
          TrustSafetyActionItem::RiskTier::Standard,
          &mConfig.packageCaches },
        { CAT_CRASH_REPORTS,
          QT_TR_NOOP("Crash Reports"),
          QT_TR_NOOP("Operating system crash report; safe to delete."),
          TrustSafetyActionItem::RiskTier::Standard,
          &mConfig.crashReports },
        { CAT_APP_LOGS,
          QT_TR_NOOP("Application Logs"),
          QT_TR_NOOP("Application log file; the app will recreate it automatically."),
          TrustSafetyActionItem::RiskTier::Standard,
          &mConfig.appLogs },
        { CAT_APP_CACHES,
          QT_TR_NOOP("Application Caches"),
          QT_TR_NOOP("Application cache directory; will be regenerated on next launch."),
          TrustSafetyActionItem::RiskTier::Standard,
          &mConfig.appCaches },
        { CAT_DEV_TOOL_CACHES,
          QT_TR_NOOP("Dev Tool Caches"),
          QT_TR_NOOP("Developer tool cache (Electron, VS Code, etc.); regenerated automatically."),
          TrustSafetyActionItem::RiskTier::Standard,
          &mConfig.devToolCaches },
        { CAT_BROKEN_SYMLINKS,
          QT_TR_NOOP("Broken Symlinks"),
          QT_TR_NOOP("Symbolic link pointing to a non-existent target; safe to remove."),
          TrustSafetyActionItem::RiskTier::Standard,
          &mConfig.brokenSymlinks },
        { CAT_BROWSER_PRIVACY,
          QT_TR_NOOP("Browser Privacy"),
          QT_TR_NOOP("Browser privacy data (cookies, history, or cached session); deleting this will sign you out of websites."),
          TrustSafetyActionItem::RiskTier::Risky,
          &mConfig.browserPrivacy },
    };

    for (const CatSpec &spec : specs) {
        for (const QFileInfo &fi : *spec.files) {
            if (cancelled && cancelled->loadRelaxed()) return;

            TrustSafetyActionItem item;
            item.id                 = fi.absoluteFilePath();   // path is a stable, unique id
            item.label              = fi.fileName();
            item.description        = QObject::tr(spec.description);
            item.command            = QStringLiteral("rm -rf ") + fi.absoluteFilePath();
            item.categoryId         = QLatin1String(spec.catId);
            item.categoryLabel      = QObject::tr(spec.catLabel);
            item.riskTier           = spec.riskTier;
            item.estimatedSizeBytes = static_cast<qint64>(FileUtil::getFileSize(fi.absoluteFilePath()));
            itemFound(item);
        }
    }

    // ── Trash roots ────────────────────────────────────────────────────────────

    for (const QString &trashRoot : mConfig.trashRoots) {
        if (cancelled && cancelled->loadRelaxed()) return;

        TrustSafetyActionItem item;
        item.id                 = QLatin1String(ID_PREFIX_TRASH) + trashRoot;
        item.label              = QFileInfo(trashRoot).fileName();
        item.description        = QObject::tr("Items in the Trash will be permanently deleted.");
        item.command            = QStringLiteral("rm -rf ") + trashRoot;
        item.categoryId         = QLatin1String(CAT_TRASH);
        item.categoryLabel      = QObject::tr("Trash");
        item.riskTier           = TrustSafetyActionItem::RiskTier::Risky;
        item.estimatedSizeBytes = static_cast<qint64>(FileUtil::getFileSize(trashRoot));
        itemFound(item);
    }

    // ── Stale snap revisions ───────────────────────────────────────────────────

    for (const StaleSnapRevision &rev : mConfig.snapRevisions) {
        if (cancelled && cancelled->loadRelaxed()) return;

        TrustSafetyActionItem item;
        item.id                 = QLatin1String(ID_PREFIX_SNAP) + rev.filePath;
        item.label              = QStringLiteral("%1 (rev %2)").arg(rev.name, rev.revision);
        item.description        = QObject::tr("Stale snap package revision; only the active revision is kept.");
        item.command            = QStringLiteral("snap remove --revision %1 %2").arg(rev.revision, rev.name);
        item.categoryId         = QLatin1String(CAT_SNAP_FLATPAK);
        item.categoryLabel      = QObject::tr("Snap/Flatpak Revisions");
        item.riskTier           = TrustSafetyActionItem::RiskTier::Standard;
        item.estimatedSizeBytes = static_cast<qint64>(rev.size);
        itemFound(item);
    }

    // ── Unused Flatpak runtimes — one aggregate item ───────────────────────────

    if (!mConfig.unusedFlatpakRefs.isEmpty()) {
        if (cancelled && cancelled->loadRelaxed()) return;

        qint64 totalSize = 0;
        for (const QString &ref : mConfig.unusedFlatpakRefs) {
            QString runtimeDir = QStringLiteral("/var/lib/flatpak/runtime/") + ref.section('/', 0, 0);
            totalSize += static_cast<qint64>(FileUtil::getFileSize(runtimeDir));
        }

        TrustSafetyActionItem item;
        item.id                 = QLatin1String(ID_FLATPAK_ALL);
        item.label              = QObject::tr("Unused Flatpak runtimes (%1)").arg(mConfig.unusedFlatpakRefs.size());
        item.description        = QObject::tr("Unused Flatpak runtimes not required by any installed application.");
        item.command            = QStringLiteral("flatpak uninstall --unused");
        item.categoryId         = QLatin1String(CAT_SNAP_FLATPAK);
        item.categoryLabel      = QObject::tr("Snap/Flatpak Revisions");
        item.riskTier           = TrustSafetyActionItem::RiskTier::Standard;
        item.estimatedSizeBytes = totalSize;
        itemFound(item);
    }
}

// ─── performItem ──────────────────────────────────────────────────────────────

TrustSafetyActionResult SystemCleanerProvider::performItem(
    const TrustSafetyActionItem &item, bool dryRun)
{
    TrustSafetyActionResult result;
    result.itemId = item.id;

    // ── Trash root ─────────────────────────────────────────────────────────────

    if (item.id.startsWith(QLatin1String(ID_PREFIX_TRASH))) {
        QString trashRoot = item.id.mid(qstrlen(ID_PREFIX_TRASH));

        // Measure before deletion (we return the freed space estimate)
        qint64 sizeBefore = static_cast<qint64>(FileUtil::getFileSize(trashRoot));

        if (!dryRun) {
            auto emptyDir = [](const QString &dirPath) {
                QDir dir(dirPath);
                if (!dir.exists()) return;
                for (const QFileInfo &entry : dir.entryInfoList(
                         QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
                    if (entry.isSymLink() || entry.isFile())
                        QFile::remove(entry.absoluteFilePath());
                    else if (entry.isDir())
                        QDir(entry.absoluteFilePath()).removeRecursively();
                }
            };

#ifdef Q_OS_MACOS
            emptyDir(trashRoot);
#else
            emptyDir(trashRoot + QStringLiteral("/files"));
            emptyDir(trashRoot + QStringLiteral("/info"));
#endif
        }

        result.succeeded  = true;
        result.bytesFreed = sizeBefore;
        return result;
    }

    // ── Stale snap revision ────────────────────────────────────────────────────

    if (item.id.startsWith(QLatin1String(ID_PREFIX_SNAP))) {
        QString filePath = item.id.mid(qstrlen(ID_PREFIX_SNAP));

        for (const StaleSnapRevision &rev : mConfig.snapRevisions) {
            if (rev.filePath == filePath) {
                result.bytesFreed = static_cast<qint64>(rev.size);
                if (!dryRun && mToolManager)
                    mToolManager->removeStaleSnapRevisions({rev});
                result.succeeded = true;
                return result;
            }
        }
        // Revision not found — treat as already gone
        result.succeeded = true;
        return result;
    }

    // ── All unused Flatpak runtimes ────────────────────────────────────────────

    if (item.id == QLatin1String(ID_FLATPAK_ALL)) {
        result.bytesFreed = item.estimatedSizeBytes;
        if (!dryRun && mToolManager)
            mToolManager->removeUnusedFlatpakRuntimes();
        result.succeeded = true;
        return result;
    }

    // ── Regular file / directory ───────────────────────────────────────────────

    {
        QString path = item.id;
        QFileInfo fi(path);

        if (dryRun) {
            // stat-only; no writes
            result.succeeded  = fi.exists();
            result.bytesFreed = static_cast<qint64>(FileUtil::getFileSize(path));
            if (!result.succeeded)
                result.error = QObject::tr("Path no longer exists: %1").arg(path);
        } else {
            // Route through CleanerService so exclusion enforcement and the
            // elevated-removal path (sudo rm -rf for system-owned files) are
            // preserved, matching what the pre-dialog batch systemClean() did.
            quint64 freed = mCleanerService
                ? mCleanerService->cleanFiles({path})
                : quint64(0);
            result.succeeded  = true;
            result.bytesFreed = static_cast<qint64>(freed);
        }
    }

    return result;
}
