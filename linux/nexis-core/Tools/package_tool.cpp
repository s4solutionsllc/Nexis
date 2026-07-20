#include "package_tool_linux.h"

#include "Tools/leftover_deny_list.h"
#include "Tools/leftover_scanner_linux.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>

PackageToolLinux::PackageToolLinux()
{
    currentPackageTool =
        (CommandUtil::isExecutable("apt-get") && CommandUtil::isExecutable("rpm")
            && !CommandUtil::isExecutable("dpkg")) ? APT_RPM :
        CommandUtil::isExecutable("apt-get") ? APT :
        CommandUtil::isExecutable("dnf")     ? DNF :
        CommandUtil::isExecutable("yum")     ? YUM :
        CommandUtil::isExecutable("pacman")  ? PACMAN :
        CommandUtil::isExecutable("zypper")  ? ZYPPER :
                                               UNKNOWN;
}

QList<Package> PackageToolLinux::getPackages()
{
    switch (currentPackageTool) {
    case APT:
        return getDpkgPackages();
    case APT_RPM:
    case YUM:
    case DNF:
        return getRpmPackages();
    case PACMAN:
        return getPacmanPackages();
    default:
        return {};
    }
}

QFileInfoList PackageToolLinux::getPackageCaches()
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return getDpkgPackageCaches();
    case YUM:
    case DNF:
        return getYumDnfPackageCaches();
    case PACMAN:
        return getPacmanPackageCaches();
    default:
        return {};
    }
}

void PackageToolLinux::uninstallPackages(const QStringList &packages, bool purge)
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        dpkgRemovePackages(packages, purge);
        break;
    case YUM:
        yumRemovePackages(packages);
        break;
    case DNF:
        dnfRemovePackages(packages);
        break;
    case PACMAN:
        pacmanRemovePackages(packages);
        break;
    default:
        break;
    }
}

QStringList PackageToolLinux::dryRunRemovePackages(const QStringList &packages)
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return dpkgDryRunRemove(packages);
    case YUM:
    case DNF:
        return rpmDryRunRemove(packages);
    case PACMAN:
        return pacmanDryRunRemove(packages);
    default:
        return {};
    }
}

QStringList PackageToolLinux::getSnapPackages()
{
    QStringList packageList = {};

    if (CommandUtil::isExecutable("snap")) {
        ExecResult result = CommandUtil::execWithStatus("snap", {"list"});
        if (!result.ok()) {
            qCritical() << result.error;
            return packageList;
        }

        packageList = result.output.trimmed().split('\n');
        packageList.removeFirst();

        for (int i = 0; i < packageList.count(); ++i)
            packageList[i] = packageList.at(i).split(QRegularExpression("\\s+")).first();
    }

    return packageList;
}

bool PackageToolLinux::uninstallSnapPackages(const QStringList &packages)
{
    QStringList args = packages;
    args.insert(0, "remove");
    qDebug() << args;
    return runSudoCommand("snap", args);
}

QList<Package> PackageToolLinux::getInstalledApps()
{
    return {};
}

bool PackageToolLinux::trashApps(const QStringList &)
{
    return false;
}

/***********
 * DPKG
 ***********/

QFileInfoList PackageToolLinux::getDpkgPackageCaches()
{
    QDir caches("/var/cache/apt/archives/");
    return caches.entryInfoList(QDir::Files);
}

QFileInfoList PackageToolLinux::getYumDnfPackageCaches()
{
    QFileInfoList caches;

    QDir dnfCache("/var/cache/dnf/");
    if (dnfCache.exists())
        caches.append(dnfCache.entryInfoList(QDir::Files, QDir::Size));

    QDir yumCache("/var/cache/yum/");
    if (yumCache.exists())
        caches.append(yumCache.entryInfoList(QDir::Files, QDir::Size));

    return caches;
}

QList<Package> PackageToolLinux::getDpkgPackages()
{
    QList<Package> packages;

    ExecResult result = CommandUtil::execWithStatus("bash", {"-c",
        "dpkg-query -W -f '${Package}\\t${Section}\\t${binary:Summary}\\n' 2> /dev/null"}, 60000);
    if (!result.ok()) {
        qCritical() << result.error;
        return packages;
    }

    const QStringList lines = result.output.trimmed().split('\n');
    for (const QString &line : lines) {
        QStringList parts = line.split('\t');
        if (parts.size() < 3)
            continue;

        Package pkg;
        pkg.name = parts.at(0).trimmed();
        pkg.section = parts.at(1).trimmed();
        pkg.description = parts.at(2).trimmed();

        if (!pkg.name.isEmpty())
            packages.append(pkg);
    }

    return packages;
}

