#include "apt_source_tool.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

// On macOS, APT sources don't exist. This adapter maps the APTSource
// interface to Homebrew installed packages (formulae + casks).

// Homebrew binary path — checked at well-known locations since GUI apps
// don't inherit the user's shell PATH.
static QString findBrew()
{
    for (const QString &path : {"/opt/homebrew/bin/brew", "/usr/local/bin/brew"}) {
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return QString();
}

bool AptSourceTool::checkSourceRepository()
{
    return !findBrew().isEmpty();
}

void AptSourceTool::removeAPTSource(const APTSourcePtr aptSource)
{
    // Uninstall a Homebrew package
    if (!aptSource->uri.isEmpty()) {
        try {
            QString type = aptSource->distribution;  // "formula" or "cask"
            if (type == "cask") {
                CommandUtil::exec(findBrew(), {"uninstall", "--cask", aptSource->uri});
            } else {
                CommandUtil::exec(findBrew(), {"uninstall", aptSource->uri});
            }
        } catch (const QString &ex) {
            qCritical() << "Failed to uninstall:" << ex;
        }
    }
}

void AptSourceTool::addRepository(const QString &repository, const bool isSource)
{
    Q_UNUSED(isSource);
    if (!repository.isEmpty()) {
        try {
            CommandUtil::exec(findBrew(), {"install", repository});
        } catch (const QString &ex) {
            qCritical() << "Failed to install:" << ex;
        }
    }
}

void AptSourceTool::changeSource(const APTSourcePtr aptSource, const QString newSource)
{
    Q_UNUSED(aptSource);
    Q_UNUSED(newSource);
    // Not applicable for Homebrew packages
}

void AptSourceTool::changeStatus(const APTSourcePtr aptSource, const bool status)
{
    Q_UNUSED(aptSource);
    Q_UNUSED(status);
    // Not applicable for Homebrew packages (they're either installed or not)
}

QList<APTSourcePtr> AptSourceTool::getSourceList()
{
    QList<APTSourcePtr> sourceList;

    try {
        // Single call to get metadata (name + description) for all installed packages
        QString jsonOutput = CommandUtil::exec(findBrew(), {"info", "--json=v2", "--installed"}).trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(jsonOutput.toUtf8());

        if (doc.isNull()) {
            qCritical() << "Failed to parse brew info JSON";
            return sourceList;
        }

        QJsonObject root = doc.object();

        // Installed formulae
        QJsonArray formulae = root.value("formulae").toArray();
        for (const QJsonValue &val : formulae) {
            QJsonObject obj = val.toObject();
            APTSourcePtr source(new APTSource);
            source->uri          = obj.value("name").toString();
            source->isActive     = true;
            source->isSource     = false;
            source->distribution = "formula";
            source->filePath     = "";

            // "source" stores the display name (formula name is already readable)
            source->source = obj.value("name").toString();
            // "components" stores the description
            source->components = obj.value("desc").toString();

            sourceList.append(source);
        }

        // Installed casks
        QJsonArray casks = root.value("casks").toArray();
        for (const QJsonValue &val : casks) {
            QJsonObject obj = val.toObject();
            APTSourcePtr source(new APTSource);
            source->uri          = obj.value("token").toString();   // e.g. "alt-tab"
            source->isActive     = true;
            source->isSource     = true;    // reuse to flag cask packages
            source->distribution = "cask";
            source->filePath     = "";

            // Cask "name" is the human-friendly name (e.g. "AltTab")
            QJsonArray names = obj.value("name").toArray();
            source->source = names.isEmpty() ? source->uri : names.first().toString();
            // "components" stores the description
            source->components = obj.value("desc").toString();

            sourceList.append(source);
        }
    } catch (const QString &ex) {
        qCritical() << "Failed to list packages:" << ex;
    }

    return sourceList;
}
