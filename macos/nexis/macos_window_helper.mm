#import <AppKit/AppKit.h>
#include "macos_window_helper.h"

void nexis_macos_set_window_dark(void *nsView, int dark)
{
    if (!nsView)
        return;
    NSView *view = (__bridge NSView *)nsView;
    NSWindow *window = [view window];
    if (!window)
        return;
    [window setAppearance:[NSAppearance appearanceNamed:
        dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua]];
}
