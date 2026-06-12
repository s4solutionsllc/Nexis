#ifndef OOMD_INFO_PARSER_H
#define OOMD_INFO_PARSER_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

#include "oomd_snapshot.h"

// FW-11 (SSO-3739): pure parsers for the systemd-oomd / cgroup v2 inputs the
// OomdInfoLinux provider feeds at runtime. Lives in linux/ but contains no
// system calls so the test target can compile it directly on any host.
namespace OomdInfoParser {

// Parse `systemctl show ...` output. Keys are case-sensitive and follow the
// `Key=Value` form. Values may legitimately be empty (e.g. `LoadState=`).
// Unknown / blank lines are ignored.
QMap<QString, QString> parseSystemctlShow(const QByteArray &bytes);

// Parse a cgroup v2 keyed file (memory.events, cgroup.events, memory.swap.events).
// One `key value\n` per line; numeric values only — non-numeric values are skipped.
QMap<QString, quint64> parseCgroupV2KeyedFile(const QByteArray &bytes);

// Parse `journalctl -u systemd-oomd.service -o short-iso --no-pager` output
// into the most recent OOM-kill events. Newest first, capped at `maxEvents`.
// Lines that do not look like an oomd kill are ignored. Time is ISO-8601
// (e.g. `2026-06-11T13:30:42+0000`); if parsing fails the event still lands
// with `when.isValid() == false`.
QList<OomdEvent> parseOomdJournalLines(const QStringList &lines, int maxEvents = 16);

// Combine the three inputs into a snapshot. `cgroupV2Detected` indicates that
// `/sys/fs/cgroup/cgroup.controllers` exists (the unified-hierarchy marker the
// provider checks before reading any other cgroup path).
OomdSnapshot assembleSnapshot(bool cgroupV2Detected,
                              const QMap<QString, QString> &systemctlProps,
                              const QMap<QString, quint64> &memoryEvents,
                              const QList<OomdEvent> &recentEvents);

} // namespace OomdInfoParser

#endif // OOMD_INFO_PARSER_H
