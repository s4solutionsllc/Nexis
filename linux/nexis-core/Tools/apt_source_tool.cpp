#include "apt_source_tool_linux.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QDebug>
#include <QRegularExpression>

static constexpr const char *APT_SOURCES_LIST_D_PATH = "/etc/apt/sources.list.d";
static constexpr const char *APT_SOURCES_LIST_PATH   = "/etc/apt/sources.list";

static bool isAptRpm()
{
    return CommandUtil::isExecutable("apt-get")
        && CommandUtil::isExecutable("rpm")
        && !CommandUtil::isExecutable("dpkg");
}

static QString binaryType()
{
    return isAptRpm() ? "rpm" : "deb";
}

static QString sourceType()
{
    return isAptRpm() ? "rpm-src" : "deb-src";
}

bool AptSourceToolLinux::checkSourceRepository()
{
    QDir sourceList(APT_SOURCES_LIST_D_PATH);
    return sourceList.exists();
}

void AptSourceToolLinux::removeAPTSource(const APTSourcePtr aptSource)
{
    if (isAptRpm() && CommandUtil::isExecutable("apt-repo")) {
        CommandUtil::sudoExec("apt-repo", {"rm", aptSource->source});
    } else {
        changeSource(aptSource, nullptr);
    }
}

void AptSourceToolLinux::addRepository(const QString &repository, const bool isSource)
{
    if (!repository.isEmpty()) {
        if (isAptRpm() && CommandUtil::isExecutable("apt-repo")) {
            QString source = repository;
            if (isSource && !source.startsWith(sourceType())) {
                source.replace(QRegularExpression("^" + binaryType() + "\\s"), sourceType() + " ");
            }
            CommandUtil::sudoExec("apt-repo", {"add", source});
        } else {
            QStringList args = { "-y", repository };
            if (isSource)
                args << "-s";
            CommandUtil::sudoExec("add-apt-repository", args);
        }
    }
}

void AptSourceToolLinux::changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource)
{
    if (aptSource->filePath.endsWith(".sources")) {
        // deb822 format: stanza-aware rewriting
        QStringList sourceFileContent = FileUtil::readListFromFile(aptSource->filePath);
        QStringList updatedContent;
        QString entry;

        auto processEntry = [&](const QString &entry) {
            QStringList entryLines = entry.split('\n');
            QMap<QString, QString> fields;
            QList<QPair<int, QString>> commentLines;

            for (int idx = 0; idx < entryLines.size(); ++idx) {
                const QString &entryLine = entryLines[idx];
                if (entryLine.trimmed().startsWith('#')) {
                    commentLines.append(qMakePair(idx, entryLine));
                    continue;
                }
                int sep = entryLine.indexOf(':');
                if (sep > 0) {
                    QString key = entryLine.left(sep).trimmed();
                    QString value = entryLine.mid(sep + 1).trimmed();
                    fields[key] = value;
                }
            }

            // Match expanded entries against multi-suite stanzas:
            // check URI matches and the entry's single suite is within
            // the stanza's Suites field
            bool stanzaMatches = (fields.value("URIs") == aptSource->uri)
                && fields.value("Suites").split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)
                       .contains(aptSource->suites);

            if (stanzaMatches) {
                if (!newSource) {
                    return; // skip = remove stanza
                }
                // Update fields from newSource
                fields["Types"] = newSource->isSource ? sourceType() : binaryType();
                fields["URIs"] = newSource->uri.trimmed();
                fields["Suites"] = newSource->suites.trimmed();
                fields["Components"] = newSource->components.trimmed();
                if (!newSource->isActive)
                    fields["Enabled"] = "no";
                else
                    fields.remove("Enabled");
            }

            // Reconstruct entry preserving field order, multi-line Signed-By, and comments
            QSet<QString> handledFields;
            QStringList newEntryLines(entryLines.size(), QString());

            for (const auto &pair : commentLines)
                newEntryLines[pair.first] = pair.second;

            for (int i = 0; i < entryLines.size(); ++i) {
                if (!newEntryLines[i].isEmpty())
                    continue; // comment already placed
                QString line = entryLines[i];
                int sep = line.indexOf(':');
                if (sep > 0) {
                    QString key = line.left(sep).trimmed();
                    if (key == "Signed-By" && fields.contains("Signed-By")) {
                        // Preserve multi-line Signed-By (embedded GPG keys)
                        bool found = false;
                        for (int j = i; j < entryLines.size(); ++j) {
                            const QString &l = entryLines[j];
                            if (l.startsWith("Signed-By:")) {
                                found = true;
                                newEntryLines[j] = l;
                                handledFields.insert("Signed-By");
                            } else if (found) {
                                if (l.trimmed().isEmpty() || l.contains(':'))
                                    break;
                                newEntryLines[j] = l; // continuation line
                            }
                        }
                        while (i + 1 < entryLines.size()
                               && !entryLines[i + 1].trimmed().isEmpty()
                               && !entryLines[i + 1].contains(':')) {
                            ++i;
                        }
                    } else if (fields.contains(key)) {
                        newEntryLines[i] = QString("%1: %2").arg(key).arg(fields.value(key));
                        handledFields.insert(key);
                    }
                }
            }

            // Append new fields not in original order
            for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
                if (!handledFields.contains(it.key())) {
                    if (it.key() == "Signed-By")
                        continue;
                    bool placed = false;
                    for (int i = 0; i < newEntryLines.size(); ++i) {
                        if (newEntryLines[i].isEmpty()) {
                            newEntryLines[i] = QString("%1: %2").arg(it.key()).arg(it.value());
                            placed = true;
                            break;
                        }
                    }
                    if (!placed)
                        newEntryLines << QString("%1: %2").arg(it.key()).arg(it.value());
                }
            }

            while (!newEntryLines.isEmpty() && newEntryLines.last().isEmpty())
                newEntryLines.removeLast();

            updatedContent.append(newEntryLines.join('\n'));
            updatedContent.append(""); // blank line separator between stanzas
        };

        for (const QString &line : sourceFileContent) {
            if (line.trimmed().isEmpty()) {
                if (!entry.isEmpty()) {
                    processEntry(entry);
                    entry.clear();
                }
            } else {
                entry += line + '\n';
            }
        }
        if (!entry.isEmpty())
            processEntry(entry);

        QStringList args = { aptSource->filePath };
        QByteArray data = updatedContent.join('\n').append('\n').toUtf8();

        if (data.trimmed().isEmpty()) {
            CommandUtil::sudoExec("rm", args);
            return;
        }
        CommandUtil::sudoExec("tee", args, data);

    } else if (aptSource->filePath.endsWith(".list")) {
        // Legacy .list format: line-based rewriting
        QStringList sourceFileContent = FileUtil::readListFromFile(aptSource->filePath);

        int pos = -1;
        for (int i = 0; i < sourceFileContent.count(); ++i) {
            if (sourceFileContent[i].indexOf(aptSource->source) != -1) {
                pos = i;
                break;
            }
        }

        if (pos != -1) {
            if (!newSource) {
                sourceFileContent.removeAt(pos);
            } else {
                QString line = newSource->isSource ? sourceType() : binaryType();
                if (!newSource->options.isEmpty())
                    line += " " + newSource->options;
                line += " " + newSource->uri + " " + newSource->suites;
                if (!newSource->components.isEmpty())
                    line += " " + newSource->components;
                if (!newSource->isActive)
                    line = "# " + line;
                sourceFileContent.replace(pos, line);
            }

            QStringList args = { aptSource->filePath };
            QByteArray data = sourceFileContent.join('\n').append('\n').toUtf8();

            if (data.trimmed().isEmpty()) {
                CommandUtil::sudoExec("rm", args);
                return;
            }
            CommandUtil::sudoExec("tee", args, data);
        }
    }
}

