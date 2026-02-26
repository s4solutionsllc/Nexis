#ifndef DUPLICATE_FINDER_SERVICE_H
#define DUPLICATE_FINDER_SERVICE_H

#include <QObject>
#include <QFileInfo>
#include <QAtomicInt>
#include <QFuture>

struct DuplicateGroup {
    QList<QFileInfo> files;
    quint64 fileSize = 0;
    QByteArray hash;
};

class DuplicateFinderService : public QObject
{
    Q_OBJECT

public:
    static DuplicateFinderService *ins();

    void scan(const QStringList &directories, qint64 minSize,
              const QString &globFilter = QString());
    void cancel();
    bool isScanning() const;

signals:
    void progressUpdated(int stage, int current, int total, const QString &message);
    void scanFinished(const QList<DuplicateGroup> &results);
    void scanCancelled();

private:
    explicit DuplicateFinderService(QObject *parent = nullptr);
    static DuplicateFinderService *instance;

    QList<DuplicateGroup> runPipeline(const QStringList &directories,
                                       qint64 minSize,
                                       const QString &globFilter);

    QAtomicInt mCancelled{0};
    QFuture<void> mWorkerFuture;
};

#endif // DUPLICATE_FINDER_SERVICE_H
