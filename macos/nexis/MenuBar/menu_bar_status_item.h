#ifndef MENU_BAR_STATUS_ITEM_H
#define MENU_BAR_STATUS_ITEM_H

#ifdef __cplusplus
extern "C" {
#endif

// FW-20: thin C bridge so Qt/C++ code can drive an NSStatusItem without
// pulling AppKit headers into Qt translation units — same pattern as
// macos_dock_helper.h.
typedef void (*NexisMenuBarClickCallback)(void);

// SSO-23853: the status item now carries a dropdown NSMenu ("Open Nexis",
// "Clean Now") instead of reacting to a bare click — onActivate fires for
// "Open Nexis", onClean fires for "Clean Now".
void nexis_menubar_create(NexisMenuBarClickCallback onActivate, NexisMenuBarClickCallback onClean);
void nexis_menubar_destroy(void);
void nexis_menubar_set_title(const char *title);

// Updates the "Clean Now" menu item's label/enabled state (e.g. to
// "Cleaning…" / disabled while a clean is running). No-op if the status
// item hasn't been created yet.
void nexis_menubar_set_clean_item_state(const char *title, int enabled);

#ifdef __cplusplus
}
#endif

#endif // MENU_BAR_STATUS_ITEM_H
