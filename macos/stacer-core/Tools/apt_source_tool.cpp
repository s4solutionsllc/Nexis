#include "apt_source_tool.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

// On macOS, APT sources don't exist. These are replaced by Homebrew taps.
// We reuse the APTSource structure but map it to Homebrew tap concepts.

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
    // Untap a Homebrew tap
    if (!aptSource->uri.isEmpty()) {
        try {
            CommandUtil::exec(findBrew(), {"untap", aptSource->uri});
        } catch (const QString &ex) {
            qCritical() << "Failed to untap:" << ex;
        }
    }
}

void AptSourceTool::addRepository(const QString &repository, const bool isSource)
{
    Q_UNUSED(isSource);
    if (!repository.isEmpty()) {
        try {
            CommandUtil::exec(findBrew(), {"tap", repository});
        } catch (const QString &ex) {
            qCritical() << "Failed to tap:" << ex;
        }
    }
}

void AptSourceTool::changeSource(const APTSourcePtr aptSource, const QString newSource)
{
    Q_UNUSED(aptSource);
    Q_UNUSED(newSource);
    // Not applicable for Homebrew taps
}

void AptSourceTool::changeStatus(const APTSourcePtr aptSource, const bool status)
{
    Q_UNUSED(aptSource);
    Q_UNUSED(status);
    // Not applicable for Homebrew taps (they're either tapped or not)
}

QList<APTSourcePtr> AptSourceTool::getSourceList()
{
    QList<APTSourcePtr> sourceList;

    try {
        QString output = CommandUtil::exec(findBrew(), {"tap"}).trimmed();
        QStringList taps = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &tap : taps) {
            APTSourcePtr source(new APTSource);
            source->uri = tap.trimmed();
            source->isActive = true;
            source->isSource = false;
            source->source = tap.trimmed();
            source->distribution = "homebrew";
            source->filePath = "";
            sourceList.append(source);
        }
    } catch (const QString &ex) {
        qCritical() << "Failed to list taps:" << ex;
    }

    return sourceList;
}
