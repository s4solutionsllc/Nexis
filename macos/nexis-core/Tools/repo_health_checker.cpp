#include "repo_health_checker_macos.h"
#include "Utils/command_util.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

RepoHealthCache RepoHealthCheckerMac::checkAll(const QList<APTSourcePtr> &)
{
    // macOS doesn't use APTSource — use checkBrewPackages() instead
    return {};
}

RepoHealthResult RepoHealthCheckerMac::checkOne(const APTSourcePtr &)
{
    // Not used on macOS
    return {};
}

RepoHealthCache RepoHealthCheckerMac::checkBrewPackages(const QList<Package> &packages)
{
    RepoHealthCache cache;

    // Initialize all packages as Healthy
    for (const Package &pkg : packages) {
        RepoHealthResult result;
        result.status = RepoHealthResult::Healthy;
        result.name = pkg.name;
        result.description = pkg.description;
        result.lastChecked = QDateTime::currentDateTime();
        cache[pkg.name] = result;
    }

    checkOutdated(cache);
    checkDeprecated(cache);
    checkTaps(cache);
    checkPinned(cache);

    return cache;
}

void RepoHealthCheckerMac::checkOutdated(RepoHealthCache &cache)
{
    ExecResult r = CommandUtil::execWithStatus("brew", {"outdated", "--json=v2"}, 30000);
    if (r.exitCode != 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(r.output.toUtf8());
    if (!doc.isObject())
        return;

    auto processArray = [&](const QJsonArray &arr) {
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            if (cache.contains(name)) {
                QString currentVer = obj["installed_versions"].toArray().first().toString();
                QString latestVer;
                QJsonObject versions = obj["current_version"].toObject();
                if (versions.isEmpty())
                    latestVer = obj["current_version"].toString();
                else
                    latestVer = versions["stable"].toString();

                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "outdated";
                issue.summary = QObject::tr("Update available: %1 → %2").arg(currentVer, latestVer);
                issue.detail = QObject::tr("A newer version is available. "
                                            "Current: %1, Available: %2")
                    .arg(currentVer, latestVer);
                issue.repairLabel = QObject::tr("Update package");
                issue.repairCmd = QString("brew upgrade %1").arg(name);

                cache[name].issues.append(issue);
                cache[name].status = RepoHealthResult::Warning;
            }
        }
    };

    QJsonObject root = doc.object();
    processArray(root["formulae"].toArray());
    processArray(root["casks"].toArray());
}

void RepoHealthCheckerMac::checkDeprecated(RepoHealthCache &cache)
{
    ExecResult r = CommandUtil::execWithStatus("brew", {"info", "--json=v2", "--installed"}, 30000);
    if (r.exitCode != 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(r.output.toUtf8());
    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();

    auto processFormulae = [&](const QJsonArray &arr) {
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            if (!cache.contains(name))
                continue;

            if (obj["deprecated"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "deprecated";
                issue.summary = QObject::tr("Package deprecated");
                issue.detail = obj["deprecation_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This package has been deprecated and may be removed in a future Homebrew update.");
                cache[name].issues.append(issue);
                if (cache[name].status != RepoHealthResult::Error)
                    cache[name].status = RepoHealthResult::Warning;
            }

            if (obj["disabled"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Error;
                issue.code = "disabled";
                issue.summary = QObject::tr("Package disabled");
                issue.detail = obj["disable_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This package has been disabled and will not receive updates.");
                issue.repairLabel = QObject::tr("Uninstall package");
                issue.repairCmd = QString("brew uninstall %1").arg(name);
                cache[name].issues.append(issue);
                cache[name].status = RepoHealthResult::Error;
            }
        }
    };

    auto processCasks = [&](const QJsonArray &arr) {
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString token = obj["token"].toString();
            if (!cache.contains(token))
                continue;

            if (obj["deprecated"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "deprecated";
                issue.summary = QObject::tr("Cask deprecated");
                issue.detail = obj["deprecation_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This cask has been deprecated.");
                cache[token].issues.append(issue);
                if (cache[token].status != RepoHealthResult::Error)
                    cache[token].status = RepoHealthResult::Warning;
            }

            if (obj["disabled"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Error;
                issue.code = "disabled";
                issue.summary = QObject::tr("Cask disabled");
                issue.detail = obj["disable_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This cask has been disabled.");
                issue.repairLabel = QObject::tr("Uninstall cask");
                issue.repairCmd = QString("brew uninstall --cask %1").arg(token);
                cache[token].issues.append(issue);
                cache[token].status = RepoHealthResult::Error;
            }
        }
    };

    processFormulae(root["formulae"].toArray());
    processCasks(root["casks"].toArray());
}

void RepoHealthCheckerMac::checkTaps(RepoHealthCache &cache)
{
    ExecResult r = CommandUtil::execWithStatus("brew", {"tap-info", "--json=v2"}, 30000);
    if (r.exitCode != 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(r.output.toUtf8());
    if (!doc.isObject())
        return;

    QJsonArray taps = doc.object()["taps"].toArray();
    for (const QJsonValue &val : taps) {
        QJsonObject tap = val.toObject();
        QString remote = tap["remote"].toString();
        QString tapName = tap["name"].toString();
        if (remote.isEmpty())
            continue;

        // Use git ls-remote to test reachability of the tap's remote URL
        ExecResult gitResult = CommandUtil::execWithStatus(
            "git", {"ls-remote", "--exit-code", "--heads", remote, "HEAD"}, 5000);

        if (gitResult.exitCode != 0) {
            // Annotate every cached package that belongs to this tap
            for (auto it = cache.begin(); it != cache.end(); ++it) {
                QString pkgTap = it->name.contains('/') ? it->name.section('/', 0, 1) : QString();
                if (!pkgTap.isEmpty() && pkgTap != tapName)
                    continue;
                // Only annotate if we can associate; otherwise add a generic entry
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "tap_unreachable";
                issue.summary = QObject::tr("Tap unreachable: %1").arg(tapName);
                issue.detail = QObject::tr("The tap remote %1 could not be reached. "
                                            "Updates from this tap may fail.")
                    .arg(remote);
                it->issues.append(issue);
                if (it->status == RepoHealthResult::Healthy)
                    it->status = RepoHealthResult::Warning;
                break; // one annotation per tap is enough
            }
        }
    }
}

void RepoHealthCheckerMac::checkPinned(RepoHealthCache &cache)
{
    ExecResult r = CommandUtil::execWithStatus("brew", {"list", "--pinned"}, 10000);
    if (r.exitCode != 0)
        return;

    QStringList pinned = r.output.split('\n', Qt::SkipEmptyParts);
    for (const QString &pkg : pinned) {
        QString name = pkg.trimmed();
        if (!cache.contains(name))
            continue;

        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Info;
        issue.code = "pinned";
        issue.summary = QObject::tr("Package pinned");
        issue.detail = QObject::tr("This package is pinned and will not be upgraded by 'brew upgrade'.");
        cache[name].issues.append(issue);
    }
}
