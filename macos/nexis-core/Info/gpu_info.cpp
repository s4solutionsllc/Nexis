#include "gpu_info.h"

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

GpuInfo::GpuInfo()
{
    discoverGpus();
}

/**
 * Walk the IORegistry tree to find the "model" property on or above
 * the given IOAccelerator entry.  Returns empty string if not found.
 */
static QString getModelName(io_object_t accel)
{
    // First try the accelerator object itself
    CFTypeRef modelRef = IORegistryEntryCreateCFProperty(
        accel, CFSTR("model"), kCFAllocatorDefault, 0);

    if (!modelRef) {
        // Try the parent (GPU device is typically one level up)
        io_object_t parent = 0;
        if (IORegistryEntryGetParentEntry(accel, kIOServicePlane, &parent) == KERN_SUCCESS) {
            modelRef = IORegistryEntryCreateCFProperty(
                parent, CFSTR("model"), kCFAllocatorDefault, 0);
            IOObjectRelease(parent);
        }
    }

    QString name;
    if (modelRef) {
        if (CFGetTypeID(modelRef) == CFDataGetTypeID()) {
            CFDataRef data = (CFDataRef)modelRef;
            name = QString::fromUtf8(
                (const char *)CFDataGetBytePtr(data),
                (int)CFDataGetLength(data)).trimmed();
            // Remove trailing null if present
            name = name.replace(QChar('\0'), QString());
        } else if (CFGetTypeID(modelRef) == CFStringGetTypeID()) {
            CFStringRef str = (CFStringRef)modelRef;
            char buf[256];
            if (CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8))
                name = QString::fromUtf8(buf);
        }
        CFRelease(modelRef);
    }

    return name;
}

/**
 * Detect vendor from class-code or model name.
 */
static QString detectVendor(io_object_t accel, const QString &modelName)
{
    // Check vendor-id property
    CFTypeRef vendorRef = IORegistryEntryCreateCFProperty(
        accel, CFSTR("vendor-id"), kCFAllocatorDefault, 0);

    if (!vendorRef) {
        // Try parent
        io_object_t parent = 0;
        if (IORegistryEntryGetParentEntry(accel, kIOServicePlane, &parent) == KERN_SUCCESS) {
            vendorRef = IORegistryEntryCreateCFProperty(
                parent, CFSTR("vendor-id"), kCFAllocatorDefault, 0);
            IOObjectRelease(parent);
        }
    }

    if (vendorRef) {
        if (CFGetTypeID(vendorRef) == CFDataGetTypeID()) {
            CFDataRef data = (CFDataRef)vendorRef;
            if (CFDataGetLength(data) >= 2) {
                const uint8_t *bytes = CFDataGetBytePtr(data);
                uint16_t vid = bytes[0] | (bytes[1] << 8);  // little-endian
                CFRelease(vendorRef);
                if (vid == 0x1002) return "AMD";
                if (vid == 0x10de) return "NVIDIA";
                if (vid == 0x8086) return "Intel";
                return "Unknown";
            }
        }
        CFRelease(vendorRef);
    }

    // Heuristic fallback from model name
    QString lower = modelName.toLower();
    if (lower.contains("apple"))   return "Apple";
    if (lower.contains("amd") || lower.contains("radeon")) return "AMD";
    if (lower.contains("nvidia") || lower.contains("geforce") || lower.contains("quadro")) return "NVIDIA";
    if (lower.contains("intel"))   return "Intel";

    return "Apple";  // On Apple Silicon, typically no vendor-id
}

void GpuInfo::discoverGpus()
{
    mDevices.clear();

    io_iterator_t iterator = 0;
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMainPortDefault,
        IOServiceMatching("IOAccelerator"),
        &iterator);

    if (kr != KERN_SUCCESS || !iterator)
        return;

    io_object_t accel;
    int idx = 0;
    while ((accel = IOIteratorNext(iterator)) != 0) {
        GpuDevice dev;
        dev.deviceIndex = idx++;
        dev.utilization = -1;

        dev.name = getModelName(accel);
        if (dev.name.isEmpty())
            dev.name = QString("GPU %1").arg(dev.deviceIndex);

        dev.vendor = detectVendor(accel, dev.name);

        mDevices.append(dev);
        IOObjectRelease(accel);
    }

    IOObjectRelease(iterator);
}

void GpuInfo::updateGpuInfo()
{
    // Re-enumerate IOAccelerators and read PerformanceStatistics
    io_iterator_t iterator = 0;
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMainPortDefault,
        IOServiceMatching("IOAccelerator"),
        &iterator);

    if (kr != KERN_SUCCESS || !iterator)
        return;

    io_object_t accel;
    int idx = 0;
    while ((accel = IOIteratorNext(iterator)) != 0) {
        if (idx < mDevices.size()) {
            int utilization = -1;

            // Read PerformanceStatistics dictionary
            CFMutableDictionaryRef props = nullptr;
            if (IORegistryEntryCreateCFProperties(accel, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
                CFDictionaryRef perfStats = (CFDictionaryRef)CFDictionaryGetValue(props, CFSTR("PerformanceStatistics"));
                if (perfStats && CFGetTypeID(perfStats) == CFDictionaryGetTypeID()) {

                    // Try several known keys for GPU utilization
                    static const CFStringRef utilKeys[] = {
                        CFSTR("Device Utilization %"),
                        CFSTR("GPU Core Utilization"),
                        CFSTR("GPU Activity(%)"),
                        CFSTR("gpuCoreUtilizationPercent"),
                    };

                    for (const auto &key : utilKeys) {
                        CFNumberRef val = (CFNumberRef)CFDictionaryGetValue(perfStats, key);
                        if (val && CFGetTypeID(val) == CFNumberGetTypeID()) {
                            int64_t num = 0;
                            CFNumberGetValue(val, kCFNumberSInt64Type, &num);
                            // Some keys report 0-100, others 0-10000000 (per-million)
                            if (num > 100) {
                                utilization = static_cast<int>(num / 10000000);
                            } else {
                                utilization = static_cast<int>(num);
                            }
                            utilization = qBound(0, utilization, 100);
                            break;
                        }
                    }
                }
                CFRelease(props);
            }

            mDevices[idx].utilization = utilization;
        }

        IOObjectRelease(accel);
        idx++;
    }

    IOObjectRelease(iterator);
}

// Getters (getGpuDevices, hasGpu) are in shared/nexis-core/Info/gpu_info_shared.cpp
