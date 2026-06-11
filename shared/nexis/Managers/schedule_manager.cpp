#include "schedule_manager.h"
#include "setting_manager.h"
#include <Utils/command_util.h>
#include <Utils/macos_xattr_util.h>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDebug>

ScheduleManager *ScheduleManager::instance = nullptr;

ScheduleManager::ScheduleManager() : QObject(nullptr)
{
    loadSchedules();
}

ScheduleManager *ScheduleManager::ins()
{
    if (!instance) {
        instance = new ScheduleManager;
    }
    return instance;
}

void ScheduleManager::loadSchedules()
{
    mSchedules.clear();
    QString json = SettingManager::ins()->getSchedules();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        CleaningSchedule s;
        s.id = obj["id"].toString();
        s.name = obj["name"].toString();
        s.enabled = obj["enabled"].toBool(true);
        s.frequency = static_cast<Frequency>(obj["frequency"].toInt(Weekly));
        s.everyNDays = obj["everyNDays"].toInt(3);
        s.dayOfWeek = obj["dayOfWeek"].toInt(0);
        s.dayOfMonth = obj["dayOfMonth"].toInt(1);
        s.hour = obj["hour"].toInt(3);
        s.minute = obj["minute"].toInt(0);
        s.minFileAgeSecs = obj["minFileAgeSecs"].toInt(86400);
        s.lastBytesFreed = static_cast<quint64>(obj["lastBytesFreed"].toDouble(0));
        s.dryRunCompleted = obj["dryRunCompleted"].toBool(false);

        QJsonArray cats = obj["categories"].toArray();
        for (const QJsonValue &cv : cats) {
            s.categories.append(cv.toInt());
        }

        QString lastRunStr = obj["lastRun"].toString();
        if (!lastRunStr.isEmpty()) {
            s.lastRun = QDateTime::fromString(lastRunStr, Qt::ISODate);
        }

        mSchedules.append(s);
    }
}

