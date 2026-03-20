#include "repo_health_checker_linux.h"
#include "repo_knowledge_base.h"
#include "Utils/command_util.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>

// --- Helpers ---

static QString systemCodename()
{
    static QString codename;
    if (codename.isEmpty()) {
        ExecResult r = CommandUtil::execWithStatus("lsb_release", {"-cs"});
        if (r.exitCode == 0)
            codename = r.output.trimmed();
    }
    return codename;
}

static QString httpHead(const QString &url, int timeoutMs = 5000)
{
    // Returns empty string on success, error string on failure
    QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
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

static QString httpGet(const QString &url, int timeoutMs = 5000)
{
    QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs + 500, &loop, &QEventLoop::quit);
    loop.exec();

    QString body;
    if (reply->error() == QNetworkReply::NoError)
        body = QString::fromUtf8(reply->readAll());

    reply->deleteLater();
    return body;
}

// --- RepoHealthCheckerLinux ---

RepoHealthResult::Status RepoHealthCheckerLinux::worstStatus(const QList<RepoHealthIssue> &issues)
{
    RepoHealthResult::Status worst = RepoHealthResult::Healthy;
    for (const auto &issue : issues) {
        if (issue.severity == RepoHealthIssue::Error)
            return RepoHealthResult::Error;
        if (issue.severity == RepoHealthIssue::Warning)
            worst = RepoHealthResult::Warning;
    }
    return worst;
}

RepoHealthCache RepoHealthCheckerLinux::checkAll(const QList<APTSourcePtr> &sources)
{
    RepoHealthCache cache;
    for (const APTSourcePtr &src : sources) {
        QString key = cacheKey(src);
        if (cache.contains(key))
            continue; // skip duplicate entries
        cache[key] = checkOne(src);
    }
    checkDuplicates(sources, cache);
    return cache;
}

RepoHealthResult RepoHealthCheckerLinux::checkOne(const APTSourcePtr &source)
{
    RepoHealthResult result;
    result.lastChecked = QDateTime::currentDateTime();

    checkConnection(source, result);

    // Only run deeper checks if connection succeeded
    bool connected = true;
    for (const auto &issue : result.issues) {
        if (issue.code == "connection_error") {
            connected = false;
            break;
        }
    }

    // Fetch Release file once — used by both resolveDescription and checkReleaseFile
    QString releaseContent;
    if (connected)
        releaseContent = fetchReleaseFile(source->uri, source->suites);

    resolveDescription(source, releaseContent, result);

    if (connected) {
        checkReleaseFile(source, releaseContent, result);
        checkGpgKey(source, result);
        checkSuiteMismatch(source, result);
    }

    checkDeprecatedFormat(source, result);

    result.status = result.issues.isEmpty()
        ? RepoHealthResult::Healthy
        : worstStatus(result.issues);

    return result;
}

void RepoHealthCheckerLinux::checkConnection(const APTSourcePtr &source, RepoHealthResult &result)
{
    QString error = httpHead(source->uri);
    if (!error.isEmpty()) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Error;
        issue.code = "connection_error";
        issue.summary = QObject::tr("Repository unreachable");
        issue.detail = QObject::tr("Could not connect to %1: %2")
            .arg(source->uri, error);
        result.issues.append(issue);
    }
}

void RepoHealthCheckerLinux::checkReleaseFile(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result)
{
    if (releaseContent.isEmpty()) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Error;
        issue.code = "release_404";
        issue.summary = QObject::tr("Release file not found");
        issue.detail = QObject::tr("No InRelease or Release file at %1/dists/%2/. "
                                    "This suite may not exist for this repository.")
            .arg(source->uri, source->suites);
        result.issues.append(issue);
    } else {
        // Parse Origin for metadata
        for (const QString &line : releaseContent.split('\n')) {
            if (line.startsWith("Origin:"))
                result.releaseOrigin = line.mid(7).trimmed();
        }
    }
}

QString RepoHealthCheckerLinux::fetchReleaseFile(const QString &uri, const QString &suite)
{
    // Try InRelease first, then Release
    QString base = uri;
    if (!base.endsWith('/'))
        base += '/';
    base += "dists/" + suite + "/";

    QString content = httpGet(base + "InRelease");
    if (content.isEmpty())
        content = httpGet(base + "Release");
    return content;
}

