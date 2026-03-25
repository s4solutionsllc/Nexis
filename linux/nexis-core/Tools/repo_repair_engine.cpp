#include "repo_repair_engine_linux.h"
#include "repo_knowledge_base.h"
#include "Utils/command_util.h"
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDnsLookup>
#include <QUrl>
#include <QFileInfo>

QString RepoRepairEngineLinux::buildMatchPattern(const APTSourcePtr &source) const
{
    return QString("deb %1 %2 %3").arg(source->uri, source->suites, source->components);
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::modifySourceFile(
    const APTSourcePtr &source,
    std::function<QString(const QString &line)> lineTransform,
    const QString &successMsg)
{
    QFile file(source->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, QObject::tr("Cannot read %1").arg(source->filePath)};

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    backupFile(source->filePath);

    QStringList lines = content.split('\n');
    bool found = false;

    if (source->format == APTSource::Deb822) {
        QString result;
        QString stanza;
        bool inMatchingStanza = false;

        for (const QString &line : lines) {
            if (line.trimmed().isEmpty() && !stanza.isEmpty()) {
                if (inMatchingStanza && !found) {
                    stanza = lineTransform(stanza);
                    found = true;
                }
                result += stanza + "\n";
                stanza.clear();
                inMatchingStanza = false;
                continue;
            }
            stanza += line + "\n";
            if (line.startsWith("URIs:") && line.contains(source->uri))
                inMatchingStanza = true;
        }
        if (!stanza.isEmpty()) {
            if (inMatchingStanza && !found) {
                stanza = lineTransform(stanza);
                found = true;
            }
            result += stanza;
        }
        content = result;
    } else {
        QString pattern = buildMatchPattern(source);
        for (int i = 0; i < lines.size(); ++i) {
            QString stripped = lines[i].trimmed();
            if (stripped == pattern || stripped == "# " + pattern ||
                stripped == "#" + pattern) {
                QString transformed = lineTransform(lines[i]);
                if (transformed.isEmpty()) {
                    lines.removeAt(i);
                } else {
                    lines[i] = transformed;
                }
                found = true;
                break;
            }
        }
        content = lines.join('\n');
    }

    if (!found)
        return {false, {}, QObject::tr("Could not find matching entry in %1").arg(source->filePath)};

    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return {false, {}, QObject::tr("Cannot create temporary file")};
    tmp.write(content.toUtf8());
    tmp.close();

    if (!writeFileElevated(tmp.fileName(), source->filePath)) {
        QFile::remove(tmp.fileName());
        return {false, {}, QObject::tr("Failed to write %1 (pkexec denied or failed)").arg(source->filePath)};
    }
    QFile::remove(tmp.fileName());

    return {true, successMsg, {}};
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::disableSource(const APTSourcePtr &source)
{
    if (source->format == APTSource::Deb822) {
        return modifySourceFile(source, [](const QString &stanza) {
            if (stanza.contains(QRegularExpression("Enabled:\\s*yes", QRegularExpression::CaseInsensitiveOption)))
                return QString(stanza).replace(QRegularExpression("Enabled:\\s*yes", QRegularExpression::CaseInsensitiveOption), "Enabled: no");
            return "Enabled: no\n" + stanza;
        }, QObject::tr("Repository disabled"));
    }

    return modifySourceFile(source, [](const QString &line) {
        QString stripped = line.trimmed();
        if (!stripped.startsWith('#'))
            return "# " + line;
        return line;
    }, QObject::tr("Repository disabled"));
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::enableSource(const APTSourcePtr &source)
{
    if (source->format == APTSource::Deb822) {
        return modifySourceFile(source, [](const QString &stanza) {
            QString result = stanza;
            result.remove(QRegularExpression("Enabled:\\s*no\\n?", QRegularExpression::CaseInsensitiveOption));
            return result;
        }, QObject::tr("Repository enabled"));
    }

    return modifySourceFile(source, [](const QString &line) {
        QString stripped = line.trimmed();
        if (stripped.startsWith("# "))
            return stripped.mid(2);
        if (stripped.startsWith("#"))
            return stripped.mid(1);
        return line;
    }, QObject::tr("Repository enabled"));
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::removeSource(const APTSourcePtr &source)
{
    if (source->isActive)
        return {false, QObject::tr("Cannot remove an active repository. Disable it first."), {}};

    QFile file(source->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, QObject::tr("Cannot read %1").arg(source->filePath)};

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    backupFile(source->filePath);

    if (source->format == APTSource::Legacy) {
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        int debLineCount = 0;
        for (const QString &line : lines) {
            QString stripped = line.trimmed();
            if (stripped.startsWith("deb ") || stripped.startsWith("# deb ") || stripped.startsWith("#deb "))
                debLineCount++;
        }
        if (debLineCount <= 1) {
            if (!removeFileElevated(source->filePath))
                return {false, {}, QObject::tr("Failed to delete %1").arg(source->filePath)};
            return {true, QObject::tr("Repository file deleted: %1").arg(QFileInfo(source->filePath).fileName()), {}};
        }
    }

    return modifySourceFile(source, [](const QString &) {
        return QString();
    }, QObject::tr("Repository entry removed"));
}

// Stubs — implemented in subsequent tasks
RepoRepairEngine::RepairResult RepoRepairEngineLinux::convertToDeb822(const APTSourcePtr &)
{
    return {false, QObject::tr("Not yet implemented"), {}};
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::removeDuplicate(const APTSourcePtr &)
{
    return {false, QObject::tr("Not yet implemented"), {}};
}

void RepoRepairEngineLinux::diagnoseConnection(const APTSourcePtr &)
{
}
