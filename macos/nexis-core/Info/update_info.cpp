#include "update_info_macos.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

#include <Utils/command_util.h>
#include <Utils/brew_util.h>

void UpdateInfoMacOS::parseBrewOutdatedJson(const QByteArray &json, UpdateCheckResult &result)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isNull() || !doc.isObject())
        return;

    QJsonObject root = doc.object();
    QJsonArray formulae = root.value("formulae").toArray();
    QJsonArray casks = root.value("casks").toArray();

    for (const QJsonValue &val : formulae) {
        QJsonObject obj = val.toObject();
        UpdateEntry entry;
        entry.source = "brew";
        entry.name = obj.value("name").toString();
        entry.version = obj.value("current_version").toString();
        result.entries.append(entry);
    }

    for (const QJsonValue &val : casks) {
        QJsonObject obj = val.toObject();
        UpdateEntry entry;
        entry.source = "brew";
        entry.name = obj.value("name").toString();
        entry.version = obj.value("current_version").toString();
        result.entries.append(entry);
    }
}

void UpdateInfoMacOS::parseSoftwareUpdateLines(const QStringList &lines, UpdateCheckResult &result)
{
    static const QRegularExpression labelRe(R"(^\s*\*)");
    for (const QString &line : lines) {
        if (!labelRe.match(line).hasMatch())
            continue;
        UpdateEntry entry;
        entry.source = "system";
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("* Label:"))
            entry.name = trimmed.mid(8).trimmed();
        else if (trimmed.startsWith("*"))
            entry.name = trimmed.mid(1).trimmed();
        result.entries.append(entry);
    }
}

UpdateCheckResult UpdateInfoMacOS::checkForUpdates()
{
    UpdateCheckResult result;
    result.checkTime = QDateTime::currentDateTime();
    result.success = true;

    // Homebrew outdated packages
    QString brewPath = findBrew();
    if (!brewPath.isEmpty()) {
        ExecResult brewResult = CommandUtil::execWithStatus(
            brewPath, {"outdated", "--json"}, 30000);

        if (brewResult.exitCode == 0 && !brewResult.output.trimmed().isEmpty())
            parseBrewOutdatedJson(brewResult.output.toUtf8(), result);
    }

    // macOS system updates via softwareupdate
    ExecResult swResult = CommandUtil::execWithStatus(
        "softwareupdate", {"-l"}, 60000);

    if (swResult.exitCode == 0)
        parseSoftwareUpdateLines(swResult.output.split('\n'), result);

    result.totalCount = result.entries.size();
    return result;
}

QStringList UpdateInfoMacOS::availableSources() const
{
    QStringList sources;
    sources << "system";
    if (!findBrew().isEmpty())
        sources << "brew";
    return sources;
}
