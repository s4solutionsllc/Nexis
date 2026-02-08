#include "apt_source_tool.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QDebug>
#include <QRegularExpression>

#ifdef Q_OS_LINUX

bool AptSourceTool::checkSourceRepository()
{
    QDir sourceList(APT_SOURCES_LIST_D_PATH);

    bool isExists = sourceList.exists();

    return isExists;
}

void AptSourceTool::removeAPTSource(const APTSourcePtr aptSource)
{
    changeSource(aptSource, "");
}

void AptSourceTool::addRepository(const QString &repository, const bool isSource)
{
    if (! repository.isEmpty()) {
        QStringList args = { "-y", repository };
        if (isSource) {
            args << "-s";
        }

        CommandUtil::sudoExec("add-apt-repository", args);
    }
}

void AptSourceTool::changeSource(const APTSourcePtr aptSource, const QString newSource)
{
    QStringList sourceFileContent = FileUtil::readListFromFile(aptSource->filePath);

    // find line index
    int pos = -1;
    for (int i = 0; i < sourceFileContent.count(); ++i) {
        int _pos = sourceFileContent[i].indexOf(aptSource->source);
        if (_pos != -1) {
            pos = i;
            break;
        }
    }

    if (pos != -1) {
        if (newSource.isEmpty()) {
            sourceFileContent.removeAt(pos);
        } else {
            sourceFileContent.replace(pos, newSource);
        }

        QStringList args = { aptSource->filePath };

        QByteArray data = sourceFileContent.join('\n').append('\n').toUtf8();

        CommandUtil::sudoExec("tee", args, data);
    }
}

void AptSourceTool::changeStatus(const APTSourcePtr aptSource, const bool status)
{
    QString newSource = aptSource->source;

    newSource.replace("#", "");

    if (! status) { // if is deactive
        newSource = "# " + newSource.trimmed();
    }

    changeSource(aptSource, newSource);
}

QList<APTSourcePtr> AptSourceTool::getSourceList()
{
    QList<APTSourcePtr> aptSourceList;

    QFileInfoList infoList = QDir(APT_SOURCES_LIST_D_PATH).entryInfoList({"*.list"}, QDir::Files, QDir::Time);
    infoList.append(QFileInfo(APT_SOURCES_LIST_PATH)); // sources.list

    // example "deb [arch=amd64] http://packages.microsoft.com/repos/vscode stable main"
    for (const QFileInfo &info : infoList) {

        QStringList fileContent = FileUtil::readListFromFile(info.absoluteFilePath()).filter(QRegularExpression("^\\s{0,}#{0,}\\s{0,}deb"));

        for (const QString &line : fileContent) {
            QString _line = line.trimmed();

            APTSourcePtr aptSource(new APTSource);
            aptSource->filePath = info.absoluteFilePath();

            aptSource->isActive = ! _line.startsWith(QChar('#'));

            _line.remove('#'); // remove comment

            // if has options
            QRegularExpression regexOption("(\\s[\\[]+.*[\\]]+)");
            QRegularExpressionMatch optMatch = regexOption.match(_line);
            if (optMatch.hasMatch()) {
                aptSource->options = optMatch.captured().trimmed();
            }
            // remove options
            _line.remove(regexOption);

            QStringList sourceColumns = _line.trimmed().split(QRegularExpression("\\s+"));
            bool isBinary = sourceColumns.first() == "deb";
            bool isSource = sourceColumns.first() == "deb-src";

            if ((isBinary || isSource) && sourceColumns.count() > 2) {
                aptSource->isSource = isSource;
                aptSource->uri = sourceColumns.at(1);
                aptSource->distribution = sourceColumns.at(2);
                aptSource->components = sourceColumns.mid(3).join(' ');

                aptSource->source = line.trimmed().remove('#').trimmed();

                aptSourceList.append(aptSource);
            }
        }
    }

    return aptSourceList;
}

#elif defined(Q_OS_MACOS)

// On macOS, APT sources don't exist. These are replaced by Homebrew taps.
// We reuse the APTSource structure but map it to Homebrew tap concepts.

bool AptSourceTool::checkSourceRepository()
{
    // Check if Homebrew is installed
    return CommandUtil::isExecutable("brew");
}

void AptSourceTool::removeAPTSource(const APTSourcePtr aptSource)
{
    // Untap a Homebrew tap
    if (!aptSource->uri.isEmpty()) {
        try {
            CommandUtil::exec("brew", {"untap", aptSource->uri});
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
            CommandUtil::exec("brew", {"tap", repository});
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
        QString output = CommandUtil::exec("brew", {"tap"}).trimmed();
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

#endif
