#ifndef HEALTH_REPORT_MANAGER_H
#define HEALTH_REPORT_MANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>

// FW-14 (SSO-3742): on-demand and scheduled health-report export.
//
// Produces a Markdown report summarising CPU, memory, disk, battery, SMART,
// thermal, GPU, and cleanable space. The optional scheduled variant is
// registered via ScheduleManager::ReportSchedule and invoked headlessly with
// `--report <schedule-id>` mirroring the `--clean` path (audit WI-06).

class InfoManager;
class CleanerService;

class HealthReportManager
{
public:
    static HealthReportManager *ins();

    struct ReportSchedule {
        QString id;
        QString name;
        bool enabled = true;

        // Timing — same fields as CleaningSchedule so we can reuse
        // ScheduleManager::Frequency and the launchd/systemd/cron backend.
        int frequencyInt = 1; // maps to ScheduleManager::Frequency::Weekly
        int everyNDays  = 7;
        int dayOfWeek   = 0;  // 0 = Sunday
        int dayOfMonth  = 1;
        int hour        = 6;
        int minute      = 0;

        QString outputDir; // empty → QStandardPaths::DocumentsLocation
        QDateTime lastRun;
    };

    // Generate a Markdown health report and return the file path written.
    // If outputPath is empty a timestamped file is created in DocumentsLocation.
    QString generateReport(const QString &outputPath = {}) const;

    // Scheduled-report CRUD — persisted alongside cleaning schedules in
    // SettingManager (separate key "reportSchedules").
    QList<ReportSchedule> getAllReportSchedules() const;
    ReportSchedule        getReportSchedule(const QString &id) const;
    QString               createReportSchedule(const ReportSchedule &s);
    void                  updateReportSchedule(const ReportSchedule &s);
    void                  deleteReportSchedule(const QString &id);

    // Sync all enabled ReportSchedules to the OS scheduler using the same
    // launchd/systemd/cron backend as ScheduleManager::syncToOS().
    void syncReportSchedulesToOS();

    // Headless entry-point: generate a report for the given schedule id,
    // update lastRun, and return the path written. Called by main() when
    // `--report <id>` is passed.
    QString runScheduledReport(const QString &scheduleId);

private:
    HealthReportManager();
    static HealthReportManager *instance;

    // Report section builders
    QString buildHeader() const;
    QString buildCpuSection() const;
    QString buildMemorySection() const;
    QString buildDiskSection() const;
    QString buildDriveHealthSection() const;
    QString buildThermalSection() const;
    QString buildGpuSection() const;
    QString buildBatterySection() const;
    QString buildCleanableSpaceSection() const;
    QString buildFooter() const;

    QString resolveOutputPath(const QString &requested) const;

    void loadReportSchedules();
    void saveReportSchedules();

    QList<ReportSchedule> mReportSchedules;
};

#endif // HEALTH_REPORT_MANAGER_H
