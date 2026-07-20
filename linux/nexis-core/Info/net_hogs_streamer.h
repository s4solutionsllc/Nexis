#ifndef NET_HOGS_STREAMER_H
#define NET_HOGS_STREAMER_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QSet>
#include <QString>

#include <sys/types.h>

// SSO-15379: fallback per-process network source used when NetAcctBpfLoader
// can't load (no CAP_BPF/root, unsupported kernel, or Nexis was built without
// libbpf). Keeps one persistent `nethogs -t` child running and parses its
// trace-mode stdout, the same "long-lived streaming child" shape as
// NettopStreamer (macOS) rather than forking per tick.
//
// Trace-mode already reports a live rate per refresh window (KB/s), not a
// cumulative counter, so unlike NettopStreamer/proc-io callers should read
// snapshot() directly as bytes/sec — no delta tracking needed on top.
//
// nethogs itself typically needs the same elevated privileges (raw socket
// capture) that eBPF needs CAP_BPF for, so a permission failure here is just
// as likely as a permission failure loading the BPF program; hasFailed()/
// lastError() let ProcessInfoLinux fold both into one coherent status.
class NetHogsStreamer : public QObject
{
    Q_OBJECT

public:
    explicit NetHogsStreamer(QObject *parent = nullptr);
    ~NetHogsStreamer() override;

    // Launches `nethogs -t -d <intervalSec>`. Safe to call while already
    // running — no-op if so.
    void start(int intervalSec = 1);
    void stop();
    bool isRunning() const;

    // True once a successfully-started child has exited or failed to start.
    // Distinguishes "never tried" / "running fine" from "tried and failed" so
    // callers don't wait forever on a process that already gave up.
    bool hasFailed() const;
    QString lastError() const;

    // {pid -> (down bytes/sec, up bytes/sec)}, read directly off the most
    // recent trace-mode refresh block. No cumulative counters involved.
    QHash<pid_t, QPair<double, double>> snapshot() const;

    // SSO-3399-style hygiene: drop entries for pids no longer alive so the
    // hash doesn't grow across the streamer's lifetime.
    void pruneDeadPids(const QSet<pid_t> &alivePids);

    // Pure trace-mode line parser. Returns true and populates the out-params
    // for a per-process data row; false for "Refreshing:"/"TOTAL" separator
    // lines, blank lines, or malformed input. Exposed for fixture tests.
    static bool parseTraceLine(const QString &line, pid_t *pid, double *sentBps, double *recvBps);

private slots:
    void onReadyRead();
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parseLine(const QString &line);

    QProcess *mProcess = nullptr;
    mutable QMutex mMutex;
    QHash<pid_t, QPair<double, double>> mLatest;
    QString mLineBuffer;
    bool mFailed = false;
    QString mLastError;
};

#endif // NET_HOGS_STREAMER_H
