#ifndef SPARKLE_UPDATE_DOWNLOADER_H
#define SPARKLE_UPDATE_DOWNLOADER_H

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include "nexis-core_global.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// SSO-17776 §3 / §11: downloads a Sparkle enclosure into memory for the new
// verify-then-write-then-exec path. Deliberately NOT built on the blocking
// QEventLoop-around-QNetworkAccessManager shape SparkleUpdateScanner::
// fetchFeed uses (the design doc's implementer constraints explicitly say
// not to copy that shape here) — this class is fully async via
// QNetworkAccessManager's own signals, so the GUI thread it normally lives
// on is never blocked, and cancel() can be called at any time to abort an
// in-flight download.
class NEXISCORESHARED_EXPORT SparkleUpdateDownloader : public QObject
{
    Q_OBJECT
public:
    // Enforced against bytes actually received as they stream in, not the
    // advertised Content-Length or the appcast's <enclosure length="...">
    // — a hostile host cannot bypass the cap by lying about size up front.
    static constexpr qint64 kMaxEnclosureBytes = 500LL * 1024 * 1024; // 500 MiB
    static constexpr int kStallTimeoutMs = 30000;        // 30 s with zero new bytes
    static constexpr int kTotalTimeoutMs = 10 * 60 * 1000; // 10 minute ceiling

    struct Result {
        bool ok = false;
        bool cancelled = false;
        QByteArray data;
        QString error;
    };

    explicit SparkleUpdateDownloader(QObject *parent = nullptr);
    ~SparkleUpdateDownloader() override;

    // https-only, checked synchronously before any request is issued: a
    // non-https url fails immediately via a synchronous finished() emission.
    // Connect finished()/progress() before calling start().
    void start(const QString &url);

    // Safe to call at any time (including before a reply exists, or after
    // completion — a no-op in both cases). Idempotent.
    void cancel();

signals:
    void progress(qint64 bytesReceived);
    void finished(SparkleUpdateDownloader::Result result);

private slots:
    void onReadyRead();
    void onNetFinished();
    void onStallTimeout();
    void onTotalTimeout();

private:
    void finish(Result result);
    void resetStallTimer();

    QNetworkAccessManager *mNam = nullptr;
    QPointer<QNetworkReply> mReply;
    QTimer *mStallTimer = nullptr;
    QTimer *mTotalTimer = nullptr;
    QByteArray mBuffer;
    bool mFinished = false;
    bool mCancelled = false;
};

#endif // SPARKLE_UPDATE_DOWNLOADER_H
