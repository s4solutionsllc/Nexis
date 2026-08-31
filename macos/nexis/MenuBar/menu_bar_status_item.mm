#import <AppKit/AppKit.h>
#include "menu_bar_status_item.h"

namespace {
NSStatusItem *gStatusItem = nil;
NexisMenuBarClickCallback gActivateCallback = nullptr;
NexisMenuBarClickCallback gCleanCallback = nullptr;
}

@interface NexisMenuBarClickTarget : NSObject
- (void)handleActivate;
- (void)handleClean;
@end

@implementation NexisMenuBarClickTarget
- (void)handleActivate
{
    if (gActivateCallback)
        gActivateCallback();
}
- (void)handleClean
{
    if (gCleanCallback)
        gCleanCallback();
}
@end

static NexisMenuBarClickTarget *gClickTarget = nil;
static NSMenuItem *gCleanMenuItem = nil;

void nexis_menubar_create(NexisMenuBarClickCallback onActivate, NexisMenuBarClickCallback onClean)
{
    if (gStatusItem)
        return;

    gActivateCallback = onActivate;
    gCleanCallback = onClean;
    gClickTarget = [[NexisMenuBarClickTarget alloc] init];

    // statusItemWithLength: returns an autoreleased item — retain it since we
    // hold onto it for the monitor's lifetime, which outlives this call.
    gStatusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength] retain];
    gStatusItem.button.font = [NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightRegular];

    // SSO-23853: a dropdown menu replaces the old bare left-click handler —
    // assigning .menu makes AppKit show it on click (any mouse button)
    // instead of invoking button.target/action, so "Open Nexis" is now an
    // explicit menu item rather than the click itself.
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    NSMenuItem *activateItem = [[NSMenuItem alloc] initWithTitle:@"Open Nexis"
                                                            action:@selector(handleActivate)
                                                     keyEquivalent:@""];
    activateItem.target = gClickTarget;
    [menu addItem:activateItem];
    [activateItem release];

    [menu addItem:[NSMenuItem separatorItem]];

    gCleanMenuItem = [[NSMenuItem alloc] initWithTitle:@"Clean Now"
                                                 action:@selector(handleClean)
                                          keyEquivalent:@""];
    gCleanMenuItem.target = gClickTarget;
    [menu addItem:gCleanMenuItem];
    // menu now holds the strong reference (kept alive until nexis_menubar_destroy
    // releases gStatusItem, which releases .menu, which releases its items) — drop
    // our own alloc ownership, same as activateItem above. gCleanMenuItem stays a
    // valid unretained pointer for nexis_menubar_set_clean_item_state() until then.
    [gCleanMenuItem release];

    gStatusItem.menu = menu;
    [menu release];
}

void nexis_menubar_destroy(void)
{
    if (!gStatusItem)
        return;

    [[NSStatusBar systemStatusBar] removeStatusItem:gStatusItem];
    [gStatusItem release];
    gStatusItem = nil;

    gCleanMenuItem = nil;

    [gClickTarget release];
    gClickTarget = nil;
    gActivateCallback = nullptr;
    gCleanCallback = nullptr;
}

void nexis_menubar_set_title(const char *title)
{
    if (!gStatusItem)
        return;
    gStatusItem.button.title = title ? [NSString stringWithUTF8String:title] : @"";
}

void nexis_menubar_set_clean_item_state(const char *title, int enabled)
{
    if (!gCleanMenuItem)
        return;
    if (title)
        gCleanMenuItem.title = [NSString stringWithUTF8String:title];
    gCleanMenuItem.enabled = enabled ? YES : NO;
}
