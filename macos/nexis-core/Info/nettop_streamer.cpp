#include "nettop_streamer.h"

#include <QDebug>
#include <QMutexLocker>

NettopStreamer::NettopStreamer(QObject *parent)
    : QObject(parent)
{
}

NettopStreamer::~NettopStreamer()
{
    stop();
}

void NettopStreamer::start(int intervalSec)
{
    if (mProcess && mProcess->state() != QProcess::NotRunning)
        return;

    if (!mProcess) {
        mProcess = new QProcess(this);
        mProcess->setProcessChannelMode(QProcess::SeparateChannels);
        connect(mProcess, &QProcess::readyReadStandardOutput,
                this, &NettopStreamer::onReadyRead);
    }

    // -P = per-process, -d = delta mode, -s<sec> = sample interval.
    // -J selects the columns we parse. -t external trims out localhost traffic.
    mProcess->start("nettop",
                    {"-P", "-d", "-s", QString::number(intervalSec),
                     "-J", "bytes_in,bytes_out", "-t", "external"});
    if (!mProcess->waitForStarted(3000)) {
        qWarning() << "NettopStreamer: failed to start nettop";
        delete mProcess;
        mProcess = nullptr;
    }
}

void NettopStreamer::stop()
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

bool NettopStreamer::isRunning() const
{
    return mProcess && mProcess->state() == QProcess::Running;
}

QHash<pid_t, QPair<quint64, quint64>> NettopStreamer::snapshot() const
{
    QMutexLocker lock(&mMutex);
    return mLatest;
}

void NettopStreamer::pruneDeadPids(const QSet<pid_t> &alivePids)
{
    QMutexLocker lock(&mMutex);
    for (auto it = mLatest.begin(); it != mLatest.end(); ) {
        if (!alivePids.contains(it.key()))
            it = mLatest.erase(it);
        else
            ++it;
    }
}

void NettopStreamer::onReadyRead()
{
    if (!mProcess)
        return;

    const QString chunk = QString::fromUtf8(mProcess->readAllStandardOutput());
    mLineBuffer += chunk;

    // Split on newline; keep the tail (possibly partial) for next round.
    int idx;
    while ((idx = mLineBuffer.indexOf('\n')) >= 0) {
        QString line = mLineBuffer.left(idx);
        mLineBuffer.remove(0, idx + 1);
        parseLine(line.trimmed());
    }
}

bool NettopStreamer::parseCsvLine(const QString &line,
                                  pid_t *pid,
                                  quint64 *bytesIn,
                                  quint64 *bytesOut)
{
    if (line.isEmpty())
        return false;

    // nettop line format with our options looks like:
    //   time,,bytes_in,bytes_out   (periodic header we skip)
    //   procname.PID,,N,M          (per-process)
    // First field is "[procname].[pid]" — split on the LAST '.'.
    const QStringList parts = line.split(',');
    if (parts.size() < 4)
        return false;

    const QString procField = parts.at(0);
    const int lastDot = procField.lastIndexOf('.');
    if (lastDot <= 0)
        return false;

    bool ok = false;
    const pid_t parsedPid = procField.mid(lastDot + 1).toLongLong(&ok);
    if (!ok || parsedPid <= 0)
        return false;

    // Bytes fields may be at index 1+2 or 2+3 depending on nettop version —
    // probe both. We want the two numeric columns that parse as ULL.
    auto tryParsePair = [](const QString &a, const QString &b, quint64 *in, quint64 *out) -> bool {
        bool okA = false;
        bool okB = false;
        const quint64 va = a.trimmed().toULongLong(&okA);
        const quint64 vb = b.trimmed().toULongLong(&okB);
        if (okA && okB) {
            *in = va;
            *out = vb;
            return true;
        }
        return false;
    };

    quint64 parsedIn = 0;
    quint64 parsedOut = 0;
    bool haveBytes = false;
    if (parts.size() >= 4)
        haveBytes = tryParsePair(parts.at(1), parts.at(2), &parsedIn, &parsedOut);
    if (!haveBytes && parts.size() >= 5)
        haveBytes = tryParsePair(parts.at(2), parts.at(3), &parsedIn, &parsedOut);

    if (!haveBytes)
        return false;

    if (pid)      *pid = parsedPid;
    if (bytesIn)  *bytesIn = parsedIn;
    if (bytesOut) *bytesOut = parsedOut;
    return true;
}

void NettopStreamer::parseLine(const QString &line)
{
    pid_t pid = 0;
    quint64 bytesIn = 0;
    quint64 bytesOut = 0;
    if (!parseCsvLine(line, &pid, &bytesIn, &bytesOut))
        return;

    QMutexLocker lock(&mMutex);
    mLatest.insert(pid, qMakePair(bytesIn, bytesOut));
}
