#ifndef BOOT_ANALYSIS_INFO_H
#define BOOT_ANALYSIS_INFO_H

#include <QString>
#include <QList>

struct BootEntry {
    QString name;
    double  durationMs = 0.0;
    QString impact;   // "High" / "Medium" / "Low"
};

struct BootAnalysisData {
    bool            available  = false;
    QString         error;
    double          totalBootMs = 0.0;
    QList<BootEntry> entries;   // sorted by durationMs descending
};

class BootAnalysisInfo
{
public:
    virtual ~BootAnalysisInfo() = default;

    virtual BootAnalysisData analyze() const = 0;

    static QString impactFor(double ms)
    {
        if (ms >= 5000.0) return QStringLiteral("High");
        if (ms >= 1000.0) return QStringLiteral("Medium");
        return QStringLiteral("Low");
    }
};

#endif // BOOT_ANALYSIS_INFO_H
