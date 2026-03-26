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

static QString httpHeadCheck(const QString &url, int timeoutMs = 5000)
{
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = nam.head(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs + 500, &loop, &QEventLoop::quit);
    loop.exec();

    QString error;
    if (reply->error() != QNetworkReply::NoError)
        error = reply->errorString();

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (error.isEmpty() && status >= 400)
        error = QString("HTTP %1").arg(status);

    reply->deleteLater();
    return error;
}

void RepoRepairEngineLinux::diagnoseConnection(const APTSourcePtr &source)
{
    DiagnoseResult result;
    QUrl url(source->uri);
    QString domain = url.host();

    // Step 1: DNS Resolution
    {
        DiagnoseStep step;
        step.check = QObject::tr("DNS Resolution");
        QDnsLookup dns;
        dns.setType(QDnsLookup::A);
        dns.setName(domain);
        dns.lookup();

        QEventLoop loop;
        QObject::connect(&dns, &QDnsLookup::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();

        if (dns.error() == QDnsLookup::NoError && !dns.hostAddressRecords().isEmpty()) {
            step.status = DiagnoseStep::Ok;
            step.detail = QObject::tr("Resolves to %1").arg(dns.hostAddressRecords().first().value().toString());
        } else if (dns.error() == QDnsLookup::NotFoundError) {
            step.status = DiagnoseStep::Failed;
            step.detail = QObject::tr("Domain does not exist (NXDOMAIN)");
            result.steps.append(step);
            result.suggestion = QObject::tr("This domain no longer exists. The repository may have been discontinued.");
            emit diagnoseFinished(result);
            return;
        } else {
            step.status = DiagnoseStep::Failed;
            step.detail = QObject::tr("DNS lookup failed: %1").arg(dns.errorString());
            result.steps.append(step);
            result.suggestion = QObject::tr("DNS resolution failed. Check your network connection.");
            emit diagnoseFinished(result);
            return;
        }
        result.steps.append(step);
    }

    // Step 2: Base domain
    {
        DiagnoseStep step;
        step.check = QObject::tr("Base Domain");
        QString baseUrl = url.scheme() + "://" + domain;
        QString err = httpHeadCheck(baseUrl);
        if (err.isEmpty()) {
            step.status = DiagnoseStep::Ok;
            step.detail = QObject::tr("Base domain is reachable");
            result.suggestion = QObject::tr("The server is up but the repository path may have changed. Try opening in a browser to check.");
        } else {
            step.status = DiagnoseStep::Failed;
            step.detail = QObject::tr("Base domain unreachable: %1").arg(err);
            result.suggestion = QObject::tr("The entire server is unreachable. It may be down or blocked.");
        }
        result.steps.append(step);
    }

    // Step 3: Protocol check
    {
        DiagnoseStep step;
        step.check = QObject::tr("Protocol Check");
        QString altScheme = (url.scheme() == "https") ? "http" : "https";
        QString altUrl = altScheme + "://" + domain + url.path();
        QString err = httpHeadCheck(altUrl);
        if (err.isEmpty()) {
            step.status = DiagnoseStep::Warning;
            step.detail = QObject::tr("Available over %1 instead").arg(altScheme.toUpper());
            result.suggestion = QObject::tr("This repository is available over %1. Consider updating the source URL.").arg(altScheme.toUpper());
        } else {
            step.status = DiagnoseStep::Ok;
            step.detail = QObject::tr("Not available over %1 either").arg(altScheme.toUpper());
        }
        result.steps.append(step);
    }

    // Step 4: Knowledge base
    {
        RepoKnownInfo info = RepoKnowledgeBase::lookup(source->uri);
        if (!info.url.isEmpty() && info.url != source->uri) {
            DiagnoseStep step;
            step.check = QObject::tr("Known Repository");
            step.status = DiagnoseStep::Warning;
            step.detail = QObject::tr("Canonical URL: %1").arg(info.url);
            result.suggestion = QObject::tr("The canonical URL for %1 is %2. Your source may be outdated.")
                .arg(info.name, info.url);
            result.steps.append(step);
        }
    }

    // Build follow-up actions
    {
        RepoRepairAction openBrowser;
        openBrowser.type = RepoRepairAction::RunCommand;
        openBrowser.label = QObject::tr("Open in Browser");
        openBrowser.command = "xdg-open " + source->uri;
        result.followUpActions.append(openBrowser);

        RepoRepairAction askClaude;
        askClaude.type = RepoRepairAction::AskClaude;
        askClaude.label = QObject::tr("Ask Claude.ai");
        {
            // Build contextual prompt from diagnose results
            ExecResult distroResult = CommandUtil::execWithStatus("lsb_release", {"-ds"});
            QString distro = distroResult.exitCode == 0 ? distroResult.output.trimmed() : "Linux";

            QString sourceLine = QString("deb %1 %2 %3").arg(source->uri, source->suites, source->components).trimmed();

            QString prompt = QString("I'm running %1 and having an issue with an APT repository.\n\n"
                                     "Repository: %2\n"
                                     "File: %3\n\n"
                                     "Diagnostic results:\n")
                .arg(distro, sourceLine, source->filePath);

            for (const DiagnoseStep &step : result.steps) {
                QString status;
                switch (step.status) {
                case DiagnoseStep::Ok:      status = "OK"; break;
                case DiagnoseStep::Warning: status = "Warning"; break;
                case DiagnoseStep::Failed:  status = "FAILED"; break;
                default:                    status = "Unknown"; break;
                }
                prompt += QString("- %1: %2 — %3\n").arg(step.check, status, step.detail);
            }

            if (!result.suggestion.isEmpty())
                prompt += QString("\nSuggestion: %1\n").arg(result.suggestion);

            prompt += "\nHow do I fix this?";

            askClaude.context["prompt"] = prompt;
        }
        result.followUpActions.append(askClaude);

        RepoRepairAction disable;
        disable.type = RepoRepairAction::DisableSource;
        disable.label = QObject::tr("Disable Repository");
        result.followUpActions.append(disable);
    }

    emit diagnoseFinished(result);
}
