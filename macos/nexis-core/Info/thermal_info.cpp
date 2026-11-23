#include "thermal_info_macos.h"
#include "smc_protocol.h"

#include <IOKit/IOKitLib.h>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QtGlobal>
#include <atomic>
#include <cstring>
#include <mutex>

// ── Apple SMC (System Management Controller) interface ──────────────
// The SMC exposes thermal sensors via a key-value store.  We talk to it
// through the AppleSMC IOService using IOKit.

#define SMC_KEY_CPU_TEMP  "TC0P"   // CPU proximity (Intel & Apple Silicon)
#define SMC_KEY_CPU_DIE   "TC0D"   // CPU die temperature (Intel)
#define SMC_KEY_GPU_TEMP  "TG0P"   // GPU proximity

using SmcBytes = uint8_t[32];

struct SmcVal {
    char     key[5];
    uint32_t dataSize;
    char     dataType[5];
    SmcBytes bytes;
};

// ── SMC helper functions ────────────────────────────────────────────
// WI-23 (M4): the wizard health-score worker and the medium-tick UI thread
// both sample temperatures, so the previous static `sConn` check-then-open
// could double-open the IOService and the IOConnectCallStructMethod call
// itself is not safe for concurrent use on one connection. Open exactly
// once via std::call_once and serialize reads with a process-wide mutex.

static io_connect_t sConn = 0;
static std::once_flag sSmcOpenFlag;
static QMutex sSmcReadMutex;
static std::atomic<unsigned> sSmcOpenAttempts{0};   // test seam (see test_thermal_info_macos)

static uint32_t fourCharToUInt(const char *s)
{
    return (uint32_t(s[0]) << 24) |
           (uint32_t(s[1]) << 16) |
           (uint32_t(s[2]) <<  8) |
            uint32_t(s[3]);
}

static bool smcOpen()
{
    std::call_once(sSmcOpenFlag, []() {
        sSmcOpenAttempts.fetch_add(1, std::memory_order_relaxed);

        io_service_t svc = IOServiceGetMatchingService(
            kIOMainPortDefault,                         // was kIOMasterPortDefault
            IOServiceMatching("AppleSMC"));
        if (!svc) return;

        kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &sConn);
        IOObjectRelease(svc);
        if (kr != KERN_SUCCESS)
            sConn = 0;
    });
    return sConn != 0;
}

// Test seams (tests/core/test_thermal_info_macos.cpp forward-declares these).
// Keeping them as plain non-static free functions in this TU avoids leaking
// the SMC internals into a public header just to satisfy a test.
unsigned nexis_smcOpenAttemptsForTest()
{
    return sSmcOpenAttempts.load(std::memory_order_relaxed);
}

void nexis_smcForceOpenForTest()
{
    smcOpen();
}

static bool smcReadKey(const char *key, SmcVal &val)
{
    if (!smcOpen()) return false;

    // AppleSMC IOConnectCallStructMethod sequences a get-info then a read
    // against a single shared connection; concurrent callers from the UI
    // and wizard worker threads can interleave the two halves. Serialize
    // the whole pair.
    QMutexLocker locker(&sSmcReadMutex);

    SmcKeyData inData{};
    SmcKeyData outData{};

    inData.key  = fourCharToUInt(key);
    inData.data8 = kSMCGetKeyInfo;

    size_t outSize = sizeof(SmcKeyData);
    kern_return_t kr = IOConnectCallStructMethod(
        sConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcKeyData),
        &outData, &outSize);
    if (kr != KERN_SUCCESS || outData.result != kSMCResultSuccess) return false;

    // Now read the actual value
    inData.keyInfo.dataSize = outData.keyInfo.dataSize;
    inData.data8 = kSMCReadKey;

    outSize = sizeof(SmcKeyData);
    kr = IOConnectCallStructMethod(
        sConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcKeyData),
        &outData, &outSize);
    if (kr != KERN_SUCCESS || outData.result != kSMCResultSuccess) return false;

    std::strncpy(val.key, key, 4);
    val.key[4] = '\0';
    val.dataSize = outData.keyInfo.dataSize;

    uint32_t dt = outData.keyInfo.dataType;
    val.dataType[0] = char(dt >> 24);
    val.dataType[1] = char(dt >> 16);
    val.dataType[2] = char(dt >>  8);
    val.dataType[3] = char(dt);
    val.dataType[4] = '\0';

    std::memcpy(val.bytes, outData.bytes, sizeof(val.bytes));
    return true;
}

