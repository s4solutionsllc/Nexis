#include "nvidia_smi_streamer.h"

#include "Utils/command_util.h"

#include <QDebug>
#include <QMutexLocker>

#include <climits>

NvidiaSmiStreamer::NvidiaSmiStreamer(QObject *parent)
    : QObject(parent)
{
}

NvidiaSmiStreamer::~NvidiaSmiStreamer()
{
    stop();
}

void NvidiaSmiStreamer::start(int intervalSec)
{
    if (mProcess && mProcess->state() != QProcess::NotRunning)
        return;

    if (!CommandUtil::isExecutable("nvidia-smi"))
        return;

    mIntervalSec = intervalSec > 0 ? intervalSec : 1;

    if (!mProcess) {
        mProcess = new QProcess(this);
        mProcess->setProcessChannelMode(QProcess::SeparateChannels);
        connect(mProcess, &QProcess::readyReadStandardOutput,
                this, &NvidiaSmiStreamer::onReadyRead);
        connect(mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &NvidiaSmiStreamer::onFinished);
    }

    mProcess->start("nvidia-smi",
                    {"--query-gpu=index,utilization.gpu,fan.speed",
                     "--format=csv,noheader,nounits",
                     "-l", QString::number(mIntervalSec)});

    if (!mProcess->waitForStarted(3000)) {
        qWarning() << "NvidiaSmiStreamer: failed to start nvidia-smi";
        delete mProcess;
        mProcess = nullptr;
    }
}

void NvidiaSmiStreamer::stop()
{
    if (!mProcess)
        return;

    mProcess->disconnect(this);   // suppress onFinished restart path
    mProcess->kill();
    mProcess->waitForFinished(1000);
    delete mProcess;
    mProcess = nullptr;

    QMutexLocker lock(&mMutex);
    mLatest.clear();
    mLineBuffer.clear();
}

bool NvidiaSmiStreamer::isRunning() const
{
    return mProcess && mProcess->state() == QProcess::Running;
}

NvidiaSmiCache::Sample NvidiaSmiStreamer::get(int index) const
{
    QMutexLocker lock(&mMutex);
    return mLatest.value(index, NvidiaSmiCache::Sample{});
}

int NvidiaSmiStreamer::ageMs() const
{
    QMutexLocker lock(&mMutex);
    if (!mEverRefreshed)
        return INT_MAX;
    return static_cast<int>(mLastRefresh.elapsed());
}

void NvidiaSmiStreamer::onReadyRead()
{
    if (!mProcess)
        return;

    const QString chunk = QString::fromUtf8(mProcess->readAllStandardOutput());

    QString pending;
    {
        QMutexLocker lock(&mMutex);
        mLineBuffer += chunk;
        pending = std::move(mLineBuffer);
        mLineBuffer.clear();
    }

    int idx;
    while ((idx = pending.indexOf('\n')) >= 0) {
        parseLine(pending.left(idx).trimmed());
        pending.remove(0, idx + 1);
    }
    // Save unterminated tail back for the next read.
    if (!pending.isEmpty()) {
        QMutexLocker lock(&mMutex);
        mLineBuffer = pending + mLineBuffer;
    }
}

void NvidiaSmiStreamer::parseLine(const QString &line)
{
    if (line.isEmpty())
        return;

    // Format: "<index>, <utilization>, <fan.speed>"
    const QStringList parts = line.split(',');
    if (parts.size() < 3)
        return;

    bool ok = false;
    const int index = parts.at(0).trimmed().toInt(&ok);
    if (!ok)
        return;

    NvidiaSmiCache::Sample s;
    const QString utilStr = parts.at(1).trimmed();
    const int util = utilStr.toInt(&ok);
    s.utilization = ok ? util : -1;

    const QString fanStr = parts.at(2).trimmed();
    const int fan = fanStr.toInt(&ok);
    s.fanPercent = ok ? fan : -1;

    QMutexLocker lock(&mMutex);
    mLatest.insert(index, s);
    if (!mEverRefreshed)
        mLastRefresh.start();
    else
        mLastRefresh.restart();
    mEverRefreshed = true;
}

void NvidiaSmiStreamer::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    // Don't restart if stop() was called — stop() disconnects first.
    if (!mProcess)
        return;

    // Single retry on unexpected exit (driver reload, suspended GPU, etc.).
    if (mRestartsRemaining > 0) {
        --mRestartsRemaining;
        const int interval = mIntervalSec;
        delete mProcess;
        mProcess = nullptr;
        start(interval);
    }
}