bool PackageToolLinux::dpkgRemovePackages(QStringList packages, bool purge)
{
    packages.insert(0, purge ? "purge" : "remove");
    packages.insert(1, "-y");
    return runSudoCommand("apt-get", packages);
}

/**********
 * RPM
 **********/
QList<Package> PackageToolLinux::getRpmPackages()
{
    QList<Package> packages;

    ExecResult result = CommandUtil::execWithStatus("bash", {"-c",
        "rpm -qa --queryformat '%{NAME}\\t%{GROUP}\\t%{SUMMARY}\\n' 2> /dev/null"}, 60000);
    if (!result.ok()) {
        qCritical() << result.error;
        return packages;
    }

    const QStringList lines = result.output.trimmed().split('\n');
    for (const QString &line : lines) {
        QStringList parts = line.split('\t');
        if (parts.size() < 3)
            continue;

        Package pkg;
        pkg.name = parts.at(0).trimmed();
        pkg.section = parts.at(1).trimmed();
        pkg.description = parts.at(2).trimmed();

        if (!pkg.name.isEmpty())
            packages.append(pkg);
    }

    return packages;
}

bool PackageToolLinux::dnfRemovePackages(QStringList packages)
{
    packages.insert(0, "remove");
    packages.insert(1, "-y");
    return runSudoCommand("dnf", packages);
}

bool PackageToolLinux::yumRemovePackages(QStringList packages)
{
    packages.insert(0, "remove");
    packages.insert(1, "-y");
    return runSudoCommand("yum", packages);
}

/**********
 * PACMAN
 **********/
QFileInfoList PackageToolLinux::getPacmanPackageCaches()
{
    QDir caches("/var/cache/pacman/pkg/");

    return caches.entryInfoList(QDir::Files);
}

QList<Package> PackageToolLinux::getPacmanPackages()
{
    QList<Package> packages;

    ExecResult result = CommandUtil::execWithStatus("bash", {"-c", "pacman -Qi 2> /dev/null"}, 60000);
    if (!result.ok()) {
        qCritical() << result.error;
        return packages;
    }

    const QStringList lines = result.output.trimmed().split('\n');
    Package pkg;
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            if (!pkg.name.isEmpty())
                packages.append(pkg);
            pkg = Package();
            continue;
        }
        int colonPos = line.indexOf(':');
        if (colonPos < 0)
            continue;
        QString key = line.left(colonPos).trimmed();
        QString val = line.mid(colonPos + 1).trimmed();
        if (key == "Name")
            pkg.name = val;
        else if (key == "Description")
            pkg.description = val;
        else if (key == "Groups")
            pkg.section = (val == "None") ? "misc" : val;
    }
    if (!pkg.name.isEmpty())
        packages.append(pkg);

    return packages;
}

bool PackageToolLinux::pacmanRemovePackages(QStringList packages)
{
    packages.push_back("--noconfirm");
    packages.push_back("-R");
    return runSudoCommand("pacman", packages);
}

/**********
 * DRY-RUN
 **********/
QStringList PackageToolLinux::dpkgDryRunRemove(const QStringList &packages)
{
    QStringList wouldRemove;

    // SSO-3399: invoke apt-get directly with argv to avoid shell interpolation
    // of package names; merge stderr because apt prints status lines to both
    // streams depending on locale/config.
    QStringList args = {"remove", "--dry-run", "--"};
    args.append(packages);
    ExecResult result = CommandUtil::execWithStatus("apt-get", args);
    const QString combined = result.output + QLatin1Char('\n') + result.error;

    static QRegularExpression re("^Remv\\s+(\\S+)");
    const QStringList lines = combined.split('\n');
    for (const QString &line : lines) {
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
            wouldRemove << match.captured(1);
    }
    return wouldRemove;
}

