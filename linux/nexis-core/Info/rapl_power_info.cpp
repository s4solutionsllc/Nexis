#include "rapl_power_info.h"

#include "Utils/file_util.h"

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>

quint64 RaplPowerInfo::energyDeltaUj(quint64 previousUj, quint64 currentUj, quint64 maxEnergyRangeUj)
{
    if (currentUj >= previousUj)
        return currentUj - previousUj;
    if (maxEnergyRangeUj == 0)
        return 0;
    return (maxEnergyRangeUj - previousUj) + currentUj;
}

void RaplPowerInfo::setPowercapRootForTesting(const QString &root)
{
    mPowercapRoot = root;
    mDiscovered = -1;
    mZones.clear();
}

void RaplPowerInfo::discoverZones()
{
    mZones.clear();

    QDir root(mPowercapRoot);
    if (!root.exists())
        return;

    // Top-level package zones look like "intel-rapl:0"; core/uncore
    // sub-zones are siblings named "intel-rapl:0:0" — the extra colon
    // excludes them so we don't double-count into the package total.
    static const QRegularExpression topLevel(R"(^[A-Za-z0-9_-]+:\d+$)");

    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (!topLevel.match(entry).hasMatch())
            continue;

        const QString dir = root.absoluteFilePath(entry);
        const QString name = FileUtil::readStringFromFile(dir + "/name").trimmed();
        if (!name.startsWith(QLatin1String("package"), Qt::CaseInsensitive))
            continue;

        ZoneState zone;
        zone.energyPath = dir + "/energy_uj";
        zone.name = name;
        const QString maxRange = FileUtil::readStringFromFile(dir + "/max_energy_range_uj").trimmed();
        zone.maxEnergyRangeUj = maxRange.toULongLong();
        mZones.append(zone);
    }
}

void RaplPowerInfo::update()
{
    if (mDiscovered == -1) {
        discoverZones();
        mDiscovered = mZones.isEmpty() ? 0 : 1;
    }

    if (mDiscovered == 0) {
        mSnapshot = RaplPowerSnapshot();
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    RaplPowerSnapshot snap;

    for (ZoneState &zone : mZones) {
        const QString raw = FileUtil::readStringFromFile(zone.energyPath).trimmed();
        bool ok = false;
        const quint64 energyUj = raw.toULongLong(&ok);
        if (!ok)
            continue;

        // energy_uj has been root-only on Intel since CVE-2020-8694 — zones
        // can be discovered (the name/max_energy_range_uj files are world
        // readable) while every energy_uj read still fails for a non-root
        // process. Only report available once a read actually succeeds.
        snap.available = true;

        RaplPackageSnapshot pkg;
        pkg.name = zone.name;

        if (zone.hasSample) {
            const qint64 elapsedMs = nowMs - zone.lastSampleMs;
            if (elapsedMs > 0) {
                const quint64 deltaUj = energyDeltaUj(zone.lastEnergyUj, energyUj, zone.maxEnergyRangeUj);
                pkg.watts = (static_cast<double>(deltaUj) / 1000000.0) / (static_cast<double>(elapsedMs) / 1000.0);
            }
        }

        zone.lastEnergyUj = energyUj;
        zone.lastSampleMs = nowMs;
        zone.hasSample = true;

        snap.totalPackageWatts += pkg.watts;
        snap.packages.append(pkg);
    }

    mSnapshot = snap;
}
