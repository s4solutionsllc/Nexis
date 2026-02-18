#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QUuid>

class ScheduleManager : public QObject
{
    Q_OBJECT

public:
    enum Frequency {
        Daily,
        EveryNDays,
        Weekly,
        Monthly
    };

    struct CleaningSchedule {
        QString id;
        QString name;
        bool enabled = true;
        Frequency frequency = Weekly;
        int everyNDays = 3;
        int dayOfWeek = 0;      // 0=Sunday
        int dayOfMonth = 1;     // 1–31
        int hour = 3;
        int minute = 0;
        QList<int> categories;  // CleanerService::CleanCategory values
        int minFileAgeSecs = 86400;
        QDateTime lastRun;
        quint64 lastBytesFreed = 0;
        bool dryRunCompleted = false;
    };

    static ScheduleManager *ins();

    QList<CleaningSchedule> getAllSchedules() const;
    CleaningSchedule getSchedule(const QString &id) const;
    QString createSchedule(const CleaningSchedule &schedule);
    void updateSchedule(const CleaningSchedule &schedule);
    void deleteSchedule(const QString &id);

    void syncToOS();
    QDateTime getNextRunTime(const CleaningSchedule &schedule) const;

    static QString frequencyDisplayText(const CleaningSchedule &schedule);

signals:
    void schedulesChanged();

private:
    ScheduleManager();
    static ScheduleManager *instance;

    void loadSchedules();
    void saveSchedules();

    // OS-native scheduling
#ifdef Q_OS_MACOS
    void createLaunchdPlist(const CleaningSchedule &schedule);
    void deleteLaunchdPlist(const QString &id);
#else
    enum SchedulerType { Systemd, Cron };
    SchedulerType detectScheduler();
    void createSystemdTimer(const CleaningSchedule &schedule);
    void deleteSystemdTimer(const QString &id);
    void createCronEntry(const CleaningSchedule &schedule);
    void deleteCronEntry(const QString &id);
#endif

    QList<CleaningSchedule> mSchedules;
};

#endif // SCHEDULE_MANAGER_H
