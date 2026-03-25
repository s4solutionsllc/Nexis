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

static QString generateDeb822(const APTSourcePtr &source, const QString &signedByPath)
{
    QString content;
    content += "Types: deb\n";
    content += "URIs: " + source->uri + "\n";
    content += "Suites: " + source->suites + "\n";
    content += "Components: " + source->components + "\n";
    if (!signedByPath.isEmpty())
        content += "Signed-By: " + signedByPath + "\n";
    return content;
}

static QString domainToFilename(const QString &uri)
{
    QUrl url(uri);
    QString domain = url.host();
    if (domain.isEmpty()) {
        QRegularExpression re("://([^/]+)");
        auto match = re.match(uri);
        domain = match.hasMatch() ? match.captured(1) : "unknown";
    }
    return domain.replace('.', '-').replace(':', '-');
}

static bool downloadGpgKey(const QString &url, const QString &destPath)
{
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setTransferTimeout(10000);
    QNetworkReply *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(11000, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return false;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    // Validate: PGP armored key or binary keyring (binary keys are typically > 100 bytes)
    if (!data.startsWith("-----BEGIN PGP") && data.size() < 100)
        return false;

    QFile f(destPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    f.close();
    return true;
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::convertToDeb822(const APTSourcePtr &source)
{
    // Step 1: Resolve GPG key
    QString signedByPath = source->signedByPath;
    QString keyWarning;

    if (signedByPath.isEmpty()) {
        RepoKnownInfo info = RepoKnowledgeBase::lookup(source->uri);
        QString filename = domainToFilename(source->uri) + "-archive-keyring.gpg";
        QString keyDest = "/usr/share/keyrings/" + filename;

        bool keyDownloaded = false;
        if (!info.keyUrl.isEmpty()) {
            QTemporaryFile tmpKey;
            tmpKey.setAutoRemove(false);
            if (tmpKey.open() && downloadGpgKey(info.keyUrl, tmpKey.fileName())) {
                if (writeFileElevated(tmpKey.fileName(), keyDest))
                    keyDownloaded = true;
                QFile::remove(tmpKey.fileName());
            }
        }

        if (!keyDownloaded) {
            // Auto-discover common key paths
            QStringList tryPaths = {"/gpg.key", "/key.gpg", "/signing-key.asc",
                                    "/gpg", "/Release.key", "/apt-key.gpg"};
            QString baseUri = source->uri;
            if (!baseUri.endsWith('/')) baseUri += '/';

            QUrl url(source->uri);
            QString domainBase = url.scheme() + "://" + url.host();

            for (const QString &path : tryPaths) {
                QTemporaryFile tmpKey;
                tmpKey.setAutoRemove(false);
                if (tmpKey.open()) {
                    if (downloadGpgKey(baseUri + path, tmpKey.fileName()) ||
                        downloadGpgKey(domainBase + path, tmpKey.fileName())) {
                        if (writeFileElevated(tmpKey.fileName(), keyDest)) {
                            keyDownloaded = true;
                            QFile::remove(tmpKey.fileName());
                            break;
                        }
                    }
                    QFile::remove(tmpKey.fileName());
                }
            }
        }

        if (keyDownloaded) {
            signedByPath = keyDest;
        } else {
            keyWarning = QObject::tr(" (Warning: GPG key could not be downloaded — Signed-By not set)");
        }
    }

    // Step 2: Generate deb822 content
    QString deb822Content = generateDeb822(source, signedByPath);

    // Step 3: Write .sources file
    QString srcDir = sourcesDir();
    QString sourcesName = domainToFilename(source->uri) + ".sources";
    QString sourcesPath = srcDir + sourcesName;

    for (int i = 1; QFile::exists(sourcesPath) && i <= 10; ++i)
        sourcesPath = srcDir + domainToFilename(source->uri) + "-" + QString::number(i) + ".sources";

    if (QFile::exists(sourcesPath))
        return {false, {}, QObject::tr("Could not find available filename in %1").arg(srcDir)};

    QTemporaryFile tmpSources;
    tmpSources.setAutoRemove(false);
    if (!tmpSources.open())
        return {false, {}, QObject::tr("Cannot create temporary file")};
    tmpSources.write(deb822Content.toUtf8());
    tmpSources.close();

    if (!writeFileElevated(tmpSources.fileName(), sourcesPath)) {
        QFile::remove(tmpSources.fileName());
        return {false, {}, QObject::tr("Failed to write %1").arg(sourcesPath)};
    }
    QFile::remove(tmpSources.fileName());

    // Step 4: Comment out old .list entry
    auto commentResult = modifySourceFile(source, [](const QString &line) {
        return "# Converted to deb822 by Nexis\n# " + line;
    }, {});

    if (!commentResult.success)
        return {false, {}, QObject::tr("Wrote %1 but failed to comment out old entry: %2")
            .arg(sourcesPath, commentResult.errorDetail)};

    return {true, QObject::tr("Converted to deb822 format: %1%2")
        .arg(QFileInfo(sourcesPath).fileName(), keyWarning), {}};
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::removeDuplicate(const APTSourcePtr &source)
{
    QFile file(source->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, QObject::tr("Cannot read %1").arg(source->filePath)};

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    backupFile(source->filePath);

    QString pattern = buildMatchPattern(source);
    QStringList lines = content.split('\n');
    bool firstSeen = false;
    bool commented = false;

    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == pattern) {
            if (!firstSeen) {
                firstSeen = true;
            } else {
                lines.insert(i, "# Disabled by Nexis: duplicate entry");
                lines[i + 1] = "# " + lines[i + 1];
                commented = true;
                break;
            }
        }
    }

    if (!commented)
        return {false, {}, QObject::tr("No duplicate entry found to remove")};

    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return {false, {}, QObject::tr("Cannot create temporary file")};
    tmp.write(lines.join('\n').toUtf8());
    tmp.close();

    if (!writeFileElevated(tmp.fileName(), source->filePath)) {
        QFile::remove(tmp.fileName());
        return {false, {}, QObject::tr("Failed to write %1").arg(source->filePath)};
    }
    QFile::remove(tmp.fileName());

    return {true, QObject::tr("Duplicate entry commented out"), {}};
}

void RepoRepairEngineLinux::diagnoseConnection(const APTSourcePtr &)
{
}
