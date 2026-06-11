#ifndef HEADLESS_UTIL_H
#define HEADLESS_UTIL_H

#include "nexis-core_global.h"

// SSO-3368 / audit H6: helpers for deciding whether a Nexis invocation is a
// headless scheduled run (cron, systemd user timer) so that main() can steer
// QApplication onto the offscreen QPA platform before construction. Without
// this, cron/boot-catch-up `--clean` runs abort during QApplication startup
// when no DISPLAY/WAYLAND_DISPLAY is available, silently skipping the clean.

class NEXISCORESHARED_EXPORT HeadlessUtil
{
public:
    // True if argv contains a CLI flag that triggers a non-interactive run
    // (`--clean` or `--check-threshold`). Pure scan of argv; performs no
    // env access or Qt calls so it is safe to call before QApplication.
    static bool isHeadlessArgv(int argc, char *const argv[]);

    // True if main() should `qputenv("QT_QPA_PLATFORM", "offscreen")` before
    // constructing QApplication. We only override when the user has not
    // pinned a platform themselves — if QT_QPA_PLATFORM is already set we
    // respect it (e.g. tests setting `minimal`, users debugging with `xcb`).
    // We deliberately do NOT consult DISPLAY/WAYLAND_DISPLAY: cron unsets
    // them, but xvfb/nested sessions may set them to a value that still
    // fails to load, so headless argv is the more reliable signal.
    static bool shouldForceOffscreen(bool isHeadless, bool qpaPlatformAlreadySet);
};

#endif // HEADLESS_UTIL_H
