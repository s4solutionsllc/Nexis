#include "proc_info_parser.h"

#include <QDateTime>
#include <QLatin1String>
#include <QList>
#include <QRegularExpression>

namespace ProcInfoParser {

bool parseStat(const QByteArray &content, StatFields &out)
{
    // /proc/<pid>/stat is a single line. comm is wrapped in parens and may
    // contain any printable chars including ')'. The kernel delimits it by
    // the FIRST '(' and the LAST ')', so use lastIndexOf.
    const int openParen  = content.indexOf('(');
    const int closeParen = content.lastIndexOf(')');
    if (openParen < 0 || closeParen <= openParen)
        return false;

    out.comm = QString::fromUtf8(content.mid(openParen + 1, closeParen - openParen - 1));

    // Tail is everything after ") " — space-separated fields starting at
    // field 3 (state).
    QByteArray tail = content.mid(closeParen + 1).trimmed();
    QList<QByteArray> tok = tail.split(' ');
    // Drop empty tokens (in case of double spaces).
    QList<QByteArray> fields;
    fields.reserve(tok.size());
    for (const QByteArray &t : tok)
        if (!t.isEmpty())
            fields.append(t);

    // We need up to index [20] relative to the tail, which corresponds to
    // kernel field 24 (rss). That's the minimum.
    if (fields.size() < 22)
        return false;

    // Index map (0-based into `fields`, relative to state):
    //   0 = state (field 3)
    //   3 = session (field 6)
    //  11 = utime (14)
    //  12 = stime (15)
    //  16 = nice (19)
    //  19 = starttime (22)
    //  20 = vsize (23)
    //  21 = rss in pages (24)
    const QByteArray &stateBytes = fields.at(0);
    out.state = stateBytes.isEmpty() ? QChar('?') : QChar::fromLatin1(stateBytes.at(0));

    bool ok = false;
    out.session = fields.at(3).toLongLong(&ok);
    if (!ok) out.session = 0;

    out.utime     = fields.at(11).toULongLong(&ok); if (!ok) out.utime = 0;
    out.stime     = fields.at(12).toULongLong(&ok); if (!ok) out.stime = 0;
    out.nice      = fields.at(16).toInt(&ok);       if (!ok) out.nice = 0;
    out.starttime = fields.at(19).toULongLong(&ok); if (!ok) out.starttime = 0;
    out.vsize     = fields.at(20).toULongLong(&ok); if (!ok) out.vsize = 0;
    out.rssPages  = fields.at(21).toULongLong(&ok); if (!ok) out.rssPages = 0;

    return true;
}

bool parseStatus(const QByteArray &content, StatusFields &out)
{
    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("Uid:")) {
            const QList<QByteArray> parts = QByteArray(line.mid(4)).simplified().split(' ');
            if (!parts.isEmpty()) {
                bool ok = false;
                quint32 v = parts.first().toUInt(&ok);
                if (ok) { out.uid = v; out.hasUid = true; }
            }
        } else if (line.startsWith("Gid:")) {
            const QList<QByteArray> parts = QByteArray(line.mid(4)).simplified().split(' ');
            if (!parts.isEmpty()) {
                bool ok = false;
                quint32 v = parts.first().toUInt(&ok);
                if (ok) { out.gid = v; out.hasGid = true; }
            }
        }
    }
    return out.hasUid || out.hasGid;
}

QString formatCmdline(const QByteArray &cmdline, const QString &commFallback)
{
    if (cmdline.isEmpty())
        return QStringLiteral("[%1]").arg(commFallback);

    // Replace NUL separators with spaces; trim trailing NUL that many kernels
    // emit after the last arg.
    QByteArray cleaned = cmdline;
    // Strip all trailing NULs.
    while (!cleaned.isEmpty() && cleaned.endsWith('\0'))
        cleaned.chop(1);
    cleaned.replace('\0', ' ');

    QString result = QString::fromUtf8(cleaned).trimmed();
    if (result.isEmpty())
        return QStringLiteral("[%1]").arg(commFallback);
    return result;
}

quint64 parseBootTime(const QByteArray &procStatContent)
{
    const QList<QByteArray> lines = procStatContent.split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("btime ") || line.startsWith("btime\t")) {
            const QByteArray val = QByteArray(line.mid(6)).trimmed();
            bool ok = false;
            const quint64 v = val.toULongLong(&ok);
            return ok ? v : 0;
        }
    }
    return 0;
}