void RepoHealthCheckerLinux::checkGpgKey(const APTSourcePtr &source, RepoHealthResult &result)
{
    if (!source->signedByPath.isEmpty()) {
        // Check if the keyring file exists
        QFileInfo keyFile(source->signedByPath);
        if (!keyFile.exists()) {
            RepoHealthIssue issue;
            issue.severity = RepoHealthIssue::Error;
            issue.code = "gpg_missing";
            issue.summary = QObject::tr("Signing key file missing");
            issue.detail = QObject::tr("The keyring file %1 does not exist. "
                                        "Packages from this repository cannot be verified.")
                .arg(source->signedByPath);
            result.issues.append(issue);
            return;
        }

        // Check key expiry using gpg
        ExecResult gpgResult = CommandUtil::execWithStatus(
            "gpg", {"--no-default-keyring", "--keyring", source->signedByPath,
                     "--list-keys", "--with-colons"}, 10000);

        if (gpgResult.exitCode == 0) {
            // Parse expiry dates from gpg colon format
            // pub:...:expire_date:...
            for (const QString &line : gpgResult.output.split('\n')) {
                QStringList fields = line.split(':');
                if (fields.size() > 6 && (fields[0] == "pub" || fields[0] == "sub")) {
                    QString expStr = fields[6];
                    if (!expStr.isEmpty()) {
                        QDateTime expiry = QDateTime::fromSecsSinceEpoch(expStr.toLongLong());
                        QDateTime now = QDateTime::currentDateTime();
                        qint64 daysUntil = now.daysTo(expiry);

                        if (daysUntil < 0) {
                            RepoHealthIssue issue;
                            issue.severity = RepoHealthIssue::Error;
                            issue.code = "gpg_expired";
                            issue.summary = QObject::tr("GPG key expired");
                            issue.detail = QObject::tr("The signing key expired on %1.")
                                .arg(expiry.toString("yyyy-MM-dd"));
                            issue.repairLabel = QObject::tr("Refresh signing key");
                            issue.repairCmd = QString("gpg --no-default-keyring --keyring %1 --recv-keys --keyserver keyserver.ubuntu.com")
                                .arg(source->signedByPath);
                            result.issues.append(issue);
                        } else if (daysUntil < 30) {
                            RepoHealthIssue issue;
                            issue.severity = RepoHealthIssue::Warning;
                            issue.code = "gpg_expiring";
                            issue.summary = QObject::tr("GPG key expires in %1 days").arg(daysUntil);
                            issue.detail = QObject::tr("The signing key expires on %1. "
                                                        "Updates will fail after this date.")
                                .arg(expiry.toString("yyyy-MM-dd"));
                            issue.repairLabel = QObject::tr("Refresh signing key");
                            issue.repairCmd = QString("gpg --no-default-keyring --keyring %1 --recv-keys --keyserver keyserver.ubuntu.com")
                                .arg(source->signedByPath);
                            result.issues.append(issue);
                        }
                        break; // Only check first key
                    }
                }
            }
        }
    }
    // If no signed-by and apt-key exists, check via apt-key (legacy)
    // Skipped for now — deprecated format check covers this case
}

void RepoHealthCheckerLinux::checkSuiteMismatch(const APTSourcePtr &source, RepoHealthResult &result)
{
    QString codename = systemCodename();
    if (codename.isEmpty())
        return;

    // Split suites (could be "jammy jammy-updates") and check each
    QStringList suites = source->suites.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &suite : suites) {
        // Skip common suffixes that are valid for any release
        if (suite.endsWith("-updates") || suite.endsWith("-backports") || suite.endsWith("-security"))
            continue;
        // Skip non-codename suites like "stable", "testing", "sid"
        if (suite == "stable" || suite == "testing" || suite == "unstable" || suite == "sid")
            continue;

        if (suite != codename) {
            RepoHealthIssue issue;
            issue.severity = RepoHealthIssue::Warning;
            issue.code = "suite_mismatch";
            issue.summary = QObject::tr("Suite mismatch: %1 vs system %2").arg(suite, codename);
            issue.detail = QObject::tr("This repository targets '%1' but your system runs '%2'. "
                                        "Packages may be incompatible or unavailable.")
                .arg(suite, codename);
            result.issues.append(issue);
            break; // One warning is enough
        }
    }
}

void RepoHealthCheckerLinux::checkDeprecatedFormat(const APTSourcePtr &source, RepoHealthResult &result)
{
    if (source->format == APTSource::Legacy) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Info;
        issue.code = "legacy_format";
        issue.summary = QObject::tr("Legacy .list format");
        issue.detail = QObject::tr("This source uses the legacy one-line format. "
                                    "The modern deb822 (.sources) format is recommended.");
        result.issues.append(issue);
    }

    if (source->signedByPath.isEmpty() && source->format == APTSource::Legacy) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Warning;
        issue.code = "no_signed_by";
        issue.summary = QObject::tr("No signed-by key specified");
        issue.detail = QObject::tr("This source does not use the signed-by option. "
                                    "It may rely on deprecated apt-key for signature verification.");
        result.issues.append(issue);
    }
}

void RepoHealthCheckerLinux::checkDuplicates(const QList<APTSourcePtr> &sources, RepoHealthCache &cache)
{
    QMap<QString, int> seen; // normalized key -> count
    for (const APTSourcePtr &src : sources)
        seen[cacheKey(src)]++;

    for (const APTSourcePtr &src : sources) {
        QString key = cacheKey(src);
        if (seen[key] > 1) {
            if (cache.contains(key)) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "duplicate_source";
                issue.summary = QObject::tr("Duplicate source entry");
                issue.detail = QObject::tr("This repository is defined %1 times across source files. "
                                            "Duplicate entries can cause apt warnings.")
                    .arg(seen[key]);
                cache[key].issues.append(issue);
                cache[key].status = worstStatus(cache[key].issues);
            }
        }
    }
}

void RepoHealthCheckerLinux::resolveDescription(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result)
{
    // 1. Try knowledge base
    RepoKnownInfo known = RepoKnowledgeBase::lookup(source->uri);
    if (!known.name.isEmpty()) {
        result.name = known.name;
        result.description = known.description;
        return;
    }

    // 2. Try Release file metadata (pre-fetched, shared with checkReleaseFile)
    if (!releaseContent.isEmpty()) {
        for (const QString &line : releaseContent.split('\n')) {
            if (line.startsWith("Origin:") && result.name.isEmpty())
                result.name = line.mid(7).trimmed();
            if (line.startsWith("Description:") && result.description.isEmpty())
                result.description = line.mid(12).trimmed();
        }
        if (!result.name.isEmpty())
            return;
    }

    // 3. Fallback: domain from URI
    result.name = RepoKnowledgeBase::domainFromUri(source->uri);
}
