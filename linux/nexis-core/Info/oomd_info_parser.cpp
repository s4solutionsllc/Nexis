#include "oomd_info_parser.h"

#include <QRegularExpression>

namespace OomdInfoParser {

QMap<QString, QString> parseSystemctlShow(const QByteArray &bytes)
{
    QMap<QString, QString> out;

    const QList<QByteArray> lines = bytes.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty())
            continue;

        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;   // no key, or key empty

        const QString key = QString::fromUtf8(line.left(eq)).trimmed();
        const QString value = QString::fromUtf8(line.mid(eq + 1));
        if (key.isEmpty())
            continue;

        out.insert(key, value);
    }

    return out;
}

QMap<QString, quint64> parseCgroupV2KeyedFile(const QByteArray &bytes)
{
    QMap<QString, quint64> out;

    const QList<QByteArray> lines = bytes.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty())
            continue;

        // cgroup v2 keyed format is `key value\n`; tolerate stray tabs.
        const int sp = line.indexOf(' ');
        const int tab = line.indexOf('\t');
        int sep = -1;
        if (sp >= 0 && tab >= 0)
            sep = qMin(sp, tab);
        else
            sep = qMax(sp, tab);
        if (sep <= 0)
            continue;

        const QString key = QString::fromUtf8(line.left(sep)).trimmed();
        bool ok = false;
        const quint64 value =
            QString::fromUtf8(line.mid(sep + 1)).trimmed().toULongLong(&ok);
        if (!ok || key.isEmpty())
            continue;

        out.insert(key, value);
    }

    return out;
}

QList<OomdEvent> parseOomdJournalLines(const QStringList &lines, int maxEvents)
{
    if (maxEvents <= 0)
        return {};

    // Typical short-iso line:
    //   2026-06-11T13:30:42+0000 host systemd-oomd[123]: Killed 3 tasks in /user.slice/user-1000.slice/user@1000.service/app.slice/firefox.service due to memory pressure
    // We also accept the simpler short form without the timezone suffix.
    static const QRegularExpression killRe(
        QStringLiteral(
            R"(^(\S+)\s+\S+\s+systemd-oomd(?:\[\d+\])?:\s+)"
            R"(Killed\s+(\d+)\s+tasks?\s+in\s+(\S+?)\s+due\s+to\s+(.+?)\s*$)"));

    QList<OomdEvent> events;
    events.reserve(qMin(maxEvents, lines.size()));

    for (const QString &line : lines) {
        const QRegularExpressionMatch m = killRe.match(line);
        if (!m.hasMatch())
            continue;

        OomdEvent ev;
        // Accept ISO 8601 with optional timezone in `+HHMM` form. Qt parses
        // `2026-06-11T13:30:42+00:00` directly; for `+0000` we strip the colon.
        QString ts = m.captured(1);
        const QRegularExpression tzFix(QStringLiteral(R"([+-]\d{4}$)"));
        if (tzFix.match(ts).hasMatch())
            ts.insert(ts.size() - 2, QLatin1Char(':'));
        ev.when = QDateTime::fromString(ts, Qt::ISODate);

        ev.tasksKilled = m.captured(2).toInt();
        ev.cgroupPath = m.captured(3);
        ev.reason = m.captured(4).trimmed();

        // Best-effort unit extraction from the cgroup path: take the rightmost
        // path segment that ends in `.service`/`.scope`/`.slice` (typically the
        // process's hosting unit). Falls back to the last segment.
        const QStringList segs = ev.cgroupPath.split(QLatin1Char('/'),
                                                    Qt::SkipEmptyParts);
        for (auto it = segs.crbegin(); it != segs.crend(); ++it) {
            if (it->endsWith(QLatin1String(".service")) ||
                it->endsWith(QLatin1String(".scope")) ||
                it->endsWith(QLatin1String(".slice"))) {
                ev.unit = *it;
                break;
            }
        }
        if (ev.unit.isEmpty() && !segs.isEmpty())
            ev.unit = segs.last();

        events.append(ev);
        if (events.size() >= maxEvents)
            break;
    }

    return events;
}

OomdSnapshot assembleSnapshot(bool cgroupV2Detected,
                              const QMap<QString, QString> &systemctlProps,
                              const QMap<QString, quint64> &memoryEvents,
                              const QList<OomdEvent> &recentEvents)
{
    OomdSnapshot s;
    s.cgroupV2 = cgroupV2Detected;
    s.loadState = systemctlProps.value(QStringLiteral("LoadState"));
    s.activeState = systemctlProps.value(QStringLiteral("ActiveState"));

    auto readU64 = [](const QString &v) -> quint64 {
        bool ok = false;
        const quint64 n = v.toULongLong(&ok);
        return ok ? n : 0ULL;
    };
    s.oomKills = readU64(systemctlProps.value(QStringLiteral("OOMKills")));
    s.managedOomKills =
        readU64(systemctlProps.value(QStringLiteral("ManagedOOMKills")));

    s.systemOomKill = memoryEvents.value(QStringLiteral("oom_kill"), 0);

    s.recentEvents = recentEvents;

    // `available` is true when any of: oomd reports a load state, kernel
    // exposes an oom_kill counter, or we recovered at least one event. This
    // lets the UI hide the panel entirely on hosts that have no signal at all.
    s.available = !s.loadState.isEmpty()
                  || !s.activeState.isEmpty()
                  || s.oomKills > 0
                  || s.managedOomKills > 0
                  || s.systemOomKill > 0
                  || !s.recentEvents.isEmpty()
                  || s.cgroupV2;

    return s;
}

} // namespace OomdInfoParser
