Resources page (SSO-26717): the Disk Read/Write and Network charts no
longer show one huge, meaningless spike right after the Resources page is
reopened or the app is restored from being minimized. The disk and network
counters are re-baselined on reactivation instead of plotting a delta that
spans however long sampling was actually paused.
