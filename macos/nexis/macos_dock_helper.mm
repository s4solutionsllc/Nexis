#import <AppKit/AppKit.h>
#include "macos_dock_helper.h"

void nexis_macos_hide_dock_icon(void)
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

void nexis_macos_show_dock_icon(void)
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}
