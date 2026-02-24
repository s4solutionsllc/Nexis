#include "fan_info_macos.h"

#include <IOKit/IOKitLib.h>
#include <cstring>

#define KERNEL_INDEX_SMC  2

enum SmcFanCmd : uint8_t {
    kSMCReadKey    = 5,
    kSMCGetKeyInfo = 9
};

#pragma pack(push, 1)

struct SmcFanKeyData {
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

using SmcFanBytes = uint8_t[32];

struct SmcFanVal {
    char     key[5];
    uint32_t dataSize;
    char     dataType[5];
    SmcFanBytes bytes;
};

#pragma pack(pop)

static io_connect_t sFanConn = 0;

static uint32_t fanFourCharToUInt(const char *s)
{
    return (uint32_t(s[0]) << 24) |
           (uint32_t(s[1]) << 16) |
           (uint32_t(s[2]) <<  8) |
            uint32_t(s[3]);
}

static bool smcFanOpen()
{
    if (sFanConn) return true;

    io_service_t svc = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("AppleSMC"));
    if (!svc) return false;

    kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &sFanConn);
    IOObjectRelease(svc);
    return kr == KERN_SUCCESS;
}

static bool smcFanReadKey(const char *key, SmcFanVal &val)
{
    if (!smcFanOpen()) return false;

    SmcFanKeyData inData{};
    SmcFanKeyData outData{};

    inData.key  = fanFourCharToUInt(key);
    inData.data8 = kSMCGetKeyInfo;

    size_t outSize = sizeof(SmcFanKeyData);
    kern_return_t kr = IOConnectCallStructMethod(
        sFanConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcFanKeyData),
        &outData, &outSize);
    if (kr != KERN_SUCCESS) return false;

    inData.keyInfo.dataSize = outData.keyInfo.dataSize;
    inData.data8 = kSMCReadKey;

    outSize = sizeof(SmcFanKeyData);
    kr = IOConnectCallStructMethod(
        sFanConn, KERNEL_INDEX_SMC,
        &inData, sizeof(SmcFanKeyData),
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

static int smcGetFanRpm(const char *key)
{
    SmcFanVal val{};
    if (!smcFanReadKey(key, val))
        return -1;

    if (std::strcmp(val.dataType, "fpe2") == 0 && val.dataSize == 2) {
        uint16_t raw = (uint16_t(val.bytes[0]) << 8) | val.bytes[1];
        return static_cast<int>(raw >> 2);
    }
    return -1;
}

static int smcGetUInt8(const char *key)
{
    SmcFanVal val{};
    if (!smcFanReadKey(key, val))
        return -1;

    if (val.dataSize >= 1)
        return val.bytes[0];
    return -1;
}

FanInfoMacOS::FanInfoMacOS()
{
    discoverSensors();
}

void FanInfoMacOS::discoverSensors()
{
    mSensors.clear();

    int numFans = smcGetUInt8("FNum");
    if (numFans <= 0)
        return;

    for (int i = 0; i < numFans && i < 8; ++i) {
        char acKey[5];
        std::snprintf(acKey, sizeof(acKey), "F%dAc", i);

        int rpm = smcGetFanRpm(acKey);
        if (rpm < 0)
            continue;

        char mnKey[5], mxKey[5];
        std::snprintf(mnKey, sizeof(mnKey), "F%dMn", i);
        std::snprintf(mxKey, sizeof(mxKey), "F%dMx", i);

        int minRpm = smcGetFanRpm(mnKey);
        int maxRpm = smcGetFanRpm(mxKey);

        FanSensor sensor;
        sensor.id         = QString("smc/F%1Ac").arg(i);
        sensor.deviceName = "SMC";
        sensor.label      = QString("Fan %1").arg(i + 1);
        sensor.inputPath  = QString(acKey);
        sensor.minRpm     = (minRpm >= 0) ? minRpm : -1;
        sensor.maxRpm     = (maxRpm >= 0) ? maxRpm : -1;

        mSensors.append(sensor);
    }
}

int FanInfoMacOS::getFanSpeed(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0;

    int rpm = smcGetFanRpm(mSensors.at(index).inputPath.toLatin1().constData());
    return (rpm >= 0) ? rpm : 0;
}
