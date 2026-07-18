#ifndef MENU_BAR_STATUS_ITEM_H
#define MENU_BAR_STATUS_ITEM_H

#ifdef __cplusplus
extern "C" {
#endif

// FW-20: thin C bridge so Qt/C++ code can drive an NSStatusItem without
// pulling AppKit headers into Qt translation units — same pattern as
// macos_dock_helper.h.
typedef void (*NexisMenuBarClickCallback)(void);

void nexis_menubar_create(NexisMenuBarClickCallback onClick);
void nexis_menubar_destroy(void);
void nexis_menubar_set_title(const char *title);

#ifdef __cplusplus
}
#endif

#endif // MENU_BAR_STATUS_ITEM_H
