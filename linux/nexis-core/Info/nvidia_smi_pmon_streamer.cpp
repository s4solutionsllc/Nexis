#include "nvidia_smi_pmon_streamer.h"

#include "Utils/command_util.h"

#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>

NvidiaSmiPmonStreamer::NvidiaSmiPmonStreamer(QObject *parent)
    : QObject(parent)
{
}

NvidiaSmiPmonStreamer::~NvidiaSmiPmonStreamer()
{
    stop();
}

void NvidiaSmiPmonStreamer::start()
{
    if (!CommandUtil::isExecutable("nvidia-smi"))
        return;

    if (!mPmon) {
        mPmon = new QProcess(this);
        mPmon->setProcessChannelMode(QProcess::SeparateChannels);
        connect(mPmon, &QProcess::readyReadStandardOutput,
                this, &NvidiaSmiPmonStreamer::onPmonReadyRead);
        connect(mPmon, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &NvidiaSmiPmonStreamer::onProcessFinished);
        mPmon->start("nvidia-smi",
                     {"pmon", "-d", "1", "-s", "u", "-c", "0"});
        if (!mPmon->waitForStarted(3000)) {
            qWarning() << "NvidiaSmiPmonStreamer: failed to start nvidia-smi pmon";
            delete mPmon;
            mPmon = nullptr;
        }
    }

    if (!mApps) {
        mApps = new QProcess(this);
        mApps->setProcessChannelMode(QProcess::SeparateChannels);
        connect(mApps, &QProcess::readyReadStandardOutput,
                this, &NvidiaSmiPmonStreamer::onComputeAppsReadyRead);
        connect(mApps, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &NvidiaSmiPmonStreamer::onProcessFinished);
        mApps->start("nvidia-smi",
                     {"--query-compute-apps=pid,used_memory",
                      "--format=csv,noheader,nounits", "-l", "1"});
        if (!mApps->waitForStarted(3000)) {
            qWarning() << "NvidiaSmiPmonStreamer: failed to start query-compute-apps";
            delete mApps;
            mApps = nullptr;
        }
    }
}

void NvidiaSmiPmonStreamer::stop()
{
    auto killProc = [](QProcess *&p) {
        if (!p) return;
        p->disconnect();
        p->kill();
        p->waitForFinished(1000);
        delete p;
        p = nullptr;
    };
    killProc(mPmon);
    killProc(mApps);

    QMutexLocker lock(&mMutex);
    mLatest.clear();
    mPmonBuffer.clear();
    mAppsBuffer.clear();
}

bool NvidiaSmiPmonStreamer::isRunning() const
{
    return (mPmon && mPmon->state() == QProcess::Running)
        || (mApps && mApps->state() == QProcess::Running);
}

NvidiaSmiPmonStreamer::Sample NvidiaSmiPmonStreamer::get(pid_t pid) const
{
    QMutexLocker lock(&mMutex);
    return mLatest.value(pid, Sample{});
}

void NvidiaSmiPmonStreamer::pruneDeadPids(const QSet<pid_t> &alivePids)
{
    QMutexLocker lock(&mMutex);
    for (auto it = mLatest.begin(); it != mLatest.end(); ) {
        if (!alivePids.contains(it.key()))
            it = mLatest.erase(it);
        else
            ++it;
    }
}

void NvidiaSmiPmonStreamer::drainToLines(QProcess *proc, QString &buffer,
    void (NvidiaSmiPmonStreamer::*handler)(const QString &))
{
    if (!proc)
        return;

    const QString chunk = QString::fromUtf8(proc->readAllStandardOutput());

    QString pending;
    {
        QMutexLocker lock(&mMutex);
        buffer += chunk;
        pending = std::move(buffer);
        buffer.clear();
    }

    int idx;
    while ((idx = pending.indexOf('\n')) >= 0) {
        (this->*handler)(pending.left(idx).trimmed());
        pending.remove(0, idx + 1);
    }
    if (!pending.isEmpty()) {
        QMutexLocker lock(&mMutex);
        buffer = pending + buffer;
    }
}

void NvidiaSmiPmonStreamer::onPmonReadyRead()
{
    drainToLines(mPmon, mPmonBuffer, &NvidiaSmiPmonStreamer::parsePmonLine);
}

void NvidiaSmiPmonStreamer::onComputeAppsReadyRead()
{
    drainToLines(mApps, mAppsBuffer, &NvidiaSmiPmonStreamer::parseComputeAppsLine);
}

void NvidiaSmiPmonStreamer::parsePmonLine(const QString &line)
{
    if (line.isEmpty() || line.startsWith('#'))
        return;

    // Format: "<gpu> <pid> <type> <sm> <mem> <enc> <dec> <command>"
    // Whitespace-separated, variable widths. "type" of "-" means idle slot.
    static const QRegularExpression ws(R"(\s+)");
    const QStringList parts = line.split(ws, Qt::SkipEmptyParts);
    if (parts.size() < 4)
        return;

    bool ok = false;
    const pid_t pid = parts.at(1).toLongLong(&ok);
    if (!ok || pid <= 0)
        return;

    const int sm = parts.at(3).toInt(&ok);
    if (!ok)
        return;

    QMutexLocker lock(&mMutex);
    Sample &s = mLatest[pid];
    s.gpuPercent = sm;
}

void NvidiaSmiPmonStreamer::parseComputeAppsLine(const QString &line)
{
    if (line.isEmpty())
        return;

    // Format: "<pid>, <used_memory_MiB>"
    const QStringList parts = line.split(',');
    if (parts.size() < 2)
        return;

    bool ok = false;
    const pid_t pid = parts.at(0).trimmed().toLongLong(&ok);
    if (!ok || pid <= 0)
        return;

    const qint64 mib = parts.at(1).trimmed().toLongLong(&ok);
    if (!ok)
        return;

    QMutexLocker lock(&mMutex);
    Sample &s = mLatest[pid];
    s.vramBytes = mib * 1024LL * 1024LL;
}

void NvidiaSmiPmonStreamer::onProcessFinished(int, QProcess::ExitStatus)
{
    QProcess *sender = qobject_cast<QProcess *>(QObject::sender());
    if (!sender)
        return;

    int *retries = (sender == mPmon) ? &mRestartsRemainingPmon
                                     : &mRestartsRemainingApps;
    if (*retries <= 0)
        return;
    --(*retries);

    if (sender == mPmon) {
        delete mPmon;
        mPmon = nullptr;
    } else {
        delete mApps;
        mApps = nullptr;
    }
    start();   // idempotent — restarts only what's nullptr.
}
