#include "docker_tool.h"
#include <Utils/command_util.h>
#include <QRegularExpression>

static const QString DOCKER_CMD = "docker";

bool DockerTool::isDockerInstalled()
{
    return CommandUtil::isExecutable(DOCKER_CMD);
}

bool DockerTool::isDaemonRunning()
{
    if (!isDockerInstalled())
        return false;

    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"version"}, 5000);
    return result.exitCode == 0;
}

QString DockerTool::dockerVersion()
{
    if (!isDaemonRunning())
        return {};

    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"version", "--format", "{{.Server.Version}}"}, 5000);
    if (result.exitCode == 0)
        return result.output.trimmed();

    return {};
}

QStringList DockerTool::getUsedImageIds()
{
    QStringList usedIds;
    ExecResult result = CommandUtil::execWithStatus(
        DOCKER_CMD, {"ps", "--all", "--format", "{{.Image}}"}, 10000);

    if (result.exitCode == 0) {
        const QStringList lines = result.output.trimmed().split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
            usedIds << line.trimmed();
    }
    return usedIds;
}

QStringList DockerTool::getUsedVolumeNames()
{
    QStringList used;
    ExecResult result = CommandUtil::execWithStatus(
        DOCKER_CMD, {"ps", "--all", "--format", "{{.Mounts}}"}, 10000);

    if (result.exitCode == 0) {
        const QStringList lines = result.output.trimmed().split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList mounts = line.trimmed().split(',', Qt::SkipEmptyParts);
            for (const QString &m : mounts)
                used << m.trimmed();
        }
    }
    return used;
}

qint64 DockerTool::parseSizeToBytes(const QString &sizeStr)
{
    static const QRegularExpression re(R"(^([\d.]+)\s*(B|KB|MB|GB|TB|kB)$)");
    QRegularExpressionMatch match = re.match(sizeStr.trimmed());
    if (!match.hasMatch())
        return 0;

    double value = match.captured(1).toDouble();
    QString unit = match.captured(2).toUpper();

    if (unit == "B")  return static_cast<qint64>(value);
    if (unit == "KB") return static_cast<qint64>(value * 1000);
    if (unit == "MB") return static_cast<qint64>(value * 1000 * 1000);
    if (unit == "GB") return static_cast<qint64>(value * 1000 * 1000 * 1000);
    if (unit == "TB") return static_cast<qint64>(value * 1000 * 1000 * 1000 * 1000);
    return 0;
}

QList<DockerImage> DockerTool::getImages()
{
    QList<DockerImage> images;
    if (!isDaemonRunning())
        return images;

    ExecResult result = CommandUtil::execWithStatus(
        DOCKER_CMD,
        {"images", "--all", "--format", "{{.ID}}|{{.Repository}}|{{.Tag}}|{{.Size}}|{{.CreatedAt}}"},
        15000);

    if (result.exitCode != 0)
        return images;

    const QStringList usedRefs = getUsedImageIds();
    const QStringList lines = result.output.trimmed().split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.trimmed().split('|');
        if (parts.size() < 5)
            continue;

        DockerImage img;
        img.id = parts[0].trimmed();
        img.repository = parts[1].trimmed();
        img.tag = parts[2].trimmed();
        img.size = parts[3].trimmed();
        img.sizeBytes = parseSizeToBytes(img.size);
        img.createdAt = parts[4].trimmed();
        img.isDangling = (img.repository == "<none>" && img.tag == "<none>");

        QString ref = img.repository + ":" + img.tag;
        img.isUsed = usedRefs.contains(ref) || usedRefs.contains(img.id) || usedRefs.contains(img.repository);

        images.append(img);
    }

    return images;
}

