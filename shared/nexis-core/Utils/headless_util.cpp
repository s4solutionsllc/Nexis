#include "headless_util.h"

#include <cstring>

bool HeadlessUtil::isHeadlessArgv(int argc, char *const argv[])
{
    if (argc <= 1 || argv == nullptr) {
        return false;
    }
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (a == nullptr) continue;
        if (std::strcmp(a, "--clean") == 0 ||
            std::strcmp(a, "--check-threshold") == 0) {
            return true;
        }
    }
    return false;
}

bool HeadlessUtil::shouldForceOffscreen(bool isHeadless, bool qpaPlatformAlreadySet)
{
    return isHeadless && !qpaPlatformAlreadySet;
}
