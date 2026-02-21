#ifndef DOCKER_SERVICE_H
#define DOCKER_SERVICE_H

#include <QObject>
#include <QFuture>
#include <Tools/docker_tool.h>

class DockerService : public QObject
{
    Q_OBJECT

public:
    static DockerService *ins();

    explicit DockerService(QObject *parent = nullptr);

    void checkDaemon();
    void fetchImages();
    void fetchContainers();
    void fetchVolumes();

    void removeImages(const QStringList &ids);
    void removeContainers(const QStringList &ids);
    void removeVolumes(const QStringList &names);

    void pruneImages();
    void pruneContainers();
    void pruneVolumes();

    void startContainers(const QStringList &ids);
    void stopContainers(const QStringList &ids);

signals:
    void daemonChecked(bool running, const QString &version);
    void imagesFetched(QList<DockerImage> images);
    void containersFetched(QList<DockerContainer> containers);
    void volumesFetched(QList<DockerVolume> volumes);

    void operationFinished(const QString &type);

private:
    static DockerService *instance;
};

#endif // DOCKER_SERVICE_H
