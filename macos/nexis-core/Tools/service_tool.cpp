#include "service_tool_macos.h"
#include "Utils/command_util.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>

#include <unistd.h>

// Service constructor is in shared/nexis-core/Tools/service_tool_shared.cpp

// macOS uses launchd (launchctl) instead of systemd (systemctl). Jobs are
// defined as plist files in:
//   ~/Library/LaunchAgents/       (per-user agents)       -> gui/<uid>
//   /Library/LaunchAgents/        (agents for every user) -> gui/<uid>
//   /Library/LaunchDaemons/       (system daemons, root)  -> system

namespace {

// An admin prompt can sit unanswered far longer than the 30 s default.
const int kElevatedTimeoutMs = 5 * 60 * 1000;

}

QHash<QString, bool> ServiceToolMacOS::parseLaunchctlList(const QString &output)
{
    QHash<QString, bool> jobs;
    static const QRegularExpression sep(QStringLiteral("\\s+"));
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.trimmed().split(sep);
        if (parts.size() < 3 || parts.at(0) == QLatin1String("PID"))
            continue;
        const QString &pid = parts.at(0);
        jobs.insert(parts.at(2), pid != QLatin1String("-") && pid != QLatin1String("0"));
    }
    return jobs;
}

QHash<QString, bool> ServiceToolMacOS::parsePrintDisabled(const QString &output)
{
    // Current macOS prints `"label" => disabled|enabled`; older releases
    // printed `"label" => true|false` (true meaning disabled).
    QHash<QString, bool> overrides;
    static const QRegularExpression lineRx(
        QStringLiteral("\"([^\"]+)\"\\s*=>\\s*(disabled|enabled|true|false)"));
    QRegularExpressionMatchIterator it = lineRx.globalMatch(output);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString state = match.captured(2);
        overrides.insert(match.captured(1), state == QLatin1String("disabled") || state == QLatin1String("true"));
    }
    return overrides;
}

bool ServiceToolMacOS::isEnabled(const Job &job, const QHash<QString, bool> &overrides)
{
    const auto it = overrides.constFind(job.label);
    return it != overrides.constEnd() ? !it.value() : !job.disabledInPlist;
}

