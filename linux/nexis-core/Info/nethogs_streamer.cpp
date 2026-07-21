#include "nethogs_streamer.h"

#include "Utils/command_util.h"

#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>

NethogsStreamer::NethogsStreamer(QObject *parent)
    : QObject(parent)
{
}

NethogsStreamer::~NethogsStreamer()
{
    stop();
}

void NethogsStreamer::start(int intervalSec)
{
    if (mProcess && mProcess->state() != QProcess::NotRunning)
        return;

    if (!CommandUtil::isExecutable("nethogs")) {
        mStatus = Status::ToolMissing;
        return;
    }

    mEverSampled = false;

    if (!mProcess) {
        mProcess = new QProcess(this);
        mProcess->setProcessChannelMode(QProcess::SeparateChannels);
        connect(mProcess, &QProcess::readyReadStandardOutput,
                this, &NethogsStreamer::onReadyRead);
        connect(mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &NethogsStreamer::onFinished);
    }

    // -t = tracemode (machine-parseable, one refresh cycle per line group).
    // -d<sec> = refresh delay. Requires CAP_NET_RAW/CAP_NET_ADMIN or root to
    // open a raw capture socket; without it nethogs typically prints an
    // error and exits within the first cycle — surfaced via onFinished().
    mProcess->start("nethogs", {"-t", "-d", QString::number(intervalSec)});

    if (!mProcess->waitForStarted(3000)) {
        qWarning() << "NethogsStreamer: failed to start nethogs";
        delete mProcess;
        mProcess = nullptr;
        mStatus = Status::ToolMissing;
        return;
    }

    mStatus = Status::Running;
}

void NethogsStreamer::stop()
{
    if (!mProcess)
        return;

    mProcess->disconnect(this);   // suppress onFinished's status update below
    mProcess->kill();
    mProcess->waitForFinished(1000);
    delete mProcess;
    mProcess = nullptr;
    mStatus = Status::NotStarted;

    QMutexLocker lock(&mMutex);
    mLatest.clear();
    mLineBuffer.clear();
}

bool NethogsStreamer::isRunning() const
{
    return mProcess && mProcess->state() == QProcess::Running;
}

NethogsStreamer::Status NethogsStreamer::status() const
{
    return mStatus;
}

QHash<pid_t, QPair<double, double>> NethogsStreamer::snapshot() const
{
    QMutexLocker lock(&mMutex);
    return mLatest;
}

void NethogsStreamer::pruneDeadPids(const QSet<pid_t> &alivePids)
{
    QMutexLocker lock(&mMutex);
    for (auto it = mLatest.begin(); it != mLatest.end(); ) {
        if (!alivePids.contains(it.key()))
            it = mLatest.erase(it);
        else
            ++it;
    }
}

void NethogsStreamer::onReadyRead()
{
    if (!mProcess)
        return;

    const QString chunk = QString::fromUtf8(mProcess->readAllStandardOutput());
    mLineBuffer += chunk;

    int idx;
    while ((idx = mLineBuffer.indexOf('\n')) >= 0) {
        const QString line = mLineBuffer.left(idx);
        mLineBuffer.remove(0, idx + 1);

        pid_t pid = 0;
        double sentKBps = 0.0;
        double receivedKBps = 0.0;
        if (parseLine(line.trimmed(), &pid, &sentKBps, &receivedKBps)) {
            QMutexLocker lock(&mMutex);
            // nethogs reports KB/s (1024 bytes) — convert to bytes/s for
            // FormatUtil::formatBytes() consistency with disk-I/O rates.
            mLatest.insert(pid, qMakePair(sentKBps * 1024.0, receivedKBps * 1024.0));
            mEverSampled = true;
        }
    }
}

bool NethogsStreamer::parseLine(const QString &line,
                                pid_t *pid,
                                double *sentKBps,
                                double *receivedKBps)
{
    if (line.isEmpty())
        return false;

    // Skip the "Refreshing:" cycle marker and any other non-data line.
    // nethogs tracemode rows are tab-separated:
    //   <program>/<pid>/<uid>\t<sent KB/s>\t<received KB/s>
    // The program field may itself contain '/' (a path), so split from the
    // right: the last two '/'-delimited segments are uid and pid.
    QStringList fields = line.split('\t');
    if (fields.size() < 3) {
        // Tolerate a build that separates with runs of whitespace instead.
        fields = line.split(QRegularExpression("\\s+"));
        if (fields.size() < 3)
            return false;
    }

    const QString procField = fields.at(0);
    const QStringList procParts = procField.split('/');
    if (procParts.size() < 3)
        return false;

    bool pidOk = false;
    const pid_t parsedPid = procParts.at(procParts.size() - 2).toLongLong(&pidOk);
    if (!pidOk || parsedPid <= 0)
        return false;

    bool sentOk = false;
    bool recvOk = false;
    const double sent = fields.at(fields.size() - 2).trimmed().toDouble(&sentOk);
    const double recv = fields.at(fields.size() - 1).trimmed().toDouble(&recvOk);
    if (!sentOk || !recvOk)
        return false;

    if (pid)           *pid = parsedPid;
    if (sentKBps)       *sentKBps = sent;
    if (receivedKBps)   *receivedKBps = recv;
    return true;
}

void NethogsStreamer::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    if (!mProcess)
        return;

    // Exited on its own (not via stop()) without ever producing a sample —
    // almost always CAP_NET_RAW/CAP_NET_ADMIN missing. No retry: unlike
    // NvidiaSmiStreamer's driver-reload case, a permission failure won't
    // resolve itself on a second attempt.
    if (!mEverSampled)
        mStatus = Status::ExitedImmediately;

    delete mProcess;
    mProcess = nullptr;
}
