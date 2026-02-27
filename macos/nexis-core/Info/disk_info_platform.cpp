#include "disk_info_macos.h"
#include <QDir>

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

QList<quint64> DiskInfoMacOS::getDiskIO() const
{
    quint64 totalRead = 0;
    quint64 totalWrite = 0;

    io_iterator_t iter = 0;
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMainPortDefault,
        IOServiceMatching("IOBlockStorageDriver"),
        &iter);

    if (kr == KERN_SUCCESS && iter) {
        io_object_t driver;
        while ((driver = IOIteratorNext(iter)) != 0) {
            CFMutableDictionaryRef props = nullptr;
            if (IORegistryEntryCreateCFProperties(driver, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
                CFDictionaryRef stats = static_cast<CFDictionaryRef>(
                    CFDictionaryGetValue(props, CFSTR("Statistics")));
                if (stats && CFGetTypeID(stats) == CFDictionaryGetTypeID()) {
                    CFNumberRef readRef = static_cast<CFNumberRef>(
                        CFDictionaryGetValue(stats, CFSTR("Bytes (Read)")));
                    CFNumberRef writeRef = static_cast<CFNumberRef>(
                        CFDictionaryGetValue(stats, CFSTR("Bytes (Write)")));

                    int64_t r = 0, w = 0;
                    if (readRef)
                        CFNumberGetValue(readRef, kCFNumberSInt64Type, &r);
                    if (writeRef)
                        CFNumberGetValue(writeRef, kCFNumberSInt64Type, &w);

                    totalRead += static_cast<quint64>(r);
                    totalWrite += static_cast<quint64>(w);
                }
                CFRelease(props);
            }
            IOObjectRelease(driver);
        }
        IOObjectRelease(iter);
    }

    // Delta-compute rates from cumulative counters
    quint64 readRate = 0;
    quint64 writeRate = 0;

    if (!mIoTimerStarted) {
        mIoTimer.start();
        mIoTimerStarted = true;
    } else {
        double elapsed = mIoTimer.elapsed() / 1000.0;
        mIoTimer.restart();
        if (elapsed > 0 && totalRead >= mPrevRead && totalWrite >= mPrevWrite) {
            readRate = static_cast<quint64>((totalRead - mPrevRead) / elapsed);
            writeRate = static_cast<quint64>((totalWrite - mPrevWrite) / elapsed);
        }
    }

    mPrevRead = totalRead;
    mPrevWrite = totalWrite;

    return {readRate, writeRate};
}

QStringList DiskInfoMacOS::getDiskNames() const
{
    QStringList diskNames;
    QDir devDir("/dev");
    QStringList entries = devDir.entryList({"disk*"}, QDir::System);
    for (const QString &entry : entries) {
        if (!entry.contains('s'))
            diskNames.append(entry);
    }
    return diskNames;
}
