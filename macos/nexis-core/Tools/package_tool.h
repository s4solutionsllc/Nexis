#ifndef PACKAGE_TOOL_H
#define PACKAGE_TOOL_H

#include "package_tool_shared.h"
#include "nexis-core_global.h"

#include "Utils/command_util.h"
#include "Utils/file_util.h"

class NEXISCORESHARED_EXPORT PackageTool
{
public:
    // Homebrew
    static QFileInfoList getHomebrewCaches();
    static QList<Package> getHomebrewPackages();
    static bool homebrewRemovePackages(QStringList packages);
    static QStringList homebrewDryRunRemove(const QStringList &packages);

    // macOS native .app bundles
    static QList<Package> getInstalledApps();
    static bool trashApps(const QStringList &appPaths);

    static QString friendlySectionName(const QString &section);

    static const PackageTools currentPackageTool;
};

#endif // PACKAGE_TOOL_H
