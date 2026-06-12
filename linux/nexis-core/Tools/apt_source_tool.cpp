#include "apt_source_tool_linux.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QDebug>
#include <QRegularExpression>
#include <QUrl>

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

bool AptSourceToolLinux::isAvailable()
{
    QDir sourceList(APT_SOURCES_LIST_D_PATH);
    return sourceList.exists();
}

QList<RepositoryPtr> AptSourceToolLinux::listRepositories()
{
    QList<RepositoryPtr> result;
    for (const APTSourcePtr &src : getSourceList()) {
        RepositoryPtr repo(new Repository);
        repo->kind = Repository::Kind::AptSource;
        repo->id = src->source;
        repo->displayName = src->source;
        repo->description = src->components;
        repo->isActive = src->isActive;
        result.append(repo);
    }
    return result;
}

void AptSourceToolLinux::removeRepository(const RepositoryPtr &repo)
{
    if (repo.isNull())
        return;
    // Resolve the matching APTSource by stable id (the raw source line)
    // and delegate to the APT-specific implementation.
    for (const APTSourcePtr &src : getSourceList()) {
        if (src->source == repo->id) {
            removeAPTSource(src);
            return;
        }
    }
}

void AptSourceToolLinux::removeAPTSource(const APTSourcePtr aptSource)
{
    if (isAptRpm() && CommandUtil::isExecutable("apt-repo")) {
        CommandUtil::sudoExec("apt-repo", {"rm", aptSource->source});
    } else {
        changeSource(aptSource, nullptr);
    }
}

bool AptSourceToolLinux::prefersDeb822() const
{
    // SSO-3728 / FW-01: on Ubuntu 26.04+ and Debian trixie+, the stock distro
    // source ships as deb822. Detect by file presence rather than parsing
    // /etc/os-release so unit-style overlays work transparently.
    QDir d(APT_SOURCES_LIST_D_PATH);
    return d.exists("ubuntu.sources") || d.exists("debian.sources");
}

void AptSourceToolLinux::addRepositoryDeb822(const QString &fileStem,
                                             const APTSourcePtr &source)
{
    if (source.isNull() || fileStem.isEmpty())
        return;

    // Sanitize fileStem to a safe basename — no path separators, no leading
    // dots, lowercase ascii alnum/dash/underscore only.
    QString safe;
    for (QChar c : fileStem) {
        if (c.isLetterOrNumber() || c == '-' || c == '_')
            safe.append(c.toLower());
        else if (c == '.')
            safe.append('-');
    }
    if (safe.isEmpty() || safe.startsWith('-') || safe.startsWith('_'))
        return;

    QString filePath = QString("%1/%2.sources").arg(APT_SOURCES_LIST_D_PATH, safe);
    QString stanza = AptSourceTool::buildDeb822Stanza(source, binaryType(), sourceType());
    if (stanza.trimmed().isEmpty())
        return;

    CommandUtil::sudoExec("tee", { filePath }, stanza.toUtf8());
}

void AptSourceToolLinux::addRepository(const QString &repository, const bool isSource)
{
    if (repository.isEmpty())
        return;

    if (isAptRpm() && CommandUtil::isExecutable("apt-repo")) {
        QString source = repository;
        if (isSource && !source.startsWith(sourceType())) {
            source.replace(QRegularExpression("^" + binaryType() + "\\s"),
                           sourceType() + " ");
        }
        CommandUtil::sudoExec("apt-repo", {"add", source});
        return;
    }

    // SSO-3728 / FW-01: on systems where deb822 is the norm, route structured
    // "deb [signed-by=/path] uri suite components" specs through the deb822
    // writer so new sources land as .sources with an explicit Signed-By, not
    // legacy .list files that would later need a one-shot convert. Anything
    // we can't parse (ppa:..., PPA short-forms, malformed lines) falls through
    // to add-apt-repository as before — that path still handles key fetch for
    // ppa: URIs via launchpad.
    if (prefersDeb822() && !repository.startsWith("ppa:")) {
        APTSourcePtr parsed = parseSourceListLine(repository, binaryType(), sourceType());
        if (!parsed.isNull() && !parsed->signedByPath.isEmpty()
            && !parsed->uri.isEmpty() && !parsed->suites.isEmpty()) {
            parsed->isSource = isSource || parsed->isSource;
            parsed->format = APTSource::Deb822;
            parsed->isActive = true;
            QString host = QUrl(parsed->uri).host();
            QString stem = host.isEmpty() ? QString("nexis-added") : host;
            addRepositoryDeb822(stem, parsed);
            return;
        }
    }

    QStringList args = { "-y", repository };
    if (isSource)
        args << "-s";
    CommandUtil::sudoExec("add-apt-repository", args);
}

void AptSourceToolLinux::changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource)
{
    if (aptSource->filePath.endsWith(".sources")) {
        // SSO-3728 / FW-01: deb822 stanza rewriting delegates to the shared
        // serializer so the same byte-stable preservation logic is exercised
        // by the unit tests. We split the file into stanzas (blank-line
        // separated), call serializeDeb822Stanza on each, then re-join.
        QStringList sourceFileContent = FileUtil::readListFromFile(aptSource->filePath);
        QStringList updatedContent;
        QString entry;

        auto processEntry = [&](const QString &entry) {
            QString out = serializeDeb822Stanza(entry, aptSource, newSource,
                                                binaryType(), sourceType());
            if (out.isEmpty())
                return; // stanza dropped
            updatedContent.append(out);
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
