#include "update_info_linux.h"

#include <QDebug>
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

void UpdateInfoLinux::parseAptLines(const QStringList &lines, UpdateCheckResult &result)
{
    // GH#524: the "[phased N%]" annotation here is not reliably present for
    // every package apt is actually phasing, so every upgradable candidate
    // is kept — checkApt() annotates phased status afterwards from
    // `apt-cache policy`'s Phased-Update-Percentage field instead.
    for (const QString &line : lines) {
        if (!line.contains("upgradable"))
            continue;
        UpdateEntry entry;
        entry.source = "apt";
        int slashIdx = line.indexOf('/');
        entry.name = (slashIdx > 0) ? line.left(slashIdx) : line.trimmed();
        result.entries.append(entry);
    }
}

QMap<QString, int> UpdateInfoLinux::parseAptCachePolicyPhased(const QString &output)
{
    QMap<QString, int> percentages;
    QString currentPackage;

    static const QString phasedPrefix = QStringLiteral("Phased-Update-Percentage:");

    const QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        // Package header lines start in column 0 and end with ':'; every
        // other line in a block ("Installed:", "Candidate:", the version
        // table) is indented.
        if (!line.isEmpty() && !line.at(0).isSpace() && line.endsWith(':')) {
            currentPackage = line.left(line.length() - 1);
            continue;
        }

        if (currentPackage.isEmpty() || percentages.contains(currentPackage))
            continue;

        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(phasedPrefix))
            continue;

        bool ok = false;
        const int percent = trimmed.mid(phasedPrefix.length()).trimmed().remove('%').toInt(&ok);
        if (ok)
            percentages.insert(currentPackage, percent);
    }

    return percentages;
}

void UpdateInfoLinux::checkApt(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus(
        "bash", {"-c", "LANG=C apt list --upgradable 2>/dev/null"}, 30000);

    if (r.exitCode != 0) {
        qWarning() << "update_info: apt upgradable check failed:" << r.error;
        return;
    }

    const int firstAptIdx = result.entries.size();
    parseAptLines(r.output.split('\n'), result);

    // Debian package names only ever use [A-Za-z0-9+.~-]; reject anything
    // else defensively before it's interpolated into the shell command below.
    static const QRegularExpression packageNameRe(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9+.~-]*$"));

    QStringList pkgNames;
    for (int i = firstAptIdx; i < result.entries.size(); ++i) {
        const QString &name = result.entries.at(i).name;
        if (packageNameRe.match(name).hasMatch())
            pkgNames << name;
    }
    if (pkgNames.isEmpty())
        return;

    ExecResult policy = CommandUtil::execWithStatus(
        "bash",
        {"-c", QStringLiteral("LANG=C apt-cache policy %1 2>/dev/null").arg(pkgNames.join(' '))},
        30000);

    if (policy.exitCode != 0) {
        qWarning() << "update_info: apt-cache policy check failed:" << policy.error;
        return;
    }

    const QMap<QString, int> phasedPercentages = parseAptCachePolicyPhased(policy.output);
    for (int i = firstAptIdx; i < result.entries.size(); ++i) {
        UpdateEntry &entry = result.entries[i];
        auto it = phasedPercentages.constFind(entry.name);
        if (it != phasedPercentages.constEnd() && it.value() < 100) {
            entry.isPhased = true;
            entry.phasedPercentage = it.value();
        }
    }
}

void UpdateInfoLinux::checkDnf(UpdateCheckResult &result) const
{
    ExecResult r = CommandUtil::execWithStatus("dnf", {"check-update", "--quiet"}, 30000);

    // dnf: exit 100 = updates available, 0 = none, 1 = error
    if (r.exitCode != 100) {
        if (r.exitCode != 0)
            qWarning() << "update_info: dnf check-update failed:" << r.error;
        return;
    }

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

    // pacman -Qu exits non-zero both on a genuine error and on "nothing to
    // upgrade" — log at debug so real failures are still traceable without
    // warning on the common empty-list case.
    if (r.exitCode != 0) {
        if (!r.error.isEmpty())
            qDebug() << "update_info: pacman -Qu exited" << r.exitCode << ":" << r.error;
        return;
    }

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

    if (r.exitCode != 0) {
        if (!r.error.isEmpty())
            qDebug() << "update_info: zypper list-updates exited" << r.exitCode << ":" << r.error;
        return;
    }

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

    if (r.exitCode != 0) {
        if (!r.error.isEmpty())
            qDebug() << "update_info: snap refresh --list exited" << r.exitCode << ":" << r.error;
        return;
    }

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

    if (r.exitCode != 0) {
        if (!r.error.isEmpty())
            qDebug() << "update_info: flatpak remote-ls --updates exited" << r.exitCode << ":" << r.error;
        return;
    }

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