quint64 parseMemTotalBytes(const QByteArray &procMeminfoContent)
{
    const QList<QByteArray> lines = procMeminfoContent.split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("MemTotal:")) {
            // Format: "MemTotal:       16318540 kB"
            const QList<QByteArray> parts = QByteArray(line.mid(9)).simplified().split(' ');
            if (parts.isEmpty())
                return 0;
            bool ok = false;
            const quint64 kib = parts.first().toULongLong(&ok);
            if (!ok)
                return 0;
            // Assume kB (kilobytes per kernel convention — actually KiB).
            return kib * 1024ULL;
        }
    }
    return 0;
}

double parseUptimeSec(const QByteArray &procUptimeContent)
{
    const QByteArray trimmed = procUptimeContent.trimmed();
    const int sp = trimmed.indexOf(' ');
    const QByteArray first = (sp < 0) ? trimmed : trimmed.left(sp);
    bool ok = false;
    const double v = first.toDouble(&ok);
    return ok ? v : 0.0;
}

QString formatStartTime(quint64 bootTimeSec, quint64 starttimeTicks,
                        long clkTck, qint64 nowSecsSinceEpoch)
{
    if (clkTck <= 0 || bootTimeSec == 0)
        return QString();

    const qint64 procStartSec =
        static_cast<qint64>(bootTimeSec) +
        static_cast<qint64>(starttimeTicks / static_cast<quint64>(clkTck));

    const QDateTime procStart = QDateTime::fromSecsSinceEpoch(procStartSec);
    const QDateTime now = QDateTime::fromSecsSinceEpoch(nowSecsSinceEpoch);

    if (procStart.date() == now.date())
        return procStart.toString("HH:mm");
    if (procStart.date().year() == now.date().year())
        return procStart.toString("MMMdd");
    return procStart.toString("yyyy");
}

QString formatCpuTime(quint64 totalTicks, long clkTck)
{
    if (clkTck <= 0)
        return QStringLiteral("00:00:00");
    const quint64 totalSec = totalTicks / static_cast<quint64>(clkTck);
    const quint64 days = totalSec / 86400;
    const quint64 hours = (totalSec % 86400) / 3600;
    const quint64 minutes = (totalSec % 3600) / 60;
    const quint64 seconds = totalSec % 60;

    if (days > 0) {
        return QString("%1-%2:%3:%4")
            .arg(days)
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

namespace {

// Parse an fdinfo memory-value suffix: "12345 KiB" or "7 MiB" etc. Returns
// the byte value, or 0 on parse failure.
quint64 parseMemoryValue(const QByteArray &v)
{
    const QByteArray trimmed = v.trimmed();
    int sp = trimmed.indexOf(' ');
    const QByteArray num = (sp < 0) ? trimmed : trimmed.left(sp);
    const QByteArray unit = (sp < 0) ? QByteArray() : trimmed.mid(sp + 1).trimmed();

    bool ok = false;
    const quint64 n = num.toULongLong(&ok);
    if (!ok)
        return 0;

    if (unit == "KiB" || unit == "kiB" || unit == "kB" || unit == "KB")
        return n * 1024ULL;
    if (unit == "MiB" || unit == "MB")
        return n * 1024ULL * 1024ULL;
    if (unit == "GiB" || unit == "GB")
        return n * 1024ULL * 1024ULL * 1024ULL;
    if (unit == "B" || unit.isEmpty())
        return n;
    // Unknown unit — assume KiB (the most common kernel emit).
    return n * 1024ULL;
}

// Parse an fdinfo nanosecond engine value: "12345 ns" or plain "12345".
quint64 parseNsValue(const QByteArray &v)
{
    const QByteArray trimmed = v.trimmed();
    int sp = trimmed.indexOf(' ');
    const QByteArray num = (sp < 0) ? trimmed : trimmed.left(sp);
    bool ok = false;
    const quint64 n = num.toULongLong(&ok);
    return ok ? n : 0;
}

} // namespace

bool parseDrmFdinfo(const QByteArray &content, DrmFdinfo &out)
{
    out = DrmFdinfo{};

    const QList<QByteArray> lines = content.split('\n');
    bool isDrm = false;

    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;

        const QByteArray key = line.left(colon).trimmed();
        const QByteArray val = line.mid(colon + 1).trimmed();

        if (key == "drm-driver") {
            isDrm = true;
            out.driver = QString::fromLatin1(val);
        } else if (key == "drm-client-id") {
            bool ok = false;
            const qint64 id = val.toLongLong(&ok);
            if (ok)
                out.clientId = id;
        } else if (key.startsWith("drm-engine-")) {
            out.engineNs += parseNsValue(val);
        } else if (key == "drm-memory-vram") {
            out.memVramB += parseMemoryValue(val);
        } else if (key.startsWith("drm-total-")) {
            out.memTotalB += parseMemoryValue(val);
        }
    }

    return isDrm;
}

} // namespace ProcInfoParser
