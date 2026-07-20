#include "menu_bar_format_util.h"

#include <algorithm>

QString MenuBarFormatUtil::formatTitle(int cpuPercent, int memPercent)
{
    const int cpu = std::clamp(cpuPercent, 0, 100);
    const int mem = std::clamp(memPercent, 0, 100);
    return QString("C %1%  M %2%").arg(cpu).arg(mem);
}
