#include "boot_analysis_info_macos.h"

#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>

BootAnalysisData BootAnalysisInfoMacOS::analyze() const
{
    BootAnalysisData result;

    // kern.boottime: { sec = 1745000000, usec = 123456 }
    QProcess p;
    p.start(QStringLiteral("sysctl"), {QStringLiteral("kern.boottime")});
    if (!p.waitForFinished(3000) || p.exitCode() != 0) {
        result.error = QStringLiteral("Could not read kern.boottime.");
        return result;
    }

    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    static const QRegularExpression re(QStringLiteral(R"(sec\s*=\s*(\d+))"));
    const auto m = re.match(out);
    if (!m.hasMatch()) {
        result.error = QStringLiteral("Could not parse kern.boottime output.");
        return result;
    }

    const qint64 bootSec = m.captured(1).toLongLong();
    const qint64 nowSec  = QDateTime::currentSecsSinceEpoch();
    result.totalBootMs = static_cast<double>(nowSec - bootSec) * 1000.0;

    // Per-service timing requires elevated privileges on macOS; not implemented.
    result.available = true;
    return result;
}