QStringList PackageToolLinux::rpmDryRunRemove(const QStringList &packages)
{
    QStringList wouldRemove;

    // dnf --assumeno auto-declines the transaction, which makes dnf exit
    // non-zero even though the summary we need was printed to stdout —
    // branching on ok() here would silently break the feature, so parse the
    // output unconditionally like the pre-migration exec() call did.
    QStringList args = packages;
    args.insert(0, "remove");
    args.insert(1, "--assumeno");
    ExecResult result = CommandUtil::execWithStatus("dnf", args);
    if (!result.ok())
        qDebug() << "dnf --assumeno exited non-zero (expected for an aborted dry-run):" << result.error;

    bool inRemoveSection = false;
    const QStringList lines = result.output.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("Removing:") || trimmed.startsWith("Removing dependent packages:"))
            inRemoveSection = true;
        else if (trimmed.startsWith("Transaction Summary") || trimmed.isEmpty())
            inRemoveSection = false;
        else if (inRemoveSection) {
            QString name = trimmed.split(QRegularExpression("\\s+")).first();
            if (!name.isEmpty())
                wouldRemove << name;
        }
    }
    return wouldRemove;
}

QStringList PackageToolLinux::pacmanDryRunRemove(const QStringList &packages)
{
    QStringList wouldRemove;

    QStringList args = packages;
    args.insert(0, "-Rs");
    args.insert(1, "--print");
    ExecResult result = CommandUtil::execWithStatus("pacman", args);

    const QStringList lines = result.output.trimmed().split('\n');
    for (const QString &line : lines) {
        QString name = line.section('/', -1).section('-', 0, 0);
        if (!name.isEmpty())
            wouldRemove << name;
    }
    return wouldRemove;
}

/**********
 * STALE SNAP REVISIONS (FR-79)
 **********/
QList<StaleSnapRevision> PackageToolLinux::getStaleSnapRevisions()
{
    if (!CommandUtil::isExecutable("snap"))
        return {};

    ExecResult result = CommandUtil::execWithStatus("snap", {"list", "--all"});
    if (!result.ok()) {
        qCritical() << "Failed to get stale snap revisions:" << result.error;
        return {};
    }

    QList<StaleSnapRevision> revisions = PackageTool::parseSnapListAll(result.output.trimmed());

    for (int i = 0; i < revisions.size(); ++i) {
        QFileInfo fi(revisions[i].filePath);
        if (fi.exists())
            revisions[i].size = fi.size();
    }

    return revisions;
}

bool PackageToolLinux::removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions)
{
    bool allOk = true;
    for (const StaleSnapRevision &rev : revisions) {
        if (!runSudoCommand("snap", {"remove", rev.name, "--revision=" + rev.revision})) {
            qCritical() << "Failed to remove snap revision:" << rev.name << rev.revision;
            allOk = false;
        }
    }
    return allOk;
}

/**********
 * UNUSED FLATPAK RUNTIMES (FR-79)
 **********/
QStringList PackageToolLinux::getUnusedFlatpakRuntimes()
{
    if (!CommandUtil::isExecutable("flatpak"))
        return {};

    ExecResult result = CommandUtil::execWithStatus("flatpak", {"uninstall", "--unused",
                                                    "--noninteractive"}, 30000);
    if (!result.ok()) {
        qCritical() << "Failed to get unused flatpak runtimes:" << result.error;
        return {};
    }

    const QString output = result.output.trimmed();
    if (output.isEmpty())
        return {};

    QStringList refs;
    const QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("ID") || trimmed.contains("---"))
            continue;
        // Lines may start with "1. " numbering or just be ref IDs
        static const QRegularExpression refRe(R"((?:\d+\.\s+)?(\S+))");
        QRegularExpressionMatch match = refRe.match(trimmed);
        if (match.hasMatch()) {
            QString ref = match.captured(1);
            if (ref.contains('.'))
                refs.append(ref);
        }
    }
    return refs;
}

bool PackageToolLinux::removeUnusedFlatpakRuntimes()
{
    if (!CommandUtil::isExecutable("flatpak"))
        return false;

    ExecResult result = CommandUtil::execWithStatus("flatpak", {"uninstall", "--unused", "-y", "--noninteractive"}, 120000);
    if (!result.ok()) {
        qCritical() << "Failed to remove unused flatpak runtimes:" << result.error;
        return false;
    }
    return true;
}

/**********
 * ORPHAN PACKAGES (FR-80)
 **********/
QList<OrphanPackage> PackageToolLinux::getOrphanPackages()
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return getAptOrphans();
    case DNF:
    case YUM:
        return getDnfOrphans();
    case PACMAN:
        return getPacmanOrphans();
    default:
        return {};
    }
}

