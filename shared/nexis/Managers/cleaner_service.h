#ifndef CLEANER_SERVICE_H
#define CLEANER_SERVICE_H

#include <QObject>
#include <QMap>
#include <QFileInfoList>
#include <QDateTime>

class CleanerService : public QObject
{
    Q_OBJECT

public:
    enum CleanCategory {
        PACKAGE_CACHE,
        CRASH_REPORTS,
        APPLICATION_LOGS,
        APPLICATION_CACHES,
        TRASH,
        DEV_TOOL_CACHES,
        BROKEN_SYMLINKS,
        BROWSER_PRIVACY,
        SNAP_FLATPAK_REVISIONS
    };

    struct ScanResult {
        QMap<CleanCategory, QFileInfoList> categoryFiles;
        quint64 totalSize = 0;
    };

    struct CleanResult {
        quint64 totalBytesFreed = 0;
        int totalFilesRemoved = 0;
        QMap<CleanCategory, quint64> categoryBreakdown;
        QDateTime timestamp;
        QString scheduleName;
    };

    static CleanerService *ins();

    ScanResult scan(const QList<CleanCategory> &categories);
    CleanResult clean(const QList<CleanCategory> &categories, int minFileAgeSecs = 0);
    CleanResult cleanSchedule(const QString &scheduleId);

    static QString categoryName(CleanCategory cat);
    static QList<CleanCategory> allCategories();

    quint64 cleanTrash();
    quint64 cleanFiles(const QStringList &paths, int minFileAgeSecs = 0);

signals:
    void cleaningStarted(QString scheduleName);
    void cleaningFinished(CleanResult result);

private:
    CleanerService();
    static CleanerService *instance;

    void logCleanResult(const CleanResult &result);
};

#endif // CLEANER_SERVICE_H
