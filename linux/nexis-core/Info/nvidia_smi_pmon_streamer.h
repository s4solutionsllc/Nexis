#ifndef NVIDIA_SMI_PMON_STREAMER_H
#define NVIDIA_SMI_PMON_STREAMER_H

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>

#include <sys/types.h>

// FR-115: persistent per-process NVIDIA sampler for the Processes page.
// Runs two long-lived nvidia-smi children:
//   1. `nvidia-smi pmon -d 1 -s u -c 0`
//        → streams `gpu, pid, type, sm, mem, enc, dec, command`
//        → we consume pid + sm (GPU %)
//   2. `nvidia-smi --query-compute-apps=pid,used_memory \
//                 --format=csv,noheader,nounits -l 1`
//        → streams `pid, used_memory_MiB`
//        → we consume pid + used_memory (VRAM)
//
// Both streams feed a single mutex-guarded QHash<pid_t, PmonSample>.
// Zero forks per tick once running — mirrors NvidiaSmiStreamer (FR-106 Step C).
//
// Started lazily when ProcessInfoLinux first sees mCollectGpu true with at
// least one NVIDIA device present; stays alive for the app lifetime.
class NvidiaSmiPmonStreamer : public QObject
{
    Q_OBJECT

public:
    struct Sample {
        int    gpuPercent  = -1;   // -1 = unknown
        qint64 vramBytes   = -1;   // -1 = unknown
    };

    explicit NvidiaSmiPmonStreamer(QObject *parent = nullptr);
    ~NvidiaSmiPmonStreamer() override;

    void start();
    void stop();
    bool isRunning() const;

    Sample get(pid_t pid) const;

    // SSO-3399: drop entries for pids that aren't in the alive set. Caller
    // typically passes the activeGpuPids snapshot for the most recent tick
    // so mLatest doesn't grow unbounded across the streamer's lifetime.
    void pruneDeadPids(const QSet<pid_t> &alivePids);

private slots:
    void onPmonReadyRead();
    void onComputeAppsReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parsePmonLine(const QString &line);
    void parseComputeAppsLine(const QString &line);
    void drainToLines(QProcess *proc, QString &buffer,
                      void (NvidiaSmiPmonStreamer::*handler)(const QString &));

    QProcess *mPmon = nullptr;
    QProcess *mApps = nullptr;
    int mRestartsRemainingPmon = 1;
    int mRestartsRemainingApps = 1;

    mutable QMutex mMutex;
    QHash<pid_t, Sample> mLatest;
    QString mPmonBuffer;
    QString mAppsBuffer;
};

#endif // NVIDIA_SMI_PMON_STREAMER_H