bool PackageToolLinux::removeOrphanPackages()
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return runSudoCommand("apt-get", {"autoremove", "-y"});
    case DNF:
        return runSudoCommand("dnf", {"autoremove", "-y"});
    case YUM:
        return runSudoCommand("yum", {"autoremove", "-y"});
    case PACMAN: {
        const QString output = runCommand("pacman", {"-Qdtq"}).trimmed();
        if (output.isEmpty())
            return true;
        QStringList args = {"-Rns", "--noconfirm"};
        args.append(output.split('\n'));
        return runSudoCommand("pacman", args);
    }
    default:
        return false;
    }
}

QList<OrphanPackage> PackageToolLinux::getAptOrphans()
{
    // apt-get autoremove --dry-run always exits 0, so its output can be
    // parsed unconditionally; still log if the process itself failed to run.
    ExecResult result = CommandUtil::execWithStatus("bash", {"-c", "LANG=C apt-get autoremove --dry-run 2>&1"});
    if (!result.ok())
        qCritical() << "Failed to get apt orphans:" << result.error;

    QList<OrphanPackage> orphans = PackageTool::parseAptAutoremoveDryRun(result.output.trimmed());
    if (orphans.isEmpty())
        return orphans;

    // Bulk auto-install flag — one call for all packages. Silently skip on
    // failure, same as the pre-migration catch(...) fallback.
    ExecResult autoResult = CommandUtil::execWithStatus("bash", {"-c", "LANG=C apt-mark showauto 2>/dev/null"});
    if (autoResult.ok()) {
        QSet<QString> autoSet;
        for (const QString &line : autoResult.output.trimmed().split('\n')) {
            QString pkg = line.trimmed();
            if (!pkg.isEmpty())
                autoSet.insert(pkg);
        }
        for (OrphanPackage &pkg : orphans)
            pkg.autoInstalled = autoSet.contains(pkg.name);
    }

    // Per-package reverse dependency count
    for (OrphanPackage &pkg : orphans) {
        ExecResult rdResult = CommandUtil::execWithStatus(
            "bash",
            {"-c", QString("LANG=C apt-cache rdepends --installed %1 2>/dev/null").arg(pkg.name)}
        );
        if (!rdResult.ok()) {
            pkg.reverseDepsCount = 0;
            continue;
        }

        int count = 0;
        bool inSection = false;
        for (const QString &line : rdResult.output.trimmed().split('\n')) {
            if (line.trimmed() == QLatin1String("Reverse Depends:")) {
                inSection = true;
                continue;
            }
            if (inSection && line.startsWith(QLatin1String("  ")) && !line.trimmed().isEmpty())
                ++count;
        }
        pkg.reverseDepsCount = count;
    }

    return orphans;
}

QList<OrphanPackage> PackageToolLinux::getDnfOrphans()
{
    // dnf --assumeno auto-declines and exits non-zero even when the
    // transaction summary we need was printed — parse unconditionally.
    ExecResult result = CommandUtil::execWithStatus("dnf", {"autoremove", "--assumeno"});
    if (!result.ok())
        qDebug() << "dnf --assumeno exited non-zero (expected for an aborted dry-run):" << result.error;
    return PackageTool::parseDnfAutoremoveDryRun(result.output.trimmed());
}

QList<OrphanPackage> PackageToolLinux::getPacmanOrphans()
{
    // pacman -Qdtq returns non-zero if no orphans found — that's an empty
    // result, not an error, so log at debug level and return {} either way.
    ExecResult result = CommandUtil::execWithStatus("pacman", {"-Qdtq"});
    if (!result.ok()) {
        qDebug() << "No pacman orphans or error:" << result.error;
        return {};
    }
    return PackageTool::parsePacmanOrphans(result.output.trimmed());
}

/**********
 * APT 3.1 TRANSACTION HISTORY (FW-07 / SSO-3735)
 **********/

// All live commands flow through the runCommand / runSudoCommand seam on
// PackageTool so the platform-tool test subclass can capture argv without
// shelling out. Production callers should always check aptHistorySupported()
// first — the UI is hidden on apt < 3.1 (parseAptVersion would return invalid
// or below 3.1.0) and these methods otherwise just return empty results.

AptVersion PackageToolLinux::aptVersion()
{
    if (currentPackageTool != APT && currentPackageTool != APT_RPM)
        return AptVersion{};
    const QString out = runCommand("apt", {"--version"});
    return PackageTool::parseAptVersion(out);
}

bool PackageToolLinux::aptHistorySupported()
{
    return aptVersion().atLeast(3, 1, 0);
}

QList<AptHistoryEntry> PackageToolLinux::getAptHistory()
{
    if (!aptHistorySupported())
        return {};
    const QString out = runCommand("apt", {"history-list"});
    return PackageTool::parseAptHistoryList(out);
}

