#ifndef MENU_BAR_FORMAT_UTIL_H
#define MENU_BAR_FORMAT_UTIL_H

#include "nexis-core_global.h"

#include <QString>

// FW-20: formats the compact CPU/memory string shown in the macOS
// NSStatusItem title. Kept separate from the AppKit bridge (macos/nexis/MenuBar)
// so it is unit-testable without a native menu bar.
class NEXISCORESHARED_EXPORT MenuBarFormatUtil
{
public:
    static QString formatTitle(int cpuPercent, int memPercent);

    // SSO-23853: formats the Dashboard health score (same 0-100 composite
    // value/label as HealthScoreCalculator::compositeScore()/scoreLabel())
    // for the menu-bar title, replacing the raw CPU/MEM readout.
    static QString formatHealthTitle(int healthScore, const QString &scoreLabel);
};

#endif // MENU_BAR_FORMAT_UTIL_H
