#include "thermal_info.h"

#include <IOKit/IOKitLib.h>
#include <cstring>

// ── Apple SMC (System Management Controller) interface ──────────────
// The SMC exposes thermal sensors via a key-value store.  We talk to it
// through the AppleSMC IOService using IOKit.

#define SMC_KEY_CPU_TEMP  "TC0P"   // CPU proximity (Intel & Apple Silicon)
#define SMC_KEY_CPU_DIE   "TC0D"   // CPU die temperature (Intel)
#define SMC_KEY_GPU_TEMP  "TG0P"   // GPU proximity

// SMC command constants
#define KERNEL_INDEX_SMC  2

enum SmcCmd : uint8_t {
    kSMCReadKey  = 5,
    kSMCGetKeyInfo = 9
};

// SMC data structures (must match kernel driver layout)
#pragma pack(push, 1)

struct SmcKeyData {
    uint32_t key;
    struct {
        uint8_t  dataSize;
        uint32_t dataType;
        uint8_t  dataAttributes;
    } keyInfo;
    uint8_t  result;
    uint8_t  status;
    uint8_t  data8;
    uint32_t data32;
    uint8_t  bytes[32];
};

using SmcBytes = uint8_t[32];

struct SmcVal {
    char     key[5];
    uint32_t dataSize;
    char     dataType[5];
    SmcBytes bytes;
};

#pragma pack(pop)

// ── SMC helper functions ────────────────────────────────────────────

static io_connect_t sConn = 0;

static uint32_t fourCharToUInt(const char *s)
{
    return (uint32_t(s[0]) << 24) |
           (uint32_t(s[1]) << 16) |
           (uint32_t(s[2]) <<  8) |
            uint32_t(s[3]);
}

static bool smcOpen()
{
    if (sConn) return true;

    io_service_t svc = IOServiceGetMatchingService(
        kIOMainPortDefault,                         // was kIOMasterPortDefault
        IOServiceMatching("AppleSMC"));
    if (!svc) return false;

    kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &sConn);
    IOObjectRelease(svc);
    return kr == KERN_SUCCESS;
}

static bool smcReadKey(const char *key, SmcVal &val)
{
    if (!smcOpen()) return false;

    SmcKeyData inData{};
    SmcKeyData outData{};

    inData.key  = fourCharToUInt(key);
    inData.data8 = kSMCGetKeyInfo;

    size_t outSize = sizeof(SmcKeyData);
    kern_return_t kr = IOConnectCallStructMethod(
        sConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcKeyData),
        &outData, &outSize);
    if (kr != KERN_SUCCESS) return false;

    // Now read the actual value
    inData.keyInfo.dataSize = outData.keyInfo.dataSize;
    inData.data8 = kSMCReadKey;

    outSize = sizeof(SmcKeyData);
    kr = IOConnectCallStructMethod(
        sConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcKeyData),
        &outData, &outSize);
    if (kr != KERN_SUCCESS) return false;

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

static double smcGetTemp(const char *key)
{
    SmcVal val{};
    if (!smcReadKey(key, val))
        return -1.0;

    // Most temp keys use "sp78" (signed fixed-point 7.8) or "flt " (float)
    if (std::strcmp(val.dataType, "sp78") == 0 && val.dataSize == 2) {
        int16_t raw = (int16_t(val.bytes[0]) << 8) | val.bytes[1];
        return raw / 256.0;
    }
    if (std::strcmp(val.dataType, "flt ") == 0 && val.dataSize == 4) {
        float f;
        std::memcpy(&f, val.bytes, 4);
        return double(f);
    }
    return -1.0;
}

// ── ThermalInfo implementation ──────────────────────────────────────

ThermalInfo::ThermalInfo()
{
    discoverSensors();
}

void ThermalInfo::discoverSensors()
{
    mSensors.clear();

    // Try the well-known SMC thermal keys.
    // We probe each; if the read succeeds and returns a sane value we
    // register the sensor.
    struct Probe {
        const char *smcKey;
        const char *label;
    };

    static const Probe probes[] = {
        { SMC_KEY_CPU_TEMP,  "CPU Proximity" },
        { SMC_KEY_CPU_DIE,   "CPU Die"       },
        { SMC_KEY_GPU_TEMP,  "GPU Proximity" },
    };

    for (const auto &p : probes) {
        double t = smcGetTemp(p.smcKey);
        if (t > 0.0 && t < 130.0) {           // sane range
            ThermalSensor sensor;
            sensor.id         = QString("smc/%1").arg(p.smcKey);
            sensor.deviceName = "SMC";
            sensor.label      = QString(p.label);
            sensor.inputPath  = QString(p.smcKey);   // store key for later reads
            sensor.maxTemp    = 100.0;
            sensor.critTemp   = 105.0;
            mSensors.append(sensor);
        }
    }
}

QList<ThermalSensor> ThermalInfo::getSensors() const
{
    return mSensors;
}

double ThermalInfo::getTemperature(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0.0;

    double t = smcGetTemp(mSensors.at(index).inputPath.toLatin1().constData());
    return (t > 0.0) ? t : 0.0;
}

bool ThermalInfo::hasSensors() const
{
    return !mSensors.isEmpty();
}
