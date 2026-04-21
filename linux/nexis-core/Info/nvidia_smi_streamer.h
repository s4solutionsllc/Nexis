#ifndef NVIDIA_SMI_STREAMER_H
#define NVIDIA_SMI_STREAMER_H

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QString>

#include "nvidia_smi_cache.h"

// FR-106 Step C: persistent nvidia-smi child. Supersedes the per-tick fork
// path used in Bundle B Step A.
//
// Runs `nvidia-smi --query-gpu=index,utilization.gpu,fan.speed
//       --format=csv,noheader,nounits -l <interval>`
// once for the app lifetime. Streaming stdout is parsed line-by-line and
// the result pushed into a mutex-guarded QHash that GpuInfoLinux and
// FanInfoLinux read from via the NvidiaSmiCache facade. Zero forks per
// tick in steady state.
//
// Mirrors macOS's NettopStreamer pattern (FR-102, Bundle B).
class NvidiaSmiStreamer : public QObject
{
    Q_OBJECT

public:
    explicit NvidiaSmiStreamer(QObject *parent = nullptr);
    ~NvidiaSmiStreamer() override;

    void start(int intervalSec = 1);
    void stop();
    bool isRunning() const;

    NvidiaSmiCache::Sample get(int index) const;
    int ageMs() const;

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parseLine(const QString &line);

    QProcess *mProcess = nullptr;
    int mIntervalSec = 1;
    int mRestartsRemaining = 1;   // one-shot retry on unexpected exit

    mutable QMutex mMutex;
    QHash<int, NvidiaSmiCache::Sample> mLatest;
    QElapsedTimer mLastRefresh;
    bool mEverRefreshed = false;
    QString mLineBuffer;          // stdout carryover between reads
};

#endif // NVIDIA_SMI_STREAMER_H
