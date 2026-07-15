#include "power_profile_info_linux.h"
#include <Utils/command_util.h>
#include <QDebug>
#include <QFile>
#include <QDir>

static const QString SYSFS_GOVERNOR =
    QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
static const QString SYSFS_AVAILABLE =
    QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors");
static const QString SYSFS_DRIVER =
    QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver");

PowerProfileInfoLinux::PowerProfileInfoLinux()
{
    detectBackend();
    if (mData.backend != PowerBackend::None) {
        detectConflicts();
        refresh();
    }
}

void PowerProfileInfoLinux::detectBackend()
{
    if (CommandUtil::isExecutable("powerprofilesctl")) {
        mData.backend = PowerBackend::PowerProfilesDaemon;
    } else if (QFile::exists(SYSFS_GOVERNOR)) {
        mData.backend = PowerBackend::Sysfs;
    } else {
        mData.backend = PowerBackend::None;
    }
}

void PowerProfileInfoLinux::detectConflicts()
{
    QStringList conflicting = {"tlp.service", "auto-cpufreq.service"};
    QStringList active;

    for (const QString &svc : conflicting) {
        ExecResult r = CommandUtil::execWithStatus(
            "systemctl", {"is-active", "--quiet", svc}, 3000);
        if (r.exitCode == 0)
            active.append(svc.chopped(8)); // strip ".service"
    }

    if (!active.isEmpty()) {
        mData.conflictWarning = QString("%1 detected — profile changes may be overridden")
                                    .arg(active.join(", "));
    }
}

void PowerProfileInfoLinux::refresh()
{
    if (mData.backend == PowerBackend::PowerProfilesDaemon)
        refreshPPD();
    else if (mData.backend == PowerBackend::Sysfs)
        refreshSysfs();
}

void PowerProfileInfoLinux::refreshPPD()
{
    ExecResult r = CommandUtil::execWithStatus("powerprofilesctl", {"list"}, 5000);
    if (r.exitCode != 0) {
        qWarning() << "power_profile_info: powerprofilesctl list failed:" << r.error;
        return;
    }

    PowerProfileData parsed = parsePowerprofilesctlList(r.output);
    parsed.conflictWarning = mData.conflictWarning;
    mData = parsed;
}

void PowerProfileInfoLinux::refreshSysfs()
{
    QFile fGov(SYSFS_GOVERNOR);
    QFile fAvail(SYSFS_AVAILABLE);
    QFile fDriver(SYSFS_DRIVER);

    QString current, available, driver;

    if (fGov.open(QIODevice::ReadOnly | QIODevice::Text)) {
        current = QString::fromUtf8(fGov.readAll()).trimmed();
        fGov.close();
    }
    if (fAvail.open(QIODevice::ReadOnly | QIODevice::Text)) {
        available = QString::fromUtf8(fAvail.readAll()).trimmed();
        fAvail.close();
    }
    if (fDriver.open(QIODevice::ReadOnly | QIODevice::Text)) {
        driver = QString::fromUtf8(fDriver.readAll()).trimmed();
        fDriver.close();
    }

    PowerProfileData parsed = parseSysfsGovernors(available, current, driver);
    parsed.conflictWarning = mData.conflictWarning;
    mData = parsed;
}

bool PowerProfileInfoLinux::setProfile(const QString &profile)
{
    if (mData.backend == PowerBackend::PowerProfilesDaemon)
        return setPPD(profile);
    if (mData.backend == PowerBackend::Sysfs)
        return setSysfs(profile);
    return false;
}

bool PowerProfileInfoLinux::setPPD(const QString &profile)
{
    ExecResult r = CommandUtil::execWithStatus(
        "powerprofilesctl", {"set", profile}, 5000);

    if (r.exitCode == 0) {
        mData.activeProfile = profile;
        return true;
    }
    qWarning() << "power_profile_info: powerprofilesctl set" << profile << "failed:" << r.error;
    return false;
}

bool PowerProfileInfoLinux::setSysfs(const QString &governor)
{
    QString cmd = QString("echo %1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor")
                      .arg(governor);

    ExecResult r = CommandUtil::sudoExecWithStatus("sh", {"-c", cmd});
    if (!r.ok()) {
        qWarning() << "power_profile_info: failed to set sysfs governor" << governor << ":" << r.error;
        return false;
    }
    mData.activeProfile = governor;
    return true;
}
