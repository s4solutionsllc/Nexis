#include "update_info_macos.h"
#include "sparkle_update_scanner.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

#include <Utils/command_util.h>
#include <Utils/brew_util.h>

UpdateCheckResult UpdateInfoMacOS::checkForUpdates()
{
    UpdateCheckResult result;
    result.checkTime = QDateTime::currentDateTime();
    result.success = true;

    // Homebrew outdated packages
    QString brewPath = findBrew();
    if (!brewPath.isEmpty()) {
        // --greedy includes auto-updating casks that would otherwise be skipped
        ExecResult brewResult = CommandUtil::execWithStatus(
            brewPath, {"outdated", "--greedy", "--json"}, 30000);

        if (brewResult.exitCode == 0 && !brewResult.output.trimmed().isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(brewResult.output.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonArray formulae = root.value("formulae").toArray();
                QJsonArray casks = root.value("casks").toArray();

                for (const QJsonValue &val : formulae) {
                    QJsonObject obj = val.toObject();
                    UpdateEntry entry;
                    entry.source = "brew";
                    entry.name = obj.value("name").toString();
                    QJsonArray versions = obj.value("installed_versions").toArray();
                    entry.installedVersion = versions.isEmpty() ? QString() : versions.first().toString();
                    entry.version = obj.value("current_version").toString();
                    entry.isCask = false;
                    result.entries.append(entry);
                }

                for (const QJsonValue &val : casks) {
                    QJsonObject obj = val.toObject();
                    UpdateEntry entry;
                    entry.source = "brew";
                    entry.name = obj.value("name").toString();
                    // cask installed_versions is a string, not an array
                    entry.installedVersion = obj.value("installed_versions").toString();
                    entry.version = obj.value("current_version").toString();
                    entry.isCask = true;
                    result.entries.append(entry);
                }
            }
        }
    }

    // macOS system updates via softwareupdate
    ExecResult swResult = CommandUtil::execWithStatus(
        "softwareupdate", {"-l"}, 60000);

    if (swResult.exitCode == 0) {
        static const QRegularExpression labelRe(R"(^\s*\*)");
        const QStringList lines = swResult.output.split('\n');
        for (const QString &line : lines) {
            if (labelRe.match(line).hasMatch()) {
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
    }

    // Sparkle appcast updates for non-Homebrew .app bundles
    {
        SparkleUpdateScanner scanner;
        const QList<UpdateEntry> sparkleEntries = scanner.scan();
        result.entries.append(sparkleEntries);
    }

    result.totalCount = result.entries.size();
    return result;
}

QStringList UpdateInfoMacOS::availableSources() const
{
    QStringList sources;
    sources << "system";
    if (!findBrew().isEmpty())
        sources << "brew";
    sources << "sparkle";
    return sources;
}
