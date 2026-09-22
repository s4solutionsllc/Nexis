#include "utilities.h"

#ifdef Q_OS_MAC
#include "macos_window_helper.h"
#endif

bool Utilities::prefersReducedMotion()
{
#ifdef Q_OS_MAC
    return nexis_macos_prefers_reduced_motion() != 0;
#else
    return false;
#endif
}
