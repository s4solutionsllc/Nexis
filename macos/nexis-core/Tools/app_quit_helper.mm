#import <AppKit/AppKit.h>
#include "app_quit_helper.h"

bool nexis_macos_quit_app_at_path(const char *bundlePath)
{
    if (!bundlePath || !bundlePath[0])
        return false;

    NSString *targetPath = [NSString stringWithUTF8String:bundlePath];
    NSString *targetPrefix = [targetPath stringByAppendingString:@"/"];
    bool requested = false;

    for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications]) {
        NSString *appPath = app.bundleURL.path;
        if (!appPath)
            continue;
        if (![appPath isEqualToString:targetPath] && ![appPath hasPrefix:targetPrefix])
            continue;

        // -terminate: is the graceful request path — the app gets a normal
        // quit event and may decline (e.g. unsaved-changes prompt). We never
        // call -forceTerminate (SIGKILL-equivalent) here per CISO §4.
        [app terminate];
        requested = true;
    }

    return requested;
}
