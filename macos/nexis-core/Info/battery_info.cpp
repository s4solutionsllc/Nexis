#include "battery_info_macos.h"

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// Helper: read an integer value from a CFDictionary
static bool cfDictGetInt(CFDictionaryRef dict, CFStringRef key, int &out)
{
    CFTypeRef ref = CFDictionaryGetValue(dict, key);
    if (!ref || CFGetTypeID(ref) != CFNumberGetTypeID())
        return false;
    int64_t val = 0;
    CFNumberGetValue((CFNumberRef)ref, kCFNumberSInt64Type, &val);
    out = static_cast<int>(val);
    return true;
}

// Helper: read a boolean value from a CFDictionary
static bool cfDictGetBool(CFDictionaryRef dict, CFStringRef key, bool &out)
{
    CFTypeRef ref = CFDictionaryGetValue(dict, key);
    if (!ref)
        return false;
    if (CFGetTypeID(ref) == CFBooleanGetTypeID()) {
        out = CFBooleanGetValue((CFBooleanRef)ref);
        return true;
    }
    if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
        int val = 0;
        CFNumberGetValue((CFNumberRef)ref, kCFNumberIntType, &val);
        out = (val != 0);
        return true;
    }
    return false;
}

// Helper: derive condition string from health percentage
static QString deriveCondition(int healthPercent)
{
    if (healthPercent >= 80) return QStringLiteral("Good");
    if (healthPercent >= 60) return QStringLiteral("Fair");
    return QStringLiteral("Replace");
}

BatteryInfoMacOS::BatteryInfoMacOS()
{
    discoverBattery();
    if (mData.hasBattery)
        updateBatteryInfo();
}

void BatteryInfoMacOS::discoverBattery()
{
    mData = BatteryData();

    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("AppleSmartBattery"));

    if (!service) {
        mData.hasBattery = false;
        return;
    }

    // Check BatteryInstalled property
    CFMutableDictionaryRef props = nullptr;
    if (IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
        bool installed = false;
        cfDictGetBool(props, CFSTR("BatteryInstalled"), installed);
        mData.hasBattery = installed;
        CFRelease(props);
    }

    IOObjectRelease(service);
}

void BatteryInfoMacOS::updateBatteryInfo()
{
    if (!mData.hasBattery)
        return;

    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("AppleSmartBattery"));

    if (!service)
        return;

    CFMutableDictionaryRef props = nullptr;
    if (IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault, 0) != KERN_SUCCESS || !props) {
        IOObjectRelease(service);
        return;
    }

    int val = 0;

    // Cycle count
    if (cfDictGetInt(props, CFSTR("CycleCount"), val))
        mData.cycleCount = val;

    // Design cycle count (rated max, usually 1000)
    if (cfDictGetInt(props, CFSTR("DesignCycleCount9C"), val))
        mData.designCycleCount = val;

    // Design capacity (factory mAh)
    if (cfDictGetInt(props, CFSTR("DesignCapacity"), val))
        mData.designCapacityMah = val;

    // Max capacity (current maximum — degrades over time)
    // Apple Silicon uses AppleRawMaxCapacity; Intel uses MaxCapacity
    if (cfDictGetInt(props, CFSTR("AppleRawMaxCapacity"), val))
        mData.maxCapacityMah = val;
    else if (cfDictGetInt(props, CFSTR("MaxCapacity"), val))
        mData.maxCapacityMah = val;

    // Current capacity (current charge level)
    if (cfDictGetInt(props, CFSTR("CurrentCapacity"), val))
        mData.currentCapacityMah = val;

    // Temperature (reported in tenths of °C)
    if (cfDictGetInt(props, CFSTR("Temperature"), val))
        mData.temperatureCelsius = val / 10.0;

    // Voltage (millivolts)
    if (cfDictGetInt(props, CFSTR("Voltage"), val))
        mData.voltageMv = val;

    // Amperage (milliamps, negative = discharging)
    // Try InstantAmperage first (more accurate), fall back to Amperage
    if (cfDictGetInt(props, CFSTR("InstantAmperage"), val))
        mData.amperageMa = val;
    else if (cfDictGetInt(props, CFSTR("Amperage"), val))
        mData.amperageMa = val;

    // Charging state
    bool boolVal = false;
    if (cfDictGetBool(props, CFSTR("IsCharging"), boolVal))
        mData.isCharging = boolVal;

    if (cfDictGetBool(props, CFSTR("ExternalConnected"), boolVal))
        mData.isPluggedIn = boolVal;

    bool fullyCharged = false;
    cfDictGetBool(props, CFSTR("FullyCharged"), fullyCharged);

    // Time remaining (minutes; 65535 means "calculating")
    if (cfDictGetInt(props, CFSTR("TimeRemaining"), val))
        mData.timeRemainingMinutes = (val == 65535) ? -1 : val;

    // Manufacture date (packed: year[15:9]+1980, month[8:5], day[4:0])
    if (cfDictGetInt(props, CFSTR("ManufactureDate"), val) && val > 0) {
        int year  = ((val >> 9) & 0x7F) + 1980;
        int month = (val >> 5) & 0x0F;
        int day   = val & 0x1F;
        QDate date(year, month, day);
        if (date.isValid())
            mData.manufactureDate = date;
    }

    // Derive health percentage
    if (mData.maxCapacityMah > 0 && mData.designCapacityMah > 0)
        mData.healthPercent = qBound(0, static_cast<int>((mData.maxCapacityMah / mData.designCapacityMah) * 100.0), 100);

    // Derive charge percentage
    if (mData.currentCapacityMah >= 0 && mData.maxCapacityMah > 0)
        mData.chargePercent = qBound(0, static_cast<int>((mData.currentCapacityMah / mData.maxCapacityMah) * 100.0), 100);

    // Derive power (watts) from voltage and amperage
    if (mData.voltageMv > 0 && mData.amperageMa != 0.0)
        mData.powerWatts = qAbs(mData.voltageMv * mData.amperageMa) / 1.0e6;

    // Derive condition
    if (mData.healthPercent >= 0)
        mData.condition = deriveCondition(mData.healthPercent);

    // Derive status string
    if (fullyCharged)
        mData.status = QStringLiteral("Full");
    else if (mData.isCharging)
        mData.status = QStringLiteral("Charging");
    else if (mData.isPluggedIn)
        mData.status = QStringLiteral("Not charging");
    else
        mData.status = QStringLiteral("Discharging");

    // macOS doesn't expose manufacturer/model/technology via IOKit battery service
    // Leave them empty (they're populated on Linux only)

    CFRelease(props);
    IOObjectRelease(service);
}