QList<DockerContainer> DockerTool::getContainers()
{
    QList<DockerContainer> containers;
    if (!isDaemonRunning())
        return containers;

    ExecResult result = CommandUtil::execWithStatus(
        DOCKER_CMD,
        {"ps", "--all", "--format", "{{.ID}}|{{.Names}}|{{.Status}}|{{.State}}|{{.Image}}|{{.Ports}}|{{.CreatedAt}}"},
        15000);

    if (result.exitCode != 0)
        return containers;

    const QStringList lines = result.output.trimmed().split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.trimmed().split('|');
        if (parts.size() < 7)
            continue;

        DockerContainer ctr;
        ctr.id = parts[0].trimmed();
        ctr.name = parts[1].trimmed();
        ctr.status = parts[2].trimmed();
        ctr.state = parts[3].trimmed().toLower();
        ctr.image = parts[4].trimmed();
        ctr.ports = parts[5].trimmed();
        ctr.createdAt = parts[6].trimmed();
        containers.append(ctr);
    }

    return containers;
}

QList<DockerVolume> DockerTool::getVolumes()
{
    QList<DockerVolume> volumes;
    if (!isDaemonRunning())
        return volumes;

    ExecResult result = CommandUtil::execWithStatus(
        DOCKER_CMD,
        {"volume", "ls", "--format", "{{.Name}}|{{.Driver}}|{{.Mountpoint}}"},
        15000);

    if (result.exitCode != 0)
        return volumes;

    QStringList danglingNames;
    ExecResult danglingResult = CommandUtil::execWithStatus(
        DOCKER_CMD,
        {"volume", "ls", "--filter", "dangling=true", "--format", "{{.Name}}"},
        10000);
    if (danglingResult.exitCode == 0) {
        danglingNames = danglingResult.output.trimmed().split('\n', Qt::SkipEmptyParts);
        for (QString &n : danglingNames)
            n = n.trimmed();
    }

    const QStringList lines = result.output.trimmed().split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.trimmed().split('|');
        if (parts.size() < 3)
            continue;

        DockerVolume vol;
        vol.name = parts[0].trimmed();
        vol.driver = parts[1].trimmed();
        vol.mountpoint = parts[2].trimmed();
        vol.isUsed = !danglingNames.contains(vol.name);
        volumes.append(vol);
    }

    return volumes;
}

bool DockerTool::removeImages(const QStringList &ids)
{
    bool allOk = true;
    for (const QString &id : ids) {
        ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"rmi", id}, 30000);
        if (result.exitCode != 0)
            allOk = false;
    }
    return allOk;
}

bool DockerTool::removeContainers(const QStringList &ids)
{
    bool allOk = true;
    for (const QString &id : ids) {
        ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"rm", id}, 30000);
        if (result.exitCode != 0)
            allOk = false;
    }
    return allOk;
}

bool DockerTool::removeVolumes(const QStringList &names)
{
    bool allOk = true;
    for (const QString &name : names) {
        ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"volume", "rm", name}, 30000);
        if (result.exitCode != 0)
            allOk = false;
    }
    return allOk;
}

int DockerTool::pruneImages()
{
    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"image", "prune", "--force"}, 60000);
    if (result.exitCode != 0)
        return -1;

    static const QRegularExpression re(R"(Total reclaimed space:\s*(.+))");
    QRegularExpressionMatch match = re.match(result.output);
    return match.hasMatch() ? 1 : 0;
}

int DockerTool::pruneContainers()
{
    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"container", "prune", "--force"}, 60000);
    return (result.exitCode == 0) ? 1 : -1;
}

int DockerTool::pruneVolumes()
{
    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"volume", "prune", "--force"}, 60000);
    return (result.exitCode == 0) ? 1 : -1;
}

bool DockerTool::startContainer(const QString &id)
{
    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"start", id}, 30000);
    return result.exitCode == 0;
}

bool DockerTool::stopContainer(const QString &id)
{
    ExecResult result = CommandUtil::execWithStatus(DOCKER_CMD, {"stop", id}, 30000);
    return result.exitCode == 0;
}
