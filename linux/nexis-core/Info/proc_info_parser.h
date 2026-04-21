#ifndef PROC_INFO_PARSER_H
#define PROC_INFO_PARSER_H

#include <QByteArray>
#include <QString>

// FR-127: pure parsing / formatting helpers for /proc/<pid>/{stat,status,cmdline}.
//
// Kept as free functions with no Linux-specific includes so they compile (and
// unit-test) on any platform. Callers in ProcessInfoLinux do the actual file
// I/O, libc uid→name lookups, and then hand the bytes here.
//
// All functions are pure: same input → same output, no globals, no syscalls.
namespace ProcInfoParser {

struct StatFields {
    QString  comm;        // binary name, parens stripped
    QChar    state;       // R, S, D, Z, T, …
    qint64   session  = 0;
    quint64  utime    = 0;    // clock ticks (user)
    quint64  stime    = 0;    // clock ticks (kernel)
    int      nice     = 0;
    quint64  starttime = 0;   // clock ticks since boot
    quint64  vsize    = 0;    // bytes
    quint64  rssPages = 0;    // pages — caller multiplies by page size
};

// Parse the single-line /proc/<pid>/stat content. Returns false on malformed
// input. Handles comm fields containing whitespace or parentheses by locating
// the LAST ')' and splitting there (see kernel fs/proc/array.c do_task_stat).
bool parseStat(const QByteArray &content, StatFields &out);

struct StatusFields {
    bool    hasUid = false;
    bool    hasGid = false;
    quint32 uid    = 0;       // real uid (first column of "Uid:" line)
    quint32 gid    = 0;       // real gid (first column of "Gid:" line)
};

// Parse /proc/<pid>/status (newline-separated key: value pairs). Reads only
// Uid and Gid — everything else we need is in stat.
bool parseStatus(const QByteArray &content, StatusFields &out);

// /proc/<pid>/cmdline uses '\0' separators. Empty on kernel threads — in that
// case fall back to "[comm]" to mirror `ps`.
QString formatCmdline(const QByteArray &cmdline, const QString &commFallback);

// Parse /proc/stat's "btime <N>" line. Returns seconds since epoch, or 0 on
// parse failure.
quint64 parseBootTime(const QByteArray &procStatContent);

// Parse "MemTotal: N kB" from /proc/meminfo. Returns bytes, or 0 on failure.
quint64 parseMemTotalBytes(const QByteArray &procMeminfoContent);

// Parse /proc/uptime. Returns seconds since boot (first float), or 0.
double parseUptimeSec(const QByteArray &procUptimeContent);

// Format starttime (clock ticks since boot) as a ps-style column:
//   same day   → "HH:MM"
//   same year  → "MonDD"
//   older      → "YYYY"
QString formatStartTime(quint64 bootTimeSec, quint64 starttimeTicks,
                        long clkTck, qint64 nowSecsSinceEpoch);

// Format total CPU time (utime + stime in clock ticks) as [DD-]HH:MM:SS.
QString formatCpuTime(quint64 totalTicks, long clkTck);

// FR-115: /proc/<pid>/fdinfo/<fd> contents for a DRM file descriptor.
// Non-DRM fdinfo files (e.g. for sockets, pipes) return false with
// everything zero-filled — caller should skip.
struct DrmFdinfo {
    QString driver;          // e.g. "i915", "amdgpu", "xe"
    qint64  clientId   = -1; // -1 means no drm-client-id present
    quint64 engineNs   = 0;  // sum of all drm-engine-* nanosecond counters
    quint64 memVramB   = 0;  // drm-memory-vram (bytes; converted from "KiB"/"MiB")
    quint64 memTotalB  = 0;  // sum across vram/gtt/cpu if only -total-* keys present
};

bool parseDrmFdinfo(const QByteArray &content, DrmFdinfo &out);

} // namespace ProcInfoParser

#endif // PROC_INFO_PARSER_H
