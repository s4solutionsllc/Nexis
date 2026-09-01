Feedback dialog (GH#424): on Linux, "Report a Bug" / "Request a Feature" /
"General Feedback" could leave the dialog unresponsive if the system's URL
handler (e.g. an xdg-desktop-portal backend) blocked instead of returning.
The dialog now closes immediately and the browser launch runs off the UI
thread, so a stuck handler no longer freezes the app. The AUR package also
now depends on `xdg-utils` so the `xdg-open` fallback is always present.
