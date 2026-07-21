#ifndef RAPL_POWER_SNAPSHOT_H
#define RAPL_POWER_SNAPSHOT_H

#include <QList>
#include <QMetaType>
#include <QString>

// SSO-15378: package power draw via Linux powercap/RAPL
// (/sys/class/powercap/intel-rapl:*). The same "intel-rapl" driver name is
// registered by the kernel's RAPL powercap framework on supported AMD hosts
// too, so no separate AMD backend is needed.
//
// Lives in shared/ (mirrors oomd_snapshot.h) so PowerDrawWidget can include
// it unconditionally — the producer (RaplPowerInfo) is Linux-only, but the
// widget still needs to compile on the macOS build target even though
// nothing wires it up there.
struct RaplPackageSnapshot {
    QString name;          // sysfs zone name, e.g. "package-0"
    double  watts = 0.0;   // average draw since the previous sample
};

struct RaplPowerSnapshot {
    // False when the host has no powercap RAPL zones at all (no driver
    // loaded, virtualized guest, non-x86, etc). UI hides the panel entirely
    // when !available rather than showing a broken/zeroed reading.
    bool available = false;

    double totalPackageWatts = 0.0;
    QList<RaplPackageSnapshot> packages;
};

Q_DECLARE_METATYPE(RaplPackageSnapshot)
Q_DECLARE_METATYPE(RaplPowerSnapshot)

#endif // RAPL_POWER_SNAPSHOT_H
