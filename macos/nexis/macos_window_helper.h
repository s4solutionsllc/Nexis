#ifndef MACOS_WINDOW_HELPER_H
#define MACOS_WINDOW_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Sets the native window chrome (title bar, traffic-light area) of the window
// hosting `nsView` to the dark or light system appearance, so it matches the
// app theme instead of following the system setting. `nsView` is a QWidget's
// winId() on macOS.
void nexis_macos_set_window_dark(void *nsView, int dark);

// Returns non-zero if the user has enabled Reduce Motion in
// System Settings > Accessibility > Display.
int nexis_macos_prefers_reduced_motion(void);

#ifdef __cplusplus
}
#endif

#endif // MACOS_WINDOW_HELPER_H
