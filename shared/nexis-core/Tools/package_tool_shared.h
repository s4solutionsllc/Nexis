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
    QString path;  // Full filesystem path (macOS .app bundles; empty on Linux)
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

    static QString friendlySectionName(const QString &section);

    PackageTools currentPackageTool = UNKNOWN;
};

#endif // PACKAGE_TOOL_SHARED_H
