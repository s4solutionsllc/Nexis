#ifndef PACKAGE_TOOL_SHARED_H
#define PACKAGE_TOOL_SHARED_H

#include <QString>
#include <QList>
#include <QFileInfoList>

#include "nexis-core_global.h"

struct Package {
    QString name;
    QString description;
    QString section;
    QString path;       // Full filesystem path (macOS .app bundles; empty on Linux)
    QString bundleId;   // macOS CFBundleIdentifier (empty on Linux / when unknown)
};

struct StaleSnapRevision {
    QString name;       // snap name (e.g. "firefox")
    QString revision;   // revision number (e.g. "4173")
    QString filePath;   // full path to .snap file (e.g. "/var/lib/snapd/snaps/firefox_4173.snap")
    quint64 size = 0;   // file size in bytes
};

struct OrphanPackage {
    QString name;
    QString description;
    quint64 size = 0;        // installed size in bytes (0 if unavailable)
    bool autoInstalled = false;
    int reverseDepsCount = -1; // -1 = unknown (non-APT systems)
};

// FW-07 (SSO-3735): APT 3.1 added dnf-style transaction history
// (apt history-list / history-info / history-undo / history-rollback) plus
// apt why / why-not. We model the history-list summary plus the per-id detail
// returned by history-info, and use AptVersion to gate UI surfaces on the
// 3.1+ version where these commands first ship.
struct AptHistoryEntry {
    int id = 0;                // numeric transaction id (history-undo argument)
    QString dateTime;          // raw "YYYY-MM-DD HH:MM:SS" or whatever apt printed
    QString operation;         // install / upgrade / remove / purge / autoremove …
    QString commandLine;       // requesting command line (may be empty in summary)
    QString user;              // requesting user (may be empty in summary)
    QStringList packages;      // populated by history-info, empty in summary view
};

struct AptVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool valid = false;
    bool atLeast(int M, int m, int p = 0) const {
        if (!valid) return false;
        if (major != M) return major > M;
        if (minor != m) return minor > m;
        return patch >= p;
    }
};

// FW-18 (SSO-3746): a single leftover artifact discovered for a macOS app
// uninstall, matched by bundle id under the standard ~/Library locations.
struct AppLeftover {
    QString path;       // full filesystem path to the leftover artifact
    QString category;   // human-readable label: "Application Support", "Caches", etc.
    quint64 size = 0;   // size in bytes (0 when size could not be determined)
};

enum PackageTools {
    APT,        // debian
    APT_RPM,    // ALT Linux, PCLinuxOS, Vine Linux (apt-get + rpm)
    DNF,        // fedora
    YUM,        // fedora
    PACMAN,     // arch
    SNAP,       // snap
    HOMEBREW,   // macOS
    ZYPPER,     // opensuse
    UNKNOWN
};

class NEXISCORESHARED_EXPORT PackageTool
{
public:
    virtual ~PackageTool() = default;

    virtual QList<Package> getPackages() = 0;
    virtual QFileInfoList getPackageCaches() = 0;
    virtual void uninstallPackages(const QStringList &packages, bool purge = false) = 0;
    virtual QStringList dryRunRemovePackages(const QStringList &packages) = 0;

    virtual QStringList getSnapPackages() = 0;
    virtual bool uninstallSnapPackages(const QStringList &packages) = 0;

    // SSO-15385: enumerate/uninstall installed Flatpak apps (distinct from
    // getUnusedFlatpakRuntimes(), which cleans up unreferenced runtimes).
    // Returned refs are Flatpak application ids (e.g. "org.mozilla.firefox").
    virtual QStringList getFlatpakPackages() = 0;
    virtual bool uninstallFlatpakPackages(const QStringList &refs) = 0;

    virtual QList<Package> getInstalledApps() = 0;
    virtual bool trashApps(const QStringList &appPaths) = 0;

    virtual QList<StaleSnapRevision> getStaleSnapRevisions() = 0;
    virtual bool removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions) = 0;
    virtual QStringList getUnusedFlatpakRuntimes() = 0;
    virtual bool removeUnusedFlatpakRuntimes() = 0;
    virtual QList<OrphanPackage> getOrphanPackages() = 0;
    virtual bool removeOrphanPackages() = 0;

    // FW-18: scan standard macOS ~/Library locations for leftover artifacts
    // belonging to `app` (matched by bundle id — never by loose app name).
    // Returns an empty list on non-macOS platforms.
    virtual QList<AppLeftover> findAppLeftovers(const Package &app) { Q_UNUSED(app); return {}; }
    // Move each path in `paths` to the macOS Trash via QFile::moveToTrash.
    // Returns true iff every path was trashed successfully.
    virtual bool trashLeftovers(const QStringList &paths) { Q_UNUSED(paths); return false; }

    static QList<StaleSnapRevision> parseSnapListAll(const QString &output);
    static QList<OrphanPackage> parseAptAutoremoveDryRun(const QString &output);
    static QList<OrphanPackage> parsePacmanOrphans(const QString &output);
    static QList<OrphanPackage> parseDnfAutoremoveDryRun(const QString &output);
    static QList<OrphanPackage> parseBrewAutoremoveDryRun(const QString &output);

    // FW-07 (SSO-3735): APT 3.1 history + why parsers + version gate.
    // Pure functions over the textual command output so we can fixture-test
    // them without an APT 3.1 box on the build agent.
    static AptVersion parseAptVersion(const QString &output);
    static QList<AptHistoryEntry> parseAptHistoryList(const QString &output);
    static AptHistoryEntry parseAptHistoryInfo(const QString &output);
    static QStringList parseAptWhy(const QString &output);

    static QString friendlySectionName(const QString &section);

    PackageTools currentPackageTool = UNKNOWN;

protected:
    // WI-33: command-execution seam. Production code calls these so the
    // uninstall paths funnel through one place; tests subclass the platform
    // tool and override these to capture (cmd, args) instead of actually
    // shelling out. Mirrors the TestableRepairEngine pattern in
    // tests/core/test_repo_repair_engine.cpp.
    virtual bool runSudoCommand(const QString &cmd, const QStringList &args);
    virtual QString runCommand(const QString &cmd,
                               const QStringList &args,
                               int timeoutMs = 30000);
};

#endif // PACKAGE_TOOL_SHARED_H
