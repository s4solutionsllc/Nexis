#include "apt_source_tool.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include "Utils/brew_util.h"
#include <QDebug>
#include <QRegularExpression>

// On macOS, APT sources don't exist. This adapter maps the APTSource
// interface to Homebrew installed packages (formulae + casks).

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
        QString jsonOutput = CommandUtil::exec(findBrew(), {"info", "--json=v2", "--installed"}).trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(jsonOutput.toUtf8());

        if (doc.isNull()) {
            qCritical() << "Failed to parse brew info JSON";
            return sourceList;
        }

        for (const BrewEntry &e : parseBrewJson(doc)) {
            APTSourcePtr source(new APTSource);
            source->uri          = e.identifier;
            source->isActive     = true;
            source->isSource     = e.isCask;
            source->distribution = e.isCask ? "cask" : "formula";
            source->filePath     = "";
            source->source       = e.displayName;
            source->components   = e.description;

            sourceList.append(source);
        }
    } catch (const QString &ex) {
        qCritical() << "Failed to list packages:" << ex;
    }

    return sourceList;
}
