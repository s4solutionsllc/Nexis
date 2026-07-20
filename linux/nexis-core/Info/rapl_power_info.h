#ifndef RAPL_POWER_INFO_H
#define RAPL_POWER_INFO_H

#include <QList>
#include <QString>

#include "rapl_power_snapshot.h"

// SSO-15378: per-package power draw via Linux powercap/RAPL
// (/sys/class/powercap/intel-rapl:*). The same "intel-rapl" driver name is
// registered by the kernel's RAPL powercap framework on supported AMD hosts
// too, so no separate AMD backend is needed — we just glob the class
// directory rather than hardcoding a vendor prefix.
class RaplPowerInfo
{
public:
    void update();
    RaplPowerSnapshot getSnapshot() const { return mSnapshot; }
    bool isAvailable() const { return mSnapshot.available; }

    // Pure helper, testable without touching the filesystem. energy_uj is a
    // monotonically increasing microjoule counter that wraps back to 0 once
    // it passes maxEnergyRangeUj; this recovers the true delta across a
    // wrap. Returns 0 if maxEnergyRangeUj is 0 (can't infer wrap distance).
    static quint64 energyDeltaUj(quint64 previousUj, quint64 currentUj, quint64 maxEnergyRangeUj);

private:
    struct ZoneState {
        QString energyPath;
        QString name;
        quint64 maxEnergyRangeUj = 0;
        quint64 lastEnergyUj = 0;
        qint64  lastSampleMs = 0;
        bool    hasSample = false;
    };

    // -1 = uncached, 0 = known unavailable, 1 = known available — mirrors
    // CpuInfoLinux::mSysfsCpufreqAvailable so a RAPL-less host doesn't re-glob
    // /sys/class/powercap on every tick.
    int mDiscovered = -1;
    QList<ZoneState> mZones;
    RaplPowerSnapshot mSnapshot;

    void discoverZones();
};

#endif // RAPL_POWER_INFO_H