void AptSourceToolLinux::changeStatus(const APTSourcePtr aptSource, const bool status)
{
    APTSourcePtr newSource(new APTSource(*aptSource));
    newSource->isActive = status;
    changeSource(aptSource, newSource);
}

QList<APTSourcePtr> AptSourceToolLinux::getSourceList()
{
    QList<APTSourcePtr> aptSourceList;

    QFileInfoList infoList = QDir(APT_SOURCES_LIST_D_PATH).entryInfoList({"*.list"}, QDir::Files, QDir::Time);
    infoList.append(QFileInfo(APT_SOURCES_LIST_PATH));
    QFileInfoList deb822List = QDir(APT_SOURCES_LIST_D_PATH).entryInfoList({"*.sources"}, QDir::Files, QDir::Time);
    infoList.append(deb822List);

    for (const QFileInfo &info : infoList) {
        if (info.fileName().endsWith(".sources")) {
            QStringList fileContent = FileUtil::readListFromFile(info.absoluteFilePath());
            QString entry;

            auto processEntry = [&](const QString &entry) {
                QList<APTSourcePtr> sources = parseDeb822Stanza(entry, binaryType(), sourceType());
                for (const APTSourcePtr &src : sources) {
                    src->filePath = info.absoluteFilePath();
                    aptSourceList.append(src);
                }
            };

            for (const QString &line : fileContent) {
                if (line.trimmed().isEmpty()) {
                    if (!entry.isEmpty()) {
                        processEntry(entry);
                        entry.clear();
                    }
                } else {
                    entry += line + '\n';
                }
            }
            if (!entry.isEmpty())
                processEntry(entry);

        } else if (info.fileName().endsWith(".list")) {
            QStringList fileContent = FileUtil::readListFromFile(info.absoluteFilePath())
                .filter(QRegularExpression("^\\s{0,}#{0,}\\s{0,}" + binaryType()));

            for (const QString &line : fileContent) {
                APTSourcePtr src = parseSourceListLine(line, binaryType(), sourceType());
                if (src) {
                    src->filePath = info.absoluteFilePath();
                    aptSourceList.append(src);
                }
            }
        }
    }

    return aptSourceList;
}
