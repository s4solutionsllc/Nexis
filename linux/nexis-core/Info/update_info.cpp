#include "update_info_linux.h"

#include <QRegularExpression>
#include <Utils/command_util.h>

UpdateCheckResult UpdateInfoLinux::checkForUpdates()
{
    UpdateCheckResult result;
    result.checkTime = QDateTime::currentDateTime();
    result.success = true;

    if (CommandUtil::isExecutable("apt-get"))
        checkApt(result);
    else if (CommandUtil::isExecutable("dnf"))
        checkDnf(result);
    else if (CommandUtil::isExecutable("pacman"))
        checkPacman(result);
    else if (CommandUtil::isExecutable("zypper"))
        checkZypper(result);

    if (CommandUtil::isExecutable("snap"))
        checkSnap(result);
    if (CommandUtil::isExecutable("flatpak"))
        checkFlatpak(result);

    result.totalCount = result.entries.size();
    return result;
}

QStringList UpdateInfoLinux::availableSources() const
{
    QStringList sources;
    if (CommandUtil::isExecutable("apt-get"))
        sources << "apt";
    else if (CommandUtil::isExecutable("dnf"))
        sources << "dnf";
    else if (CommandUtil::isExecutable("pacman"))
        sources << "pacman";
    else if (CommandUtil::isExecutable("zypper"))
        sources << "zypper";

    if (CommandUtil::isExecutable("snap"))
        sources << "snap";
    if (CommandUtil::isExecutable("flatpak"))
        sources << "flatpak";

    return sources;
}

void UpdateInfoLinux::checkApt(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus(
        "bash", {"-c", "LANG=C apt list --upgradable 2>/dev/null"}, 30000);

    if (r.exitCode != 0)
        return;

    const QStringList lines = r.output.split('\n');
    for (const QString &line : lines) {
        if (line.contains("upgradable")) {
            UpdateEntry entry;
            entry.source = "apt";
            int slashIdx = line.indexOf('/');
            entry.name = (slashIdx > 0) ? line.left(slashIdx) : line.trimmed();
            result.entries.append(entry);
        }
    }
}

void UpdateInfoLinux::checkDnf(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus("dnf", {"check-update", "--quiet"}, 30000);

    // dnf: exit 100 = updates available, 0 = none, 1 = error
    if (r.exitCode != 100)
        return;

    const QStringList lines = r.output.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        UpdateEntry entry;
        entry.source = "dnf";
        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        entry.name = parts.isEmpty() ? trimmed : parts.first();
        if (parts.size() >= 2)
            entry.version = parts.at(1);
        result.entries.append(entry);
    }
}

void UpdateInfoLinux::checkPacman(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus("pacman", {"-Qu"}, 30000);

    if (r.exitCode != 0)
        return;

    const QStringList lines = r.output.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        UpdateEntry entry;
        entry.source = "pacman";
        QStringList parts = trimmed.split(' ');
        entry.name = parts.isEmpty() ? trimmed : parts.first();
        if (parts.size() >= 4)
            entry.version = parts.at(3);
        result.entries.append(entry);
    }
}

void UpdateInfoLinux::checkZypper(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus("zypper", {"list-updates"}, 30000);

    if (r.exitCode != 0)
        return;

    const QStringList lines = r.output.split('\n');
    for (const QString &line : lines) {
        if (!line.startsWith("v |"))
            continue;
        QStringList cols = line.split('|');
        UpdateEntry entry;
        entry.source = "zypper";
        entry.name = (cols.size() >= 3) ? cols.at(2).trimmed() : line.trimmed();
        if (cols.size() >= 5)
            entry.version = cols.at(4).trimmed();
        result.entries.append(entry);
    }
}

void UpdateInfoLinux::checkSnap(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus(
        "bash", {"-c", "LANG=C snap refresh --list 2>/dev/null"}, 30000);

    if (r.exitCode != 0)
        return;

    const QStringList lines = r.output.split('\n');
    bool headerSkipped = false;
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        if (parts.size() < 2)
            continue;
        UpdateEntry entry;
        entry.source = "snap";
        entry.name = parts.first();
        entry.version = parts.at(1);
        result.entries.append(entry);
    }
}

void UpdateInfoLinux::checkFlatpak(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus(
        "bash",
        {"-c", "LANG=C flatpak remote-ls --updates --columns=application,version 2>/dev/null"},
        30000);

    if (r.exitCode != 0)
        return;

    const QStringList lines = r.output.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        UpdateEntry entry;
        entry.source = "flatpak";
        QStringList parts = trimmed.split('\t');
        entry.name = parts.isEmpty() ? trimmed : parts.first();
        if (parts.size() >= 2)
            entry.version = parts.at(1);
        result.entries.append(entry);
    }
}
