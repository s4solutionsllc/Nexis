#ifndef SMC_PROTOCOL_H
#define SMC_PROTOCOL_H

#include <cstdint>

// AppleSMC user-client wire format used by thermal_info.cpp.
// The kernel validates the struct size on selector 2 and answers
// kIOReturnBadArgument for anything but 80 bytes, so every field below
// (including the ones Nexis never reads) has to be present, in this order,
// with natural alignment. Do not add #pragma pack here.

#define KERNEL_INDEX_SMC 2

enum SmcCmd : uint8_t {
    kSMCReadKey      = 5,
    kSMCGetKeyFromIndex = 8,
    kSMCGetKeyInfo   = 9
};

enum SmcResult : uint8_t {
    kSMCResultSuccess  = 0,
    kSMCResultKeyNotFound = 0x84
};

struct SmcKeyData {
    uint32_t key;
    struct {
        uint8_t  major;
        uint8_t  minor;
        uint8_t  build;
        uint8_t  reserved;
        uint16_t release;
    } vers;
    struct {
        uint16_t version;
        uint16_t length;
        uint32_t cpuPLimit;
        uint32_t gpuPLimit;
        uint32_t memPLimit;
    } pLimitData;
    struct {
        uint32_t dataSize;
        uint32_t dataType;
        uint8_t  dataAttributes;
    } keyInfo;
    uint8_t  result;
    uint8_t  status;
    uint8_t  data8;
    uint32_t data32;
    uint8_t  bytes[32];
};

static_assert(sizeof(SmcKeyData) == 80, "AppleSMC expects an 80-byte SMCKeyData");

#endif // SMC_PROTOCOL_H
