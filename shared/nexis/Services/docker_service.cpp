#include "docker_service.h"
#include <QThreadPool>

DockerService *DockerService::instance = nullptr;

DockerService *DockerService::ins()
{
    if (!instance)
        instance = new DockerService;
    return instance;
}

DockerService::DockerService(QObject *parent)
    : QObject(parent)
{
}

void DockerService::checkDaemon()
{
    QThreadPool::globalInstance()->start([this]() {
        bool running = DockerTool::isDaemonRunning();
        QString ver = running ? DockerTool::dockerVersion() : QString();
        emit daemonChecked(running, ver);
    });
}

void DockerService::fetchImages()
{
    QThreadPool::globalInstance()->start([this]() {
        QList<DockerImage> images = DockerTool::getImages();
        emit imagesFetched(images);
    });
}

void DockerService::fetchContainers()
{
    QThreadPool::globalInstance()->start([this]() {
        QList<DockerContainer> containers = DockerTool::getContainers();
        emit containersFetched(containers);
    });
}

void DockerService::fetchVolumes()
{
    QThreadPool::globalInstance()->start([this]() {
        QList<DockerVolume> volumes = DockerTool::getVolumes();
        emit volumesFetched(volumes);
    });
}

void DockerService::removeImages(const QStringList &ids)
{
    QThreadPool::globalInstance()->start([this, ids]() {
        DockerTool::removeImages(ids);
        QList<DockerImage> images = DockerTool::getImages();
        emit imagesFetched(images);
    });
}

void DockerService::removeContainers(const QStringList &ids)
{
    QThreadPool::globalInstance()->start([this, ids]() {
        DockerTool::removeContainers(ids);
        QList<DockerContainer> containers = DockerTool::getContainers();
        emit containersFetched(containers);
    });
}

void DockerService::removeVolumes(const QStringList &names)
{
    QThreadPool::globalInstance()->start([this, names]() {
        DockerTool::removeVolumes(names);
        QList<DockerVolume> volumes = DockerTool::getVolumes();
        emit volumesFetched(volumes);
    });
}

void DockerService::pruneImages()
{
    QThreadPool::globalInstance()->start([this]() {
        DockerTool::pruneImages();
        QList<DockerImage> images = DockerTool::getImages();
        emit imagesFetched(images);
    });
}

void DockerService::pruneContainers()
{
    QThreadPool::globalInstance()->start([this]() {
        DockerTool::pruneContainers();
        QList<DockerContainer> containers = DockerTool::getContainers();
        emit containersFetched(containers);
    });
}

void DockerService::pruneVolumes()
{
    QThreadPool::globalInstance()->start([this]() {
        DockerTool::pruneVolumes();
        QList<DockerVolume> volumes = DockerTool::getVolumes();
        emit volumesFetched(volumes);
    });
}

void DockerService::startContainers(const QStringList &ids)
{
    QThreadPool::globalInstance()->start([this, ids]() {
        for (const QString &id : ids)
            DockerTool::startContainer(id);
        QList<DockerContainer> containers = DockerTool::getContainers();
        emit containersFetched(containers);
    });
}

void DockerService::stopContainers(const QStringList &ids)
{
    QThreadPool::globalInstance()->start([this, ids]() {
        for (const QString &id : ids)
            DockerTool::stopContainer(id);
        QList<DockerContainer> containers = DockerTool::getContainers();
        emit containersFetched(containers);
    });
}
