#ifndef NETHOGS_STREAMER_H
#define NETHOGS_STREAMER_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QProcess>

#include <sys/types.h>

// SSO-15379: persistent `nethogs -t` child process, mirroring the
// NettopStreamer (macOS, FR-102) / NvidiaSmiStreamer (Linux, FR-106 Step C)
// pattern — one long-lived streaming subprocess instead of a fork per tick.
//
// Unlike nettop's cumulative byte counters, nethogs' tracemode (`-t`) output
// already reports a per-process KB/s rate for its own refresh interval, so
// there is no delta/elapsed-time bookkeeping to do on the consumer side —
// callers just read the latest snapshot.
//
// nethogs requires raw packet capture (CAP_NET_RAW/CAP_NET_ADMIN or root).
// Without it, the process typically exits almost immediately with a non-zero
// status. Status distinguishes "binary not found" from "binary present but
// exited before ever producing a sample" (most likely a permission problem)
// so the UI can surface a real message instead of silently showing blank
// data (SSO-15379 acceptance criterion).
//
// Caveat: the exact tracemode column layout below is based on nethogs'
// long-documented `-t` format, not verified against a live binary in this
// environment (no nethogs, no CAP_NET_RAW/root available here) — see
// parseLine()'s fixture test and the SSO-15379 PR description.
class NethogsStreamer : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        NotStarted,
        Running,
        ToolMissing,        // nethogs binary not found on PATH
        ExitedImmediately,  // binary present but exited before any sample —
                             // almost always a missing-capability/permission issue
    };

    explicit NethogsStreamer(QObject *parent = nullptr);
    ~NethogsStreamer() override;

    // Launches `nethogs -t -d<intervalSec>`. Safe to call while already
    // running (no-op). Sets status() to ToolMissing immediately, without
    // spawning anything, if the binary isn't on PATH.
    void start(int intervalSec = 1);
    void stop();
    bool isRunning() const;
    Status status() const;

    // Thread-safe snapshot of the current {pid -> (sentBytesPerSec,
    // receivedBytesPerSec)} map, already converted from nethogs' KB/s to
    // bytes/s for consistency with FormatUtil::formatBytes().
    QHash<pid_t, QPair<double, double>> snapshot() const;

    // Drops entries for any pid not present in the most recent process list
    // tick, mirroring NettopStreamer::pruneDeadPids — keeps the hash from
    // growing across the streamer's app-lifetime.
    void pruneDeadPids(const QSet<pid_t> &alivePids);

    // Pure line parser, exposed for fixture tests. Returns true and
    // populates the out-params for a valid per-process tracemode row;
    // false for headers ("Refreshing:"), blank lines, or malformed input.
    // Input line is one raw line of `nethogs -t` stdout, already trimmed.
    static bool parseLine(const QString &line,
                          pid_t *pid,
                          double *sentKBps,
                          double *receivedKBps);

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *mProcess = nullptr;
    Status mStatus = Status::NotStarted;
    bool mEverSampled = false;

    mutable QMutex mMutex;
    QHash<pid_t, QPair<double, double>> mLatest;
    QString mLineBuffer;
};

#endif // NETHOGS_STREAMER_H
