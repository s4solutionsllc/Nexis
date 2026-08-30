#include "menu_bar_format_util.h"

#include <algorithm>

QString MenuBarFormatUtil::formatTitle(int cpuPercent, int memPercent)
{
    const int cpu = std::clamp(cpuPercent, 0, 100);
    const int mem = std::clamp(memPercent, 0, 100);
    return QString("C %1%  M %2%").arg(cpu).arg(mem);
}

QString MenuBarFormatUtil::formatHealthTitle(int healthScore, const QString &scoreLabel)
{
    const int score = std::clamp(healthScore, 0, 100);
    if (scoreLabel.isEmpty())
        return QString("Health %1").arg(score);
    return QString("Health %1 · %2").arg(score).arg(scoreLabel);
}