void ScheduleManager::saveSchedules()
{
    QJsonArray arr;
    for (const CleaningSchedule &s : mSchedules) {
        QJsonObject obj;
        obj["id"] = s.id;
        obj["name"] = s.name;
        obj["enabled"] = s.enabled;
        obj["frequency"] = static_cast<int>(s.frequency);
        obj["everyNDays"] = s.everyNDays;
        obj["dayOfWeek"] = s.dayOfWeek;
        obj["dayOfMonth"] = s.dayOfMonth;
        obj["hour"] = s.hour;
        obj["minute"] = s.minute;
        obj["minFileAgeSecs"] = s.minFileAgeSecs;
        obj["lastBytesFreed"] = static_cast<qint64>(s.lastBytesFreed);
        obj["dryRunCompleted"] = s.dryRunCompleted;

        QJsonArray cats;
        for (int c : s.categories) {
            cats.append(c);
        }
        obj["categories"] = cats;

        if (s.lastRun.isValid()) {
            obj["lastRun"] = s.lastRun.toString(Qt::ISODate);
        }

        arr.append(obj);
    }

    SettingManager::ins()->setSchedules(
        QString(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QList<ScheduleManager::CleaningSchedule> ScheduleManager::getAllSchedules() const
{
    return mSchedules;
}

ScheduleManager::CleaningSchedule ScheduleManager::getSchedule(const QString &id) const
{
    for (const CleaningSchedule &s : mSchedules) {
        if (s.id == id) return s;
    }
    return CleaningSchedule();
}

QString ScheduleManager::createSchedule(const CleaningSchedule &schedule)
{
    CleaningSchedule s = schedule;
    if (s.id.isEmpty()) {
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    mSchedules.append(s);
    saveSchedules();
    syncToOS();
    emit schedulesChanged();
    return s.id;
}

void ScheduleManager::updateSchedule(const CleaningSchedule &schedule)
{
    for (int i = 0; i < mSchedules.size(); ++i) {
        if (mSchedules[i].id == schedule.id) {
            mSchedules[i] = schedule;
            saveSchedules();
            syncToOS();
            emit schedulesChanged();
            return;
        }
    }
}

void ScheduleManager::deleteSchedule(const QString &id)
{
    for (int i = 0; i < mSchedules.size(); ++i) {
        if (mSchedules[i].id == id) {
            mSchedules.removeAt(i);
            saveSchedules();
            syncToOS();
            emit schedulesChanged();
            return;
        }
    }
}

QDateTime ScheduleManager::getNextRunTime(const CleaningSchedule &schedule,
                                           const QDateTime &now) const
{
    QDateTime effectiveNow = now.isValid() ? now : QDateTime::currentDateTime();
    QDateTime next;

    switch (schedule.frequency) {
    case Daily:
        next = QDateTime(effectiveNow.date(), QTime(schedule.hour, schedule.minute));
        if (next <= effectiveNow) next = next.addDays(1);
        break;

    case EveryNDays: {
        if (schedule.lastRun.isValid()) {
            next = schedule.lastRun;
            next.setTime(QTime(schedule.hour, schedule.minute));
            while (next <= effectiveNow) {
                next = next.addDays(schedule.everyNDays);
            }
        } else {
            next = QDateTime(effectiveNow.date(), QTime(schedule.hour, schedule.minute));
            if (next <= effectiveNow) next = next.addDays(schedule.everyNDays);
        }
        break;
    }

    case Weekly: {
        int currentDow = effectiveNow.date().dayOfWeek() % 7; // Qt: Mon=1..Sun=7 → 0=Sun conversion
        int targetDow = schedule.dayOfWeek;
        int daysUntil = (targetDow - currentDow + 7) % 7;

        next = QDateTime(effectiveNow.date().addDays(daysUntil), QTime(schedule.hour, schedule.minute));
        if (next <= effectiveNow) next = next.addDays(7);
        break;
    }

    case Monthly: {
        QDate targetDate(effectiveNow.date().year(), effectiveNow.date().month(),
                         qMin(schedule.dayOfMonth, effectiveNow.date().daysInMonth()));
        next = QDateTime(targetDate, QTime(schedule.hour, schedule.minute));
        if (next <= effectiveNow) {
            QDate nextMonth = effectiveNow.date().addMonths(1);
            targetDate = QDate(nextMonth.year(), nextMonth.month(),
                               qMin(schedule.dayOfMonth, nextMonth.daysInMonth()));
            next = QDateTime(targetDate, QTime(schedule.hour, schedule.minute));
        }
        break;
    }
    }

    return next;
}

bool ScheduleManager::isEveryNDaysGateBlocked(Frequency frequency, int everyNDays,
                                              const QDateTime &lastRun,
                                              const QDateTime &now)
{
    if (frequency != EveryNDays) return false;
    if (!lastRun.isValid()) return false;
    if (everyNDays <= 0) return false;
    return lastRun.daysTo(now) < everyNDays;
}

QString ScheduleManager::frequencyDisplayText(const CleaningSchedule &schedule)
{
    switch (schedule.frequency) {
    case Daily:
        return QObject::tr("Daily at %1:%2")
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case EveryNDays:
        return QObject::tr("Every %1 days at %2:%3")
            .arg(schedule.everyNDays)
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case Weekly: {
        QStringList days = { QObject::tr("Sunday"), QObject::tr("Monday"),
                             QObject::tr("Tuesday"), QObject::tr("Wednesday"),
                             QObject::tr("Thursday"), QObject::tr("Friday"),
                             QObject::tr("Saturday") };
        QString dayName = (schedule.dayOfWeek >= 0 && schedule.dayOfWeek < 7)
                              ? days[schedule.dayOfWeek] : "?";
        return QObject::tr("Weekly on %1 at %2:%3")
            .arg(dayName)
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    }
    case Monthly:
        return QObject::tr("Monthly on day %1 at %2:%3")
            .arg(schedule.dayOfMonth)
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    }
    return QString();
}

// ─── OS-native scheduling ───────────────────────────────────────────────────

void ScheduleManager::syncToOS()
{
#ifdef Q_OS_MACOS
    // Remove all existing Nexis schedule plists, then recreate enabled ones
    QString agentsDir = QDir::homePath() + "/Library/LaunchAgents";
    QDir dir(agentsDir);
    for (const QString &f : dir.entryList({"com.nexis.clean.*.plist"}, QDir::Files)) {
        QString id = f;
        id.remove("com.nexis.clean.").remove(".plist");

        if (QProcess::execute("launchctl", {"unload", dir.absoluteFilePath(f)}) != 0)
            qWarning() << "launchctl unload failed for" << dir.absoluteFilePath(f);
        if (!QFile::remove(dir.absoluteFilePath(f)))
            qWarning() << "Failed to remove" << dir.absoluteFilePath(f);
    }

    for (const CleaningSchedule &s : mSchedules) {
        if (s.enabled) {
            createLaunchdPlist(s);
        }
    }
#else
    QString appPath = QCoreApplication::applicationFilePath();
    SchedulerType scheduler = detectScheduler();

    if (scheduler == Systemd) {
        // Remove all existing Nexis schedule timers
        QString unitDir = QDir::homePath() + "/.config/systemd/user";
        QDir dir(unitDir);
        for (const QString &f : dir.entryList({"nexis-clean-*.timer"}, QDir::Files)) {
            QString id = f;
            id.remove("nexis-clean-").remove(".timer");
            deleteSystemdTimer(id);
        }

        for (const CleaningSchedule &s : mSchedules) {
            if (s.enabled) {
                createSystemdTimer(s);
            }
        }
    } else {
        // Cron: remove all Nexis schedule entries, then recreate enabled ones
        QProcess proc;
        proc.start("crontab", {"-l"});
        proc.waitForFinished(5000);
        QString crontab = proc.readAllStandardOutput();

        QStringList lines = crontab.split('\n');
        QStringList filtered;
        for (const QString &line : lines) {
            if (!line.contains("# nexis-schedule:")) {
                filtered.append(line);
            }
        }

        for (const CleaningSchedule &s : mSchedules) {
            if (s.enabled) {
                QString cronTime;
                switch (s.frequency) {
                case Daily:
                    cronTime = QString("%1 %2 * * *").arg(s.minute).arg(s.hour);
                    break;
                case EveryNDays:
                    cronTime = QString("%1 %2 */%3 * *").arg(s.minute).arg(s.hour).arg(s.everyNDays);
                    break;
                case Weekly:
                    cronTime = QString("%1 %2 * * %3").arg(s.minute).arg(s.hour).arg(s.dayOfWeek);
                    break;
                case Monthly:
                    cronTime = QString("%1 %2 %3 * *").arg(s.minute).arg(s.hour).arg(s.dayOfMonth);
                    break;
                }
                filtered.append(QString("%1 %2 --clean %3 # nexis-schedule:%3")
                    .arg(cronTime, appPath, s.id));
            }
        }

        // Write back
        QProcess writeProc;
        writeProc.start("crontab", {"-"});
        writeProc.write(filtered.join('\n').toUtf8());
        writeProc.closeWriteChannel();
        writeProc.waitForFinished(5000);
    }
#endif
}

#ifdef Q_OS_MACOS

void ScheduleManager::createLaunchdPlist(const CleaningSchedule &schedule)
{
    QString agentsDir = QDir::homePath() + "/Library/LaunchAgents";
    QDir().mkpath(agentsDir);

    QString label = QString("com.nexis.clean.%1").arg(schedule.id);
    QString plistPath = agentsDir + "/" + label + ".plist";
    QString appPath = QCoreApplication::applicationFilePath();

    // Build StartCalendarInterval based on frequency
    QString calendarInterval;
    switch (schedule.frequency) {
    case Daily:
        calendarInterval = QString(
            "    <key>StartCalendarInterval</key>\n"
            "    <dict>\n"
            "        <key>Hour</key>\n"
            "        <integer>%1</integer>\n"
            "        <key>Minute</key>\n"
            "        <integer>%2</integer>\n"
            "    </dict>\n")
            .arg(schedule.hour).arg(schedule.minute);
        break;

    case EveryNDays:
        // launchd doesn't support "every N days" directly.
        // Use StartInterval (seconds) as an approximation.
        calendarInterval = QString(
            "    <key>StartInterval</key>\n"
            "    <integer>%1</integer>\n")
            .arg(schedule.everyNDays * 86400);
        break;

    case Weekly:
        calendarInterval = QString(
            "    <key>StartCalendarInterval</key>\n"
            "    <dict>\n"
            "        <key>Weekday</key>\n"
            "        <integer>%1</integer>\n"
            "        <key>Hour</key>\n"
            "        <integer>%2</integer>\n"
            "        <key>Minute</key>\n"
            "        <integer>%3</integer>\n"
            "    </dict>\n")
            .arg(schedule.dayOfWeek).arg(schedule.hour).arg(schedule.minute);
        break;

    case Monthly:
        calendarInterval = QString(
            "    <key>StartCalendarInterval</key>\n"
            "    <dict>\n"
            "        <key>Day</key>\n"
            "        <integer>%1</integer>\n"
            "        <key>Hour</key>\n"
            "        <integer>%2</integer>\n"
            "        <key>Minute</key>\n"
            "        <integer>%3</integer>\n"
            "    </dict>\n")
            .arg(schedule.dayOfMonth).arg(schedule.hour).arg(schedule.minute);
        break;
    }

    QString plistContent = QString(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key>\n"
        "    <string>%1</string>\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n"
        "        <string>%2</string>\n"
        "        <string>--clean</string>\n"
        "        <string>%3</string>\n"
        "    </array>\n"
        "%4"
        "    <key>RunAtLoad</key>\n"
        "    <false/>\n"
        "    <key>StandardOutPath</key>\n"
        "    <string>/tmp/nexis-clean-%5.log</string>\n"
        "    <key>StandardErrorPath</key>\n"
        "    <string>/tmp/nexis-clean-%5.log</string>\n"
        "</dict>\n"
        "</plist>\n")
        .arg(label, appPath, schedule.id, calendarInterval, schedule.id);

    QFile file(plistPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << plistContent;
        file.close();
    }

    // SSO-3731 (FW-04, MX1): macOS 27 refuses to load plists carrying the
    // `com.apple.quarantine` xattr. Strip it before `launchctl load`.
    MacOsXattrUtil::stripQuarantine(plistPath);

    if (QProcess::execute("launchctl", {"load", plistPath}) != 0)
        qWarning() << "launchctl load failed for" << plistPath;
}

void ScheduleManager::deleteLaunchdPlist(const QString &id)
{
    QString agentsDir = QDir::homePath() + "/Library/LaunchAgents";
    QString label = QString("com.nexis.clean.%1").arg(id);
    QString plistPath = agentsDir + "/" + label + ".plist";

    if (QProcess::execute("launchctl", {"unload", plistPath}) != 0)
        qWarning() << "launchctl unload failed for" << plistPath;
    if (!QFile::remove(plistPath))
        qWarning() << "Failed to remove" << plistPath;
}

#else // Linux

ScheduleManager::SchedulerType ScheduleManager::detectScheduler()
{
    QProcess proc;
    proc.start("systemctl", {"--user", "status"});
    proc.waitForFinished(3000);
    return (proc.exitCode() == 0) ? Systemd : Cron;
}

void ScheduleManager::createSystemdTimer(const CleaningSchedule &schedule)
{
    QString unitDir = QDir::homePath() + "/.config/systemd/user";
    QDir().mkpath(unitDir);

    QString baseName = QString("nexis-clean-%1").arg(schedule.id);
    QString appPath = QCoreApplication::applicationFilePath();

    // Build OnCalendar string
    QString onCalendar;
    switch (schedule.frequency) {
    case Daily:
        onCalendar = QString("*-*-* %1:%2:00")
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
        break;
    case EveryNDays:
        // SSO-3369: systemd has no native "every N days" trigger. We fire daily
        // and let CleanerService::cleanSchedule() consult
        // ScheduleManager::isEveryNDaysGateBlocked() to skip runs that fall
        // inside the interval since the last successful clean.
        onCalendar = QString("*-*-* %1:%2:00")
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
        break;
    case Weekly: {
        QStringList days = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        QString dayStr = (schedule.dayOfWeek >= 0 && schedule.dayOfWeek < 7)
                             ? days[schedule.dayOfWeek] : "Sun";
        onCalendar = QString("%1 *-*-* %2:%3:00")
            .arg(dayStr)
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
        break;
    }
    case Monthly:
        onCalendar = QString("*-*-%1 %2:%3:00")
            .arg(schedule.dayOfMonth, 2, 10, QChar('0'))
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
        break;
    }

    // Write .service file. SSO-3368: the user systemd unit inherits no
    // DISPLAY/WAYLAND_DISPLAY at boot catch-up time (Persistent=true), so
    // we set QT_QPA_PLATFORM=offscreen explicitly. main() also detects the
    // headless flag and forces offscreen as a second line of defence.
    QString serviceContent = QString(
        "[Unit]\n"
        "Description=Nexis Scheduled Clean - %1\n\n"
        "[Service]\n"
        "Type=oneshot\n"
        "Environment=QT_QPA_PLATFORM=offscreen\n"
        "ExecStart=%2 --clean %3\n")
        .arg(schedule.name, appPath, schedule.id);

    QFile serviceFile(unitDir + "/" + baseName + ".service");
    if (serviceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&serviceFile);
        stream << serviceContent;
        serviceFile.close();
    }

    // Write .timer file
    QString timerContent = QString(
        "[Unit]\n"
        "Description=Nexis Scheduled Clean Timer - %1\n\n"
        "[Timer]\n"
        "OnCalendar=%2\n"
        "Persistent=true\n\n"
        "[Install]\n"
        "WantedBy=timers.target\n")
        .arg(schedule.name, onCalendar);

    QFile timerFile(unitDir + "/" + baseName + ".timer");
    if (timerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&timerFile);
        stream << timerContent;
        timerFile.close();
    }

    if (QProcess::execute("systemctl", {"--user", "daemon-reload"}) != 0)
        qWarning() << "systemctl daemon-reload failed";
    if (QProcess::execute("systemctl", {"--user", "enable", "--now", baseName + ".timer"}) != 0)
        qWarning() << "systemctl enable failed for" << baseName + ".timer";
}

void ScheduleManager::deleteSystemdTimer(const QString &id)
{
    QString baseName = QString("nexis-clean-%1").arg(id);
    QString unitDir = QDir::homePath() + "/.config/systemd/user";

    if (QProcess::execute("systemctl", {"--user", "disable", "--now", baseName + ".timer"}) != 0)
        qWarning() << "systemctl disable failed for" << baseName + ".timer";
    if (!QFile::remove(unitDir + "/" + baseName + ".timer"))
        qWarning() << "Failed to remove" << unitDir + "/" + baseName + ".timer";
    if (!QFile::remove(unitDir + "/" + baseName + ".service"))
        qWarning() << "Failed to remove" << unitDir + "/" + baseName + ".service";
    if (QProcess::execute("systemctl", {"--user", "daemon-reload"}) != 0)
        qWarning() << "systemctl daemon-reload failed";
}

void ScheduleManager::createCronEntry(const CleaningSchedule &schedule)
{
    QString appPath = QCoreApplication::applicationFilePath();
    QString cronTime;

    switch (schedule.frequency) {
    case Daily:
        cronTime = QString("%1 %2 * * *")
            .arg(schedule.minute).arg(schedule.hour);
        break;
    case EveryNDays:
        cronTime = QString("%1 %2 */%3 * *")
            .arg(schedule.minute).arg(schedule.hour).arg(schedule.everyNDays);
        break;
    case Weekly:
        cronTime = QString("%1 %2 * * %3")
            .arg(schedule.minute).arg(schedule.hour).arg(schedule.dayOfWeek);
        break;
    case Monthly:
        cronTime = QString("%1 %2 %3 * *")
            .arg(schedule.minute).arg(schedule.hour).arg(schedule.dayOfMonth);
        break;
    }

    // SSO-3368: cron never sets DISPLAY/WAYLAND_DISPLAY, so steer Qt onto the
    // offscreen platform inline. main() will also detect the headless flag
    // and force offscreen as a second line of defence.
    QString cronLine = QString("%1 QT_QPA_PLATFORM=offscreen %2 --clean %3 # nexis-schedule:%3")
        .arg(cronTime, appPath, schedule.id);

    // Append to crontab
    QProcess readProc;
    readProc.start("crontab", {"-l"});
    readProc.waitForFinished(5000);
    QString crontab = readProc.readAllStandardOutput();

    if (!crontab.endsWith('\n') && !crontab.isEmpty())
        crontab += '\n';
    crontab += cronLine + '\n';

    QProcess writeProc;
    writeProc.start("crontab", {"-"});
    writeProc.write(crontab.toUtf8());
    writeProc.closeWriteChannel();
    writeProc.waitForFinished(5000);
}

void ScheduleManager::deleteCronEntry(const QString &id)
{
    QProcess readProc;
    readProc.start("crontab", {"-l"});
    readProc.waitForFinished(5000);
    QString crontab = readProc.readAllStandardOutput();

    QString marker = QString("# nexis-schedule:%1").arg(id);
    QStringList lines = crontab.split('\n');
    QStringList filtered;
    for (const QString &line : lines) {
        if (!line.contains(marker)) {
            filtered.append(line);
        }
    }

    QProcess writeProc;
    writeProc.start("crontab", {"-"});
    writeProc.write(filtered.join('\n').toUtf8());
    writeProc.closeWriteChannel();
    writeProc.waitForFinished(5000);
}

#endif // Q_OS_MACOS
