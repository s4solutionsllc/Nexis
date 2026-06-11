#ifndef STARTUP_SERVICE_H
#define STARTUP_SERVICE_H

#include <QObject>
#include <QFileSystemWatcher>
#include <memory>

#include <Info/startup_info.h>

#ifdef Q_OS_MACOS
#include <Info/btm_parser.h>
#endif

class StartupService : public QObject
{
    Q_OBJECT

public:
    static StartupService *ins();

    QList<StartupAppData> getApps() const;
    QList<StartupAppData> getAllLoginItems() const;
    QString autostartPath() const;
    bool isAutostartDisabled() const;

#ifdef Q_OS_MACOS
    // SSO-3738 / FW-10
    QList<BtmRecord> getBtmRecords(QString *error = nullptr) const;
    // Runs `sudo sfltool resetbtm`. Returns true on success; otherwise
    // populates `error` with the captured stderr/exit code.
    bool resetBtm(QString *error = nullptr);
#endif

signals:
    void appsChanged();

private:
    explicit StartupService(QObject *parent = nullptr);
    static StartupService *instance;

    std::unique_ptr<StartupInfo> mInfo;
    QFileSystemWatcher mWatcher;
};

#endif // STARTUP_SERVICE_H
