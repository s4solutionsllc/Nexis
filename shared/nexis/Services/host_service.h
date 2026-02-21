#ifndef HOST_SERVICE_H
#define HOST_SERVICE_H

#include <QObject>
#include <QMap>

struct HostEntry {
    QString ip;
    QString fullQualified;
    QString aliases;
};

class HostService : public QObject
{
    Q_OBJECT

public:
    static HostService *ins();

    QStringList readHostFile();
    QMap<int, HostEntry> parseHostEntries(const QStringList &fileContent) const;

    static bool isValidIP(const QString &ip);
    static bool isValidHostname(const QString &hostname);

    bool saveHostFile(const QStringList &content);
    bool createBackup();

signals:
    void saveSucceeded();
    void saveFailed(QString errorMessage);
    void backupFailed(QString errorMessage);

private:
    explicit HostService(QObject *parent = nullptr);
    static HostService *instance;
};

#endif // HOST_SERVICE_H
