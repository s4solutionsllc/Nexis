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

    virtual QList<Package> getInstalledApps() = 0;
    virtual bool trashApps(const QStringList &appPaths) = 0;

    virtual QList<StaleSnapRevision> getStaleSnapRevisions() = 0;
    virtual bool removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions) = 0;
    virtual QStringList getUnusedFlatpakRuntimes() = 0;
    virtual bool removeUnusedFlatpakRuntimes() = 0;
    virtual QList<OrphanPackage> getOrphanPackages() = 0;
    virtual bool removeOrphanPackages() = 0;

    static QList<StaleSnapRevision> parseSnapListAll(const QString &output);
    static QList<OrphanPackage> parseAptAutoremoveDryRun(const QString &output);
    static QList<OrphanPackage> parsePacmanOrphans(const QString &output);
    static QList<OrphanPackage> parseDnfAutoremoveDryRun(const QString &output);
    static QList<OrphanPackage> parseBrewAutoremoveDryRun(const QString &output);

    static QString friendlySectionName(const QString &section);

    PackageTools currentPackageTool = UNKNOWN;
};

#endif // PACKAGE_TOOL_SHARED_H
