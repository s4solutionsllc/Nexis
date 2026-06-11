#ifndef NETTOP_STREAMER_H
#define NETTOP_STREAMER_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QSet>

#include <sys/types.h>

// FR-102: persistent nettop child process. Previously ProcessInfoMacOS forked
// 'nettop -P -d -L 1 ...' on every process tick (50-150 ms init cost per call,
// on the UI thread). We now keep one 'nettop -P -d -s<interval>' running for
// the lifetime of the Processes page and harvest deltas from its streaming
// stdout as they arrive.
//
// Starts/stops tied to ProcessesPage::onPageActivated/onPageDeactivated and
// to the FR-108 "net columns visible" toggle — so the child only runs while
// someone is actually looking at network columns.
class NettopStreamer : public QObject
{
    Q_OBJECT

public:
    explicit NettopStreamer(QObject *parent = nullptr);
    ~NettopStreamer() override;

    // Launches nettop with the given sample interval (seconds). Safe to call
    // while already running — no-op if so.
    void start(int intervalSec = 1);
    void stop();
    bool isRunning() const;

    // Thread-safe snapshot of the current {pid -> (bytes_in, bytes_out)} map.
    // Returns cumulative byte counts; consumers compute rate deltas.
    QHash<pid_t, QPair<quint64, quint64>> snapshot() const;

    // SSO-3399: callers pass the set of pids that are still alive at the most
    // recent tick — entries for any other pid are dropped from mLatest so the
    // hash doesn't grow without bound across the streamer's lifetime.
    void pruneDeadPids(const QSet<pid_t> &alivePids);

    // WI-33: pure CSV-line parser. Returns true and populates the out-params
    // when the line is a valid per-process row; returns false for headers,
    // empty lines, or malformed input. Exposed for fixture tests.
    static bool parseCsvLine(const QString &line,
                             pid_t *pid,
                             quint64 *bytesIn,
                             quint64 *bytesOut);

private slots:
    void onReadyRead();

private:
    void parseLine(const QString &line);

    QProcess *mProcess = nullptr;
    mutable QMutex mMutex;
    QHash<pid_t, QPair<quint64, quint64>> mLatest;
    QString mLineBuffer;   // partial line carryover between reads
};

#endif // NETTOP_STREAMER_H
