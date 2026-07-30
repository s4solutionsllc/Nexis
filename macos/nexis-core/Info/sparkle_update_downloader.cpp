#include "sparkle_update_downloader.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QDebug>

SparkleUpdateDownloader::SparkleUpdateDownloader(QObject *parent)
    : QObject(parent)
    , mNam(new QNetworkAccessManager(this))
{
    mStallTimer = new QTimer(this);
    mStallTimer->setSingleShot(true);
    mStallTimer->setInterval(kStallTimeoutMs);
    connect(mStallTimer, &QTimer::timeout, this, &SparkleUpdateDownloader::onStallTimeout);

    mTotalTimer = new QTimer(this);
    mTotalTimer->setSingleShot(true);
    mTotalTimer->setInterval(kTotalTimeoutMs);
    connect(mTotalTimer, &QTimer::timeout, this, &SparkleUpdateDownloader::onTotalTimeout);
}

SparkleUpdateDownloader::~SparkleUpdateDownloader()
{
    cancel();
}

void SparkleUpdateDownloader::start(const QString &url)
{
    mFinished = false;
    mCancelled = false;
    mBuffer.clear();

    // AC2 / design §3: https-only, checked before any request is issued —
    // no fallback, no silent downgrade, no file://\data:\other scheme.
    if (QUrl(url).scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
        finish({false, false, {}, QStringLiteral("enclosure URL is not https")});
        return;
    }

    QNetworkRequest request{QUrl(url)};
    // https→http downgrade redirects are rejected by this policy (Qt docs);
    // reused from SparkleUpdateScanner::fetchFeed for consistency.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Nexis/" + qApp->applicationVersion() + " Sparkle-Downloader/1");

    mReply = mNam->get(request);
    connect(mReply, &QNetworkReply::readyRead, this, &SparkleUpdateDownloader::onReadyRead);
    connect(mReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 /*total*/) { emit progress(received); });
    connect(mReply, &QNetworkReply::finished, this, &SparkleUpdateDownloader::onNetFinished);

    resetStallTimer();
    mTotalTimer->start();
}

void SparkleUpdateDownloader::cancel()
{
    if (mFinished)
        return;
    mCancelled = true;
    if (mReply)
        mReply->abort(); // triggers onNetFinished(), which reports cancelled=true
}

void SparkleUpdateDownloader::onReadyRead()
{
    if (!mReply)
        return;
    mBuffer.append(mReply->readAll());
    if (mBuffer.size() > kMaxEnclosureBytes) {
        qWarning() << "sparkle_downloader: enclosure exceeds size cap, aborting";
        mReply->abort(); // triggers onNetFinished(); mBuffer is discarded there
        return;
    }
    resetStallTimer();
}

void SparkleUpdateDownloader::onNetFinished()
{
    if (mFinished || !mReply)
        return;

    mStallTimer->stop();
    mTotalTimer->stop();

    if (mCancelled) {
        finish({false, true, {}, QStringLiteral("cancelled")});
        return;
    }
    if (mBuffer.size() > kMaxEnclosureBytes) {
        finish({false, false, {}, QStringLiteral("enclosure exceeded the size cap")});
        return;
    }
    if (mReply->error() != QNetworkReply::NoError) {
        finish({false, false, {}, mReply->errorString()});
        return;
    }

    // Any remaining unread bytes (finished() can fire with data still
    // buffered in the reply).
    mBuffer.append(mReply->readAll());
    if (mBuffer.size() > kMaxEnclosureBytes) {
        finish({false, false, {}, QStringLiteral("enclosure exceeded the size cap")});
        return;
    }

    finish({true, false, mBuffer, {}});
}

void SparkleUpdateDownloader::onStallTimeout()
{
    if (mFinished || !mReply)
        return;
    qWarning() << "sparkle_downloader: stalled (no bytes for" << kStallTimeoutMs << "ms), aborting";
    mReply->abort();
}

void SparkleUpdateDownloader::onTotalTimeout()
{
    if (mFinished || !mReply)
        return;
    qWarning() << "sparkle_downloader: exceeded total timeout, aborting";
    mReply->abort();
}

void SparkleUpdateDownloader::resetStallTimer()
{
    mStallTimer->start();
}

void SparkleUpdateDownloader::finish(Result result)
{
    if (mFinished)
        return;
    mFinished = true;
    mStallTimer->stop();
    mTotalTimer->stop();
    mBuffer.clear();
    if (mReply) {
        mReply->disconnect(this);
        mReply->deleteLater();
    }
    emit finished(result);
}
