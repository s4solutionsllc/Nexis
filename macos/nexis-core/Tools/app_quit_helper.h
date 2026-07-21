#ifndef APP_QUIT_HELPER_H
#define APP_QUIT_HELPER_H

// SSO-15566 / SSO-15373 CISO §4: requests a graceful quit — equivalent to
// sending the app the AppleEvent `quit`, via -[NSRunningApplication
// terminate] — for every running process whose bundle lives at bundlePath.
// Never force-terminates (-forceTerminate / SIGKILL). Returns true iff at
// least one matching running instance was asked to quit.
//
// Plain C++ declaration so callers outside Objective-C++ translation units
// (package_tool_macos.cpp) can link against it; the implementation lives in
// app_quit_helper.mm, mirroring the macos_dock_helper.mm bridging pattern.
bool nexis_macos_quit_app_at_path(const char *bundlePath);

#endif // APP_QUIT_HELPER_H
