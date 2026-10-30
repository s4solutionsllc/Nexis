macOS: clicking the tray icon could open the main window alongside its menu,
or crash the app, when minimized/hidden to tray (SSO-26761, SSO-26762). The
tray icon now shows only its menu on click; "Open Nexis" is the only action
that raises the window, and it — along with every other tray action that can
raise the window on macOS — now does so after the menu has closed instead of
mid-click. "Clean Now" completion no longer shows a parentless dialog while
the window is hidden; it reports through the tray notification instead.
