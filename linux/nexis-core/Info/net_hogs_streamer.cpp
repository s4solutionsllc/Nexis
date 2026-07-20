#include "net_hogs_streamer.h"

#include <QMutexLocker>
#include <QRegularExpression>

NetHogsStreamer::NetHogsStreamer(QObject *parent)
    : QObject(parent)
{
}

NetHogsStreamer::~NetHogsStreamer()
{
    stop();
}

void NetHogsStreamer::start(int intervalSec)
{
    if (mProcess && mProcess->state() != QProcess::NotRunning)
        return;

    mFailed = false;
    mLastError.clear();

    if (!mProcess) {
        mProcess = new QProcess(this);
        mProcess->setProcessChannelMode(QProcess::SeparateChannels);
        connect(mProcess, &QProcess::readyReadStandardOutput,
                this, &NetHogsStreamer::onReadyRead);
        connect(mProcess, &QProcess::errorOccurred,
                this, &NetHogsStreamer::onErrorOccurred);
        connect(mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &NetHogsStreamer::onFinished);
    }

    // -t = tracemode (parseable, non-interactive), -d = refresh delay
    // (seconds), -v0 = KB/s sort/display mode (the default, pinned explicitly
    // so the output shape doesn't depend on the host's nethogs config).
    mProcess->start("nethogs",
                    {"-t", "-d", QString::number(intervalSec), "-v0"});
    if (!mProcess->waitForStarted(3000)) {
        mFailed = true;
        mLastError = mProcess->errorString();
        delete mProcess;
        mProcess = nullptr;
    }
}

void NetHogsStreamer::stop()
{
    if (!mProcess)
        return;

    mProcess->kill();
    mProcess->waitForFinished(1000);
    delete mProcess;
    mProcess = nullptr;

    QMutexLocker lock(&mMutex);
    mLatest.clear();
    mLineBuffer.clear();
}

bool NetHogsStreamer::isRunning() const
{
    return mProcess && mProcess->state() == QProcess::Running;
}

bool NetHogsStreamer::hasFailed() const
{
    return mFailed;
}

QString NetHogsStreamer::lastError() const
{
    return mLastError;
}

QHash<pid_t, QPair<double, double>> NetHogsStreamer::snapshot() const
{
    QMutexLocker lock(&mMutex);
    return mLatest;
}

void NetHogsStreamer::pruneDeadPids(const QSet<pid_t> &alivePids)
{
    QMutexLocker lock(&mMutex);
    for (auto it = mLatest.begin(); it != mLatest.end(); ) {
        if (!alivePids.contains(it.key()))
            it = mLatest.erase(it);
        else
            ++it;
    }
}

void NetHogsStreamer::onReadyRead()
{
    if (!mProcess)
        return;

    const QString chunk = QString::fromUtf8(mProcess->readAllStandardOutput());
    mLineBuffer += chunk;

    int idx;
    while ((idx = mLineBuffer.indexOf('\n')) >= 0) {
        QString line = mLineBuffer.left(idx);
        mLineBuffer.remove(0, idx + 1);
        parseLine(line.trimmed());
    }
}

void NetHogsStreamer::onErrorOccurred(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        mFailed = true;
        mLastError = QStringLiteral("nethogs failed to start (not installed?)");
    }
}

void NetHogsStreamer::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        mFailed = true;
        const QString stderrText = mProcess ? QString::fromUtf8(mProcess->readAllStandardError()).trimmed() : QString();
        mLastError = stderrText.isEmpty()
            ? QStringLiteral("nethogs exited with code %1").arg(exitCode)
            : stderrText;
    }
}

bool NetHogsStreamer::parseTraceLine(const QString &line, pid_t *pid, double *sentBps, double *recvBps)
{
    if (line.isEmpty())
        return false;

    // Trace-mode ("-t") per-process rows look like:
    //   /usr/lib/firefox/firefox/12345/1000\t5.360\t120.334
    // i.e. "<program-path>/<pid>/<uid>" followed by sent/received KB/s.
    // "Refreshing:" separators and the "TOTAL ..." summary line have no '/'
    // in their first field and are rejected below.
    static const QRegularExpression whitespace("\\s+");
    const QStringList parts = line.split(whitespace, Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return false;

    const QStringList segments = parts.at(0).split(QLatin1Char('/'));
    if (segments.size() < 3)
        return false;

    bool pidOk = false;
    const pid_t parsedPid = segments.at(segments.size() - 2).toLongLong(&pidOk);
    if (!pidOk || parsedPid <= 0)
        return false;

    bool sentOk = false;
    bool recvOk = false;
    const double sentKBps = parts.at(1).toDouble(&sentOk);
    const double recvKBps = parts.at(2).toDouble(&recvOk);
    if (!sentOk || !recvOk)
        return false;

    if (pid)      *pid = parsedPid;
    if (sentBps)  *sentBps = sentKBps * 1024.0;
    if (recvBps)  *recvBps = recvKBps * 1024.0;
    return true;
}

void NetHogsStreamer::parseLine(const QString &line)
{
    pid_t pid = 0;
    double sentBps = 0;
    double recvBps = 0;
    if (!parseTraceLine(line, &pid, &sentBps, &recvBps))
        return;

    QMutexLocker lock(&mMutex);
    mLatest.insert(pid, qMakePair(recvBps, sentBps));
}
