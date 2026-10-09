macOS Services (SSO-25782): the Services page now shows the real state of
launchd jobs. Stopped and disabled services are listed (previously only loaded
ones were), the Startup switch reflects whether a service is actually enabled
instead of always showing on, system daemons appear alongside agents, and each
row shows the program it runs. Changing one of your own agents no longer asks
for an admin password; system daemons still do. Running apps no longer show up
in the list as "application.…" entries. On both macOS and Linux the window no
longer freezes while a password prompt is open.
