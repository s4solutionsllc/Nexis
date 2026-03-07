#ifndef MEMORYINFO_H
#define MEMORYINFO_H

#include "Utils/file_util.h"

#include "nexis-core_global.h"

#include <QMetaType>
#include <QMap>

struct MemorySnapshot {
    quint64 used = 0;
    quint64 total = 0;
    quint64 swapUsed = 0;
    quint64 swapTotal = 0;
    quint64 wired = 0;
    quint64 active = 0;
    quint64 inactive = 0;
    quint64 compressed = 0;
    quint64 available = 0;
    int pressureLevel = -1;  // -1=unavailable, 1=normal, 2=warning, 4=critical
};
Q_DECLARE_METATYPE(MemorySnapshot)

struct MemoryParseResult {
    quint64 memTotal = 0;
    quint64 memFree = 0;
    quint64 memUsed = 0;
    quint64 buffers = 0;
    quint64 cached = 0;
    quint64 sreclaimable = 0;
    quint64 shmem = 0;
    quint64 swapTotal = 0;
    quint64 swapFree = 0;
    quint64 swapUsed = 0;
    quint64 memAvailable = 0;
    quint64 memActive = 0;
    quint64 memInactive = 0;
};

class NEXISCORESHARED_EXPORT MemoryInfo
{
public:
    MemoryInfo();
    virtual ~MemoryInfo() = default;

    virtual void updateMemoryInfo() = 0;

    quint64 getMemTotal() const;
    quint64 getMemFree() const;
    quint64 getMemUsed() const;

    quint64 getSwapTotal() const;
    quint64 getSwapFree() const;
    quint64 getSwapUsed() const;

    quint64 getMemWired() const;
    quint64 getMemActive() const;
    quint64 getMemInactive() const;
    quint64 getMemCompressed() const;
    quint64 getMemAvailable() const;
    int getPressureLevel() const;

    // Static parsing methods for testability (FR-76).
    // Accepts raw /proc/meminfo lines, returns parsed fields.
    static MemoryParseResult parseProcMeminfo(const QStringList &lines);

    // Computes derived values (memUsed, swapUsed, adjusted cached)
    // with underflow guards.
    static void deriveMemoryValues(MemoryParseResult &r);

    // Parses PSI content or uses MemAvailable heuristic to determine
    // pressure level. Returns -1 (unavailable), 1 (normal), 2 (warning), 4 (critical).
    static int parsePressureLevel(const QString &psiContent,
                                  quint64 memAvailable, quint64 memTotal);

protected:
    quint64 memTotal;
    quint64 memFree;
    quint64 memUsed;
    quint64 buffers;
    quint64 cached;
    quint64 sreclaimable;
    quint64 shmem;

    quint64 swapTotal;
    quint64 swapFree;
    quint64 swapUsed;

    quint64 memWired;
    quint64 memActive;
    quint64 memInactive;
    quint64 memCompressed;
    quint64 memAvailable;
    int pressureLevel;
};

#endif // MEMORYINFO_H