bool ServiceToolMacOS::parsePrintIsRunning(const QString &output)
{
    // Only the job's own top-level "state" line; nested endpoints print
    // "state = active" at a deeper indent.
    static const QRegularExpression stateRx(QStringLiteral("^\\tstate = (.+)$"),
                                            QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = stateRx.match(output);
    return match.hasMatch() && match.captured(1).trimmed() == QLatin1String("running");
}

bool ServiceToolMacOS::isHiddenLabel(const QString &label)
{
    return label.isEmpty()
        || label.startsWith(QLatin1String("com.apple."))
        || label.startsWith(QLatin1String("application."))
        || label.startsWith(QLatin1Char('['));
}

QList<ServiceToolMacOS::Job> ServiceToolMacOS::jobsInDirectory(const QString &dir, Domain domain)
{
    QList<Job> jobs;
    const QFileInfoList files = QDir(dir, QStringLiteral("*.plist")).entryInfoList(QDir::Files);
    for (const QFileInfo &file : files) {
        // NativeFormat reads both XML and binary plists.
        const QSettings plist(file.absoluteFilePath(), QSettings::NativeFormat);
        Job job;
        job.label = plist.value(QStringLiteral("Label")).toString();
        if (isHiddenLabel(job.label))
            continue;
        job.plistPath = file.absoluteFilePath();
        job.domain = domain;
        job.disabledInPlist = plist.value(QStringLiteral("Disabled")).toBool();
        job.program = plist.value(QStringLiteral("Program")).toString();
        if (job.program.isEmpty())
            job.program = plist.value(QStringLiteral("ProgramArguments")).toStringList().value(0);
        jobs.append(job);
    }
    return jobs;
}

QString ServiceToolMacOS::domainTarget(Domain domain, uint uid)
{
    return domain == Domain::SystemDaemon ? QStringLiteral("system")
                                          : QStringLiteral("gui/%1").arg(uid);
}

ServiceToolMacOS::Command ServiceToolMacOS::enableCommand(const Job &job, bool enable, uint uid)
{
    Command command;
    command.elevated = job.domain == Domain::SystemDaemon;
    command.args = {enable ? QStringLiteral("enable") : QStringLiteral("disable"),
                    domainTarget(job.domain, uid) + QLatin1Char('/') + job.label};
    return command;
}

ServiceToolMacOS::Command ServiceToolMacOS::activeCommand(const Job &job, bool start, bool loaded, uint uid)
{
    const QString domain = domainTarget(job.domain, uid);
    const QString target = domain + QLatin1Char('/') + job.label;

    Command command;
    command.elevated = job.domain == Domain::SystemDaemon;
    if (!start)
        command.args = {QStringLiteral("bootout"), target};
    else if (!loaded && !job.plistPath.isEmpty())
        command.args = {QStringLiteral("bootstrap"), domain, job.plistPath};
    else
        command.args = {QStringLiteral("kickstart"), target};
    return command;
}

QList<ServiceToolMacOS::Job> ServiceToolMacOS::discoverJobs() const
{
    QList<Job> jobs;
    QSet<QString> seen;
    const QList<QPair<QString, Domain>> dirs = {
        {QDir::homePath() + QStringLiteral("/Library/LaunchAgents"), Domain::UserAgent},
        {QStringLiteral("/Library/LaunchAgents"), Domain::UserAgent},
        {QStringLiteral("/Library/LaunchDaemons"), Domain::SystemDaemon},
    };
    for (const auto &[dir, domain] : dirs) {
        for (const Job &job : jobsInDirectory(dir, domain)) {
            if (seen.contains(job.label))
                continue;
            seen.insert(job.label);
            jobs.append(job);
        }
    }
    return jobs;
}

QList<Service> ServiceToolMacOS::getServices()
{
    const uint uid = getuid();
    QList<Job> jobs = discoverJobs();

    const QHash<QString, bool> loadedAgents =
        parseLaunchctlList(CommandUtil::execWithStatus("launchctl", {"list"}).output);

    // Agents registered by an app at runtime have no plist in the standard
    // directories; keep listing them so nothing that used to show disappears.
    QSet<QString> known;
    for (const Job &job : std::as_const(jobs))
        known.insert(job.label);
    for (auto it = loadedAgents.constBegin(); it != loadedAgents.constEnd(); ++it) {
        if (known.contains(it.key()) || isHiddenLabel(it.key()))
            continue;
        Job job;
        job.label = it.key();
        jobs.append(job);
    }

    const QHash<QString, bool> agentOverrides = parsePrintDisabled(
        CommandUtil::execWithStatus("launchctl", {"print-disabled", domainTarget(Domain::UserAgent, uid)}).output);
    const QHash<QString, bool> daemonOverrides = parsePrintDisabled(
        CommandUtil::execWithStatus("launchctl", {"print-disabled", domainTarget(Domain::SystemDaemon, uid)}).output);

    QList<Service> services;
    QHash<QString, Job> byLabel;
    for (const Job &job : std::as_const(jobs)) {
        byLabel.insert(job.label, job);

        const bool daemon = job.domain == Domain::SystemDaemon;
        const bool enabled = isEnabled(job, daemon ? daemonOverrides : agentOverrides);
        const bool active = daemon ? isRunning(job) : loadedAgents.value(job.label, false);
        services.push_back({job.label, job.program.isEmpty() ? job.label : job.program, enabled, active});
    }

    std::sort(services.begin(), services.end(), [](const Service &a, const Service &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    QMutexLocker locker(&mMutex);
    mJobs = byLabel;
    return services;
}

ServiceToolMacOS::Job ServiceToolMacOS::jobFor(const QString &label)
{
    {
        QMutexLocker locker(&mMutex);
        const auto it = mJobs.constFind(label);
        if (it != mJobs.constEnd())
            return it.value();
    }
    for (const Job &job : discoverJobs()) {
        if (job.label == label)
            return job;
    }
    Job job;
    job.label = label;
    return job;
}

QString ServiceToolMacOS::getServiceDescription(const QString &serviceName)
{
    const Job job = jobFor(serviceName);
    return job.program.isEmpty() ? serviceName : job.program;
}

bool ServiceToolMacOS::isRunning(const Job &job) const
{
    // Reading job state needs no privileges, even in the system domain. A
    // non-zero exit just means the job is not loaded.
    const QString target = domainTarget(job.domain, getuid()) + QLatin1Char('/') + job.label;
    return parsePrintIsRunning(CommandUtil::execWithStatus("launchctl", {"print", target}).output);
}

bool ServiceToolMacOS::serviceIsActive(const QString &serviceName)
{
    return isRunning(jobFor(serviceName));
}

bool ServiceToolMacOS::serviceIsEnabled(const QString &serviceName)
{
    const Job job = jobFor(serviceName);
    const ExecResult result =
        CommandUtil::execWithStatus("launchctl", {"print-disabled", domainTarget(job.domain, getuid())});
    return isEnabled(job, parsePrintDisabled(result.output));
}

bool ServiceToolMacOS::run(const Command &command) const
{
    const ExecResult result = command.elevated
        ? CommandUtil::sudoExecWithStatus("launchctl", command.args, {}, kElevatedTimeoutMs)
        : CommandUtil::execWithStatus("launchctl", command.args);
    if (!result.ok()) {
        qCritical() << "launchctl" << command.args << "failed:" << result.error;
        return false;
    }
    return true;
}

bool ServiceToolMacOS::changeServiceStatus(const QString &sname, bool status)
{
    return run(enableCommand(jobFor(sname), status, getuid()));
}

bool ServiceToolMacOS::changeServiceActive(const QString &sname, bool status)
{
    const Job job = jobFor(sname);
    const uint uid = getuid();
    const QString target = domainTarget(job.domain, uid) + QLatin1Char('/') + job.label;
    const bool loaded = CommandUtil::execWithStatus("launchctl", {"print", target}).ok();
    if (!status && !loaded)
        return true;
    if (!run(activeCommand(job, status, loaded, uid)))
        return false;
    // Bootstrapping only loads the job; one without RunAtLoad would sit
    // loaded but idle, so "start" would appear to do nothing.
    if (status && !loaded && !job.plistPath.isEmpty())
        return run(activeCommand(job, true, true, uid));
    return true;
}
