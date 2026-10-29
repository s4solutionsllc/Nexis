Resources page (SSO-26717): the OOM Kills card's `systemctl`/`journalctl`
sampling no longer blocks the UI thread for up to 5 seconds every 5 seconds
on Linux. The query now runs on a background worker, is skipped while a
previous one is still in flight, and polls at most once every 30 seconds.