AptHistoryEntry PackageToolLinux::getAptHistoryInfo(int transactionId)
{
    if (!aptHistorySupported() || transactionId <= 0)
        return AptHistoryEntry{};
    const QString out = runCommand("apt", {"history-info", QString::number(transactionId)});
    return PackageTool::parseAptHistoryInfo(out);
}

QStringList PackageToolLinux::aptWhy(const QString &package, bool whyNot)
{
    if (!aptHistorySupported() || package.isEmpty())
        return {};
    // Use argv form to avoid any shell interpolation of the package name.
    const QString out = runCommand("apt", {whyNot ? "why-not" : "why", package});
    return PackageTool::parseAptWhy(out);
}

bool PackageToolLinux::aptHistoryUndo(int transactionId)
{
    if (!aptHistorySupported() || transactionId <= 0)
        return false;
    return runSudoCommand("apt", {"history-undo", "-y", QString::number(transactionId)});
}

bool PackageToolLinux::aptHistoryRedo(int transactionId)
{
    if (!aptHistorySupported() || transactionId <= 0)
        return false;
    return runSudoCommand("apt", {"history-redo", "-y", QString::number(transactionId)});
}

bool PackageToolLinux::aptHistoryRollback(int transactionId)
{
    if (!aptHistorySupported() || transactionId <= 0)
        return false;
    return runSudoCommand("apt", {"history-rollback", "-y", QString::number(transactionId)});
}

// friendlySectionName() is in shared/nexis-core/Tools/package_tool_shared.cpp

// SSO-15385: Linux leftover scanner implementation.

QList<AppLeftover> PackageToolLinux::findAppLeftovers(const Package &app)
{
    // Build the search name set: package name plus any reverse-DNS app id
    // fragments we can derive. For Flatpak the name is usually the full
    // reverse-DNS id (e.g. "org.mozilla.Firefox"); for dpkg/rpm it is the
    // short package name (e.g. "firefox"). We include both forms so the
    // scanner can match directory names in either convention.
    QStringList names;
    if (!app.name.isEmpty())
        names.append(app.name);
    // Derive last-segment shortname from reverse-DNS ids
    // (e.g. "org.mozilla.Firefox" → "Firefox").
    if (app.name.contains(QLatin1Char('.')) && !app.name.startsWith(QLatin1Char('.'))) {
        names.append(app.name.section(QLatin1Char('.'), -1));
    }
    names.removeDuplicates();

    const auto candidates = LeftoverScannerLinux::scanLeftovers(names);

    QList<AppLeftover> out;
    out.reserve(candidates.size());
    for (const auto &c : candidates) {
        AppLeftover lf;
        lf.path     = c.path;
        lf.category = c.category;
        lf.size     = c.sizeBytes;
        out.append(lf);
    }
    return out;
}

bool PackageToolLinux::trashLeftovers(const QStringList &paths)
{
    const QString batchId    = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString nexisVer   = QCoreApplication::applicationVersion();
    bool allOk = true;

    for (const QString &raw : paths) {
        const QFileInfo fi(raw);
        const QString canonical = fi.canonicalFilePath();

        // CISO §2: hard deny-list check on canonicalized path.
        if (canonical.isEmpty() || LeftoverDenyList::isDenied(canonical)) {
            qWarning() << "trashLeftovers: deny-list blocked" << raw;
            allOk = false;
            continue;
        }

        const quint64 size = static_cast<quint64>(fi.size());

        // CISO §3: log before the action so the record exists even on failure.
        LeftoverDenyList::AuditEntry entry;
        entry.batchId       = batchId;
        entry.originalPath  = raw;
        entry.canonicalPath = canonical;
        entry.action        = QStringLiteral("trash");
        entry.matchRule     = QStringLiteral("user-selected");
        entry.sizeBytes     = size;
        entry.nexisVersion  = nexisVer;
        LeftoverDenyList::logDeletion(entry);

        // QFile::moveToTrash() uses the freedesktop.org Trash spec on Linux.
        // CISO §1: fail securely — never silently fall back to unlink.
        QString trashDest;
        if (!QFile::moveToTrash(raw, &trashDest)) {
            qWarning() << "trashLeftovers: moveToTrash failed for" << raw;
            allOk = false;
        } else {
            // Update the log entry with the trash destination for the audit trail.
            entry.trashDest = trashDest;
            LeftoverDenyList::logDeletion(entry);
        }
    }

    return allOk;
}
