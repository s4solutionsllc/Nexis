#import <AppKit/AppKit.h>
#include "menu_bar_status_item.h"

namespace {
NSStatusItem *gStatusItem = nil;
NexisMenuBarClickCallback gClickCallback = nullptr;
}

@interface NexisMenuBarClickTarget : NSObject
- (void)handleClick;
@end

@implementation NexisMenuBarClickTarget
- (void)handleClick
{
    if (gClickCallback)
        gClickCallback();
}
@end

static NexisMenuBarClickTarget *gClickTarget = nil;

void nexis_menubar_create(NexisMenuBarClickCallback onClick)
{
    if (gStatusItem)
        return;

    gClickCallback = onClick;
    gClickTarget = [[NexisMenuBarClickTarget alloc] init];

    // statusItemWithLength: returns an autoreleased item — retain it since we
    // hold onto it for the monitor's lifetime, which outlives this call.
    gStatusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength] retain];
    gStatusItem.button.font = [NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightRegular];
    gStatusItem.button.target = gClickTarget;
    gStatusItem.button.action = @selector(handleClick);
}

void nexis_menubar_destroy(void)
{
    if (!gStatusItem)
        return;

    [[NSStatusBar systemStatusBar] removeStatusItem:gStatusItem];
    [gStatusItem release];
    gStatusItem = nil;

    [gClickTarget release];
    gClickTarget = nil;
    gClickCallback = nullptr;
}

void nexis_menubar_set_title(const char *title)
{
    if (!gStatusItem)
        return;
    gStatusItem.button.title = title ? [NSString stringWithUTF8String:title] : @"";
}