// Resolve the SMC key at `index` in the key table (selector kSMCGetKeyFromIndex).
// Apple Silicon parts carry different T* key sets per generation, so
// enumeration, not a hard-coded list, is what finds their sensors.
static bool smcKeyAtIndex(uint32_t index, char out[5])
{
    if (!smcOpen()) return false;
    QMutexLocker locker(&sSmcReadMutex);

    SmcKeyData inData{};
    SmcKeyData outData{};
    inData.data8  = kSMCGetKeyFromIndex;
    inData.data32 = index;

    size_t outSize = sizeof(SmcKeyData);
    kern_return_t kr = IOConnectCallStructMethod(
        sConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcKeyData),
        &outData, &outSize);
    if (kr != KERN_SUCCESS || outData.result != kSMCResultSuccess) return false;

    out[0] = char(outData.key >> 24);
    out[1] = char(outData.key >> 16);
    out[2] = char(outData.key >>  8);
    out[3] = char(outData.key);
    out[4] = '\0';
    return true;
}

static int smcKeyCount()
{
    SmcVal val{};
    if (!smcReadKey("#KEY", val) || val.dataSize != 4) return 0;
    return int((uint32_t(val.bytes[0]) << 24) | (uint32_t(val.bytes[1]) << 16) |
               (uint32_t(val.bytes[2]) <<  8) |  uint32_t(val.bytes[3]));
}

static double smcGetTemp(const char *key)
{
    SmcVal val{};
    if (!smcReadKey(key, val))
        return -1.0;

    return ThermalInfo::decodeSmcTemperature(
        QByteArray(val.dataType, 4),
        QByteArray(reinterpret_cast<const char *>(val.bytes), int(val.dataSize)));
}

// ── ThermalInfo implementation ──────────────────────────────────────

ThermalInfoMacOS::ThermalInfoMacOS()
{
    discoverSensors();
}

void ThermalInfoMacOS::discoverSensors()
{
    mSensors.clear();

    // Well-known keys first, so the labels users already see on Intel and
    // earlier Apple Silicon Macs stay stable.
    struct Probe {
        const char *smcKey;
        const char *label;
    };

    static const Probe probes[] = {
        { SMC_KEY_CPU_TEMP,  "CPU Proximity" },
        { SMC_KEY_CPU_DIE,   "CPU Die"       },
        { SMC_KEY_GPU_TEMP,  "GPU Proximity" },
    };

    QSet<QString> seen;
    auto addSensor = [&](const QString &key, const QString &label) {
        ThermalSensor sensor;
        sensor.id         = QString("smc/%1").arg(key);
        sensor.deviceName = "SMC";
        sensor.label      = label;
        sensor.inputPath  = key;   // store key for later reads
        sensor.maxTemp    = 100.0;
        sensor.critTemp   = 105.0;
        mSensors.append(sensor);
        seen.insert(key);
    };

    for (const auto &p : probes) {
        double t = smcGetTemp(p.smcKey);
        if (ThermalInfo::isPlausibleSmcTemperature(t))
            addSensor(p.smcKey, p.label);
    }

    // Walk the SMC key table for the remaining T* keys (Apple Silicon P/E-core,
    // GPU, memory, SSD, battery ...). Bounded so a misbehaving SMC can't stall startup.
    constexpr int kMaxKeysScanned = 4096;
    constexpr int kMaxSensors     = 64;
    const int total = qMin(smcKeyCount(), kMaxKeysScanned);
    for (int i = 0; i < total && mSensors.size() < kMaxSensors; ++i) {
        char key[5];
        if (!smcKeyAtIndex(uint32_t(i), key) || key[0] != 'T')
            continue;
        const QString qkey = QString::fromLatin1(key, 4);
        if (seen.contains(qkey))
            continue;
        if (ThermalInfo::isPlausibleSmcTemperature(smcGetTemp(key)))
            addSensor(qkey, ThermalInfo::smcSensorLabel(qkey));
    }

    qInfo("ThermalInfoMacOS: %lld SMC temperature sensor(s) of %d SMC keys",
          static_cast<long long>(mSensors.size()), total);
}

double ThermalInfoMacOS::getTemperature(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0.0;

    double t = smcGetTemp(mSensors.at(index).inputPath.toLatin1().constData());
    return (t > 0.0) ? t : 0.0;
}

