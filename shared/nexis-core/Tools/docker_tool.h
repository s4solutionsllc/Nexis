#ifndef DOCKER_TOOL_H
#define DOCKER_TOOL_H

#include <QString>
#include <QList>

#include "nexis-core_global.h"

struct DockerImage {
    QString id;
    QString repository;
    QString tag;
    QString size;
    qint64 sizeBytes;
    QString createdAt;
    bool isDangling;
    bool isUsed;
};

struct DockerContainer {
    QString id;
    QString name;
    QString status;
    QString state;   // "running", "exited", "paused", "created"
    QString image;
    QString ports;
    QString createdAt;
};

struct DockerVolume {
    QString name;
    QString driver;
    QString mountpoint;
    bool isUsed;
};

class NEXISCORESHARED_EXPORT DockerTool
{
public:
    static bool isDockerInstalled();
    static bool isDaemonRunning();
    static QString dockerVersion();

    static QList<DockerImage> getImages();
    static QList<DockerContainer> getContainers();
    static QList<DockerVolume> getVolumes();

    static bool removeImages(const QStringList &ids);
    static bool removeContainers(const QStringList &ids);
    static bool removeVolumes(const QStringList &names);

    static int pruneImages();
    static int pruneContainers();
    static int pruneVolumes();

    static bool startContainer(const QString &id);
    static bool stopContainer(const QString &id);

private:
    static QStringList getUsedImageIds();
    static QStringList getUsedVolumeNames();
    static qint64 parseSizeToBytes(const QString &sizeStr);
};

#endif // DOCKER_TOOL_H
