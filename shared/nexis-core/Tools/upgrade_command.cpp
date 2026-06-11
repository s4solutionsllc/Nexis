#include "upgrade_command.h"

namespace {

UpgradeCommand makeApt(const QString &name)
{
    UpgradeCommand c;
    c.program = "apt-get";
    c.args = name.isEmpty()
        ? QStringList{"-y", "dist-upgrade"}
        : QStringList{"-y", "install", "--only-upgrade", name};
    c.requiresSudo = true;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("apt (all)") : QStringLiteral("apt: %1").arg(name);
    return c;
}

UpgradeCommand makeDnf(const QString &name)
{
    UpgradeCommand c;
    c.program = "dnf";
    c.args = name.isEmpty()
        ? QStringList{"-y", "upgrade"}
        : QStringList{"-y", "upgrade", name};
    c.requiresSudo = true;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("dnf (all)") : QStringLiteral("dnf: %1").arg(name);
    return c;
}

UpgradeCommand makePacman(const QString &name)
{
    UpgradeCommand c;
    c.program = "pacman";
    c.args = name.isEmpty()
        ? QStringList{"-Syu", "--noconfirm"}
        : QStringList{"-S", "--noconfirm", name};
    c.requiresSudo = true;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("pacman (all)") : QStringLiteral("pacman: %1").arg(name);
    return c;
}

UpgradeCommand makeZypper(const QString &name)
{
    UpgradeCommand c;
    c.program = "zypper";
    c.args = name.isEmpty()
        ? QStringList{"--non-interactive", "update"}
        : QStringList{"--non-interactive", "update", name};
    c.requiresSudo = true;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("zypper (all)") : QStringLiteral("zypper: %1").arg(name);
    return c;
}

UpgradeCommand makeSnap(const QString &name)
{
    UpgradeCommand c;
    c.program = "snap";
    c.args = name.isEmpty()
        ? QStringList{"refresh"}
        : QStringList{"refresh", name};
    c.requiresSudo = true;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("snap (all)") : QStringLiteral("snap: %1").arg(name);
    return c;
}

UpgradeCommand makeFlatpak(const QString &name)
{
    // flatpak handles its own elevation via polkit when needed (system-scope
    // installs). Running through pkexec on top of that would prompt twice.
    UpgradeCommand c;
    c.program = "flatpak";
    c.args = name.isEmpty()
        ? QStringList{"update", "-y"}
        : QStringList{"update", "-y", name};
    c.requiresSudo = false;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("flatpak (all)") : QStringLiteral("flatpak: %1").arg(name);
    return c;
}

UpgradeCommand makeBrew(const QString &name)
{
    // Homebrew refuses to run as root and manages its own privilege prompts
    // (e.g. for casks). Always run as the invoking user.
    UpgradeCommand c;
    c.program = "brew";
    c.args = name.isEmpty()
        ? QStringList{"upgrade"}
        : QStringList{"upgrade", name};
    c.requiresSudo = false;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("brew (all)") : QStringLiteral("brew: %1").arg(name);
    return c;
}

UpgradeCommand makeSystem(const QString &name)
{
    // macOS softwareupdate. `-i <label>` for a single named update, `-ia` for
    // every recommended update. We deliberately omit `--restart` so the user
    // controls the reboot; the GUI surfaces an informational message instead.
    UpgradeCommand c;
    c.program = "softwareupdate";
    c.args = name.isEmpty()
        ? QStringList{"-ia"}
        : QStringList{"-i", name};
    c.requiresSudo = true;
    c.valid = true;
    c.label = name.isEmpty() ? QStringLiteral("system (all)") : QStringLiteral("system: %1").arg(name);
    return c;
}

UpgradeCommand dispatch(const QString &source, const QString &name)
{
    if (source == QLatin1String("apt"))      return makeApt(name);
    if (source == QLatin1String("dnf"))      return makeDnf(name);
    if (source == QLatin1String("pacman"))   return makePacman(name);
    if (source == QLatin1String("zypper"))   return makeZypper(name);
    if (source == QLatin1String("snap"))     return makeSnap(name);
    if (source == QLatin1String("flatpak"))  return makeFlatpak(name);
    if (source == QLatin1String("brew"))     return makeBrew(name);
    if (source == QLatin1String("system"))   return makeSystem(name);
    return UpgradeCommand{};  // valid = false
}

} // namespace

UpgradeCommand UpgradeCommandBuilder::build(const UpdateEntry &entry)
{
    if (entry.name.isEmpty())
        return UpgradeCommand{};
    return dispatch(entry.source, entry.name);
}

UpgradeCommand UpgradeCommandBuilder::buildAll(const QString &source)
{
    return dispatch(source, QString{});
}
