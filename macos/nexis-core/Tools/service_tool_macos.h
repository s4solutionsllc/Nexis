#ifndef SERVICE_TOOL_MACOS_H
#define SERVICE_TOOL_MACOS_H

#include <Tools/service_tool.h>

#include <QHash>
#include <QMutex>
#include <QStringList>

class ServiceToolMacOS : public ServiceTool
{
public:
    // Which launchd domain a job lives in. Agents run in the logged-in
    // user's GUI domain and need no privileges; daemons run in the system
    // domain and need an admin prompt to change.
    enum class Domain { UserAgent, SystemDaemon };

    struct Job {
        QString label;
        QString plistPath;
        QString program;
        Domain domain = Domain::UserAgent;
        // The plist's own Disabled key; a launchctl override wins over it.
        bool disabledInPlist = false;
    };

    struct Command {
        QStringList args;
        bool elevated = false;
    };

    QList<Service> getServices() override;
    bool serviceIsActive(const QString &serviceName) override;
    bool changeServiceStatus(const QString &sname, bool status) override;
    bool changeServiceActive(const QString &sname, bool status) override;
    bool serviceIsEnabled(const QString &serviceName) override;
    QString getServiceDescription(const QString &serviceName) override;

    // Pure helpers, exposed for tests.

    // `launchctl list` output -> label -> running (has a PID).
    static QHash<QString, bool> parseLaunchctlList(const QString &output);
    // `launchctl print-disabled <domain>` output -> label -> disabled. Only
    // jobs with an explicit override appear.
    static QHash<QString, bool> parsePrintDisabled(const QString &output);
    static bool isEnabled(const Job &job, const QHash<QString, bool> &overrides);
    // `launchctl print <domain>/<label>` output -> true when "state = running".
    static bool parsePrintIsRunning(const QString &output);
    // Labels that are not user-manageable services: Apple's own jobs and the
    // per-launch "application.<bundle>.<n>" entries launchd creates for apps.
    static bool isHiddenLabel(const QString &label);
    // Reads Label / Program / ProgramArguments[0] from every plist in `dir`.
    static QList<Job> jobsInDirectory(const QString &dir, Domain domain);
    static QString domainTarget(Domain domain, uint uid);
    static Command enableCommand(const Job &job, bool enable, uint uid);
    // Start: bootstrap the plist when the job is not loaded (the caller then
    // kickstarts it), else kickstart. Stop: bootout, so a KeepAlive job stays
    // stopped.
    static Command activeCommand(const Job &job, bool start, bool loaded, uint uid);

private:
    QList<Job> discoverJobs() const;
    Job jobFor(const QString &label);
    bool isRunning(const Job &job) const;
    bool run(const Command &command) const;

    QMutex mMutex;
    QHash<QString, Job> mJobs;
};

#endif // SERVICE_TOOL_MACOS_H
