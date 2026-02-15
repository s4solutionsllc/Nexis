#ifndef PACKAGE_TOOL_SHARED_H
#define PACKAGE_TOOL_SHARED_H

#include <QString>
#include <QList>

struct Package {
    QString name;
    QString description;
    QString section;
    QString path;  // Full filesystem path (macOS .app bundles; empty on Linux)
};

enum PackageTools {
    APT,        // debian
    DNF,        // fedora
    YUM,        // fedora
    PACMAN,     // arch
    SNAP,       // snap
    HOMEBREW,   // macOS
    ZYPPER,     // opensuse
    UNKNOWN
};

#endif // PACKAGE_TOOL_SHARED_H
