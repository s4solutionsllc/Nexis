#ifndef UPGRADE_COMMAND_H
#define UPGRADE_COMMAND_H

#include <QString>
#include <QStringList>

#include <Info/update_info.h>

#include "nexis-core_global.h"

// SSO-3741 (FW-13): pure command-construction layer for the upgrade actions
// surfaced from the Available Updates UI. Kept fully data-only so unit tests
// can assert the exact argv that the GUI would pass to CommandUtil.
//
// Mapping rationale, per `backlog/SSO-3741_research.md`:
//   - apt/dnf/pacman/zypper/snap/softwareupdate: require root
//   - brew, flatpak: rely on the tool's own privilege handling and do NOT
//     elevate via pkexec/osascript (matches the CLI ergonomics users expect)
struct NEXISCORESHARED_EXPORT UpgradeCommand {
    QString program;        // e.g. "apt-get"
    QStringList args;       // already includes -y / --noconfirm where needed
    bool requiresSudo = false;
    bool valid = false;     // false → unknown source / nothing to do
    QString label;          // human-readable label used in progress signals
};

class NEXISCORESHARED_EXPORT UpgradeCommandBuilder
{
public:
    // Build the upgrade command for a single update entry.
    // entry.source must be one of: apt, dnf, pacman, zypper, snap, flatpak,
    //   brew, system. entry.name is required.
    static UpgradeCommand build(const UpdateEntry &entry);

    // Build the "upgrade all" command for an entire source.
    static UpgradeCommand buildAll(const QString &source);
};

#endif // UPGRADE_COMMAND_H
