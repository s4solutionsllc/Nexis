#include "boot_analysis_info_linux.h"

#include <QProcess>
#include <QRegularExpression>
#include <algorithm>

static QString runCmd(const QString &prog, const QStringList &args)
{
    QProcess p;
    p.start(prog, args);
    if (!p.waitForFinished(10000))
        return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}

static double parseTimeToMs(const QString &value, const QString &unit)
{
    double v = value.toDouble();
    return (unit == QLatin1String("ms")) ? v : v * 1000.0;
}

BootAnalysisData BootAnalysisInfoLinux::analyze() const
{
    BootAnalysisData result;

    // Verify systemd-analyze is present
    {
        QProcess p;
        p.start(QStringLiteral("systemd-analyze"), {QStringLiteral("--version")});
        if (!p.waitForFinished(3000) || p.exitCode() != 0) {
            result.error = QStringLiteral("systemd-analyze not available on this system.");
            return result;
        }
    }

    // Total boot time
    {
        const QString out = runCmd(QStringLiteral("systemd-analyze"), {});
        // "= 9.257s" or "= 450ms"
        static const QRegularExpression reTot(
            QStringLiteral(R"(=\s*([\d.]+)(ms|s)\s*$)"), QRegularExpression::MultilineOption);
        const auto m = reTot.match(out);
        if (m.hasMatch())
            result.totalBootMs = parseTimeToMs(m.captured(1), m.captured(2));
    }

    // Per-service breakdown
    {
        const QString out = runCmd(QStringLiteral("systemd-analyze"), {QStringLiteral("blame")});
        static const QRegularExpression reEntry(
            QStringLiteral(R"(^\s*([\d.]+)(ms|s)\s+(.+)$)"), QRegularExpression::MultilineOption);

        QRegularExpressionMatchIterator it = reEntry.globalMatch(out);
        while (it.hasNext()) {
            const auto m = it.next();
            double ms = parseTimeToMs(m.captured(1), m.captured(2));
            BootEntry e;
            e.durationMs = ms;
            e.name       = m.captured(3).trimmed();
            e.impact     = impactFor(ms);
            result.entries.append(e);
        }

        std::stable_sort(result.entries.begin(), result.entries.end(),
            [](const BootEntry &a, const BootEntry &b) { return a.durationMs > b.durationMs; });
    }

    result.available = true;
    return result;
}
