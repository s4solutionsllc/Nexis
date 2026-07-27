#include "sparkle_update_scanner.h"

#include "sparkle_appcast_parser.h"
#include <Utils/command_util.h>
#include <Utils/plist_util.h>
#include <Utils/brew_util.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>
#include <QDebug>

static constexpr int kFeedTimeoutMs = 10000; // 10 s per feed

// Build the set of app names managed by Homebrew casks so we can exclude them.
static QSet<QString> installedCaskAppNames()
{
    const QString brewPath = findBrew();
    if (brewPath.isEmpty())
        return {};

    ExecResult r = CommandUtil::execWithStatus(brewPath, {"info", "--json=v2", "--installed"}, 30000);
    if (r.exitCode != 0 || r.output.trimmed().isEmpty())
        return {};

    QSet<QString> names;
    const QList<BrewEntry> entries = parseBrewJson(QJsonDocument::fromJson(r.output.toUtf8()));
    for (const BrewEntry &e : entries) {
        if (e.isCask)
            names.insert(e.displayName.toLower());
    }
    return names;
}

QByteArray SparkleUpdateScanner::fetchFeed(const QString &feedUrl)
{
    // SSO-17776 AC2: reject any non-https feed URL before issuing a request
    // — no fallback, no silent downgrade. A hostile/misconfigured SUFeedURL
    // (plain http, or any other scheme) must never reach the network layer.
    if (QUrl(feedUrl).scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
        qWarning() << "sparkle_scanner: rejecting non-https feed URL:" << feedUrl;
        return {};
    }

    // Must be called from a non-GUI thread.  We spin a local event loop to
    // drive the async QNetworkAccessManager without blocking the GUI thread.
    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(feedUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Nexis/" + qApp->applicationVersion() + " Sparkle-Scanner/1");

    QNetworkReply *reply = nam.get(request);

    // Enforce per-feed timeout and size limit.
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(kFeedTimeoutMs);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&loop, reply]() {
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start();
    loop.exec();
    timeoutTimer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "sparkle_scanner: feed fetch failed for" << feedUrl
                 << ":" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    // Read with size cap to prevent denial-of-service.
    QByteArray data;
    while (!reply->atEnd()) {
        QByteArray chunk = reply->read(65536);
        data.append(chunk);
        if (data.size() > SparkleAppcastParser::kMaxFeedBytes) {
            qWarning() << "sparkle_scanner: feed too large, aborting:" << feedUrl;
            reply->abort();
            reply->deleteLater();
            return {};
        }
    }

    reply->deleteLater();
    return data;
}

QList<UpdateEntry> SparkleUpdateScanner::scan(const QStringList &homebrewAppNames) const
{
    QSet<QString> brewNames;
    for (const QString &n : homebrewAppNames)
        brewNames.insert(n.toLower());

    // Merge with dynamically detected Homebrew cask names.
    brewNames.unite(installedCaskAppNames());

    // Search /Applications then ~/Applications
    QStringList searchDirs = {"/Applications"};
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty())
        searchDirs.append(home + "/Applications");

    QList<UpdateEntry> results;

    for (const QString &dir : searchDirs) {
        const QStringList apps = QDir(dir).entryList({"*.app"}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &appDirName : apps) {
            const QString appPath = dir + "/" + appDirName;
            const QString appBaseName = appDirName.chopped(4); // strip ".app"

            if (brewNames.contains(appBaseName.toLower()))
                continue;

            PlistUtil::AppBundleInfo bundleInfo = PlistUtil::readAppBundleInfo(appPath);
            if (bundleInfo.suFeedUrl.isEmpty())
                continue; // silently excluded per AC

            const QByteArray feedData = fetchFeed(bundleInfo.suFeedUrl);
            if (feedData.isEmpty())
                continue; // network/timeout failure — skip this app, don't crash

            const SparkleAppcastParser::SparkleAppcastResult parsed =
                SparkleAppcastParser::parse(feedData);
            if (!parsed.ok)
                continue; // malformed feed — skip, don't surface an error

            const SparkleAppcastParser::EnclosureInfo *latest =
                SparkleAppcastParser::latestEnclosure(parsed);
            if (!latest || latest->url.isEmpty())
                continue;

            // Only surface the app if the feed offers a newer version.
            const QVersionNumber installedVer =
                QVersionNumber::fromString(bundleInfo.version);
            const QVersionNumber availableVer =
                QVersionNumber::fromString(latest->version);
            if (availableVer <= installedVer)
                continue;

            UpdateEntry entry;
            entry.source        = "sparkle";
            entry.name          = bundleInfo.displayName.isEmpty() ? appBaseName
                                                                    : bundleInfo.displayName;
            entry.version       = latest->version;
            entry.installedVersion = bundleInfo.version;
            entry.enclosureUrl  = latest->url;
            entry.edSignature   = latest->edSignature;
            entry.dsaSignature  = latest->dsaSignature;
            entry.publicKey     = bundleInfo.suPublicEDKey;
            entry.bundleId      = bundleInfo.bundleId;
            // Metadata presence only — not a cryptographic verification.
            // SparkleSignatureVerifier is not yet invoked on downloaded bytes
            // (see SSO-15431); the UI must not present this as "verified".
            entry.signatureMetadataPresent = latest->signaturePresent()
                                             && !bundleInfo.suPublicEDKey.isEmpty();
            results.append(entry);
        }
    }

    return results;
}
