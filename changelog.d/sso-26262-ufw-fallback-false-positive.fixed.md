Firewall widget (SSO-26262, GH#513): the unprivileged UFW status fallback no
longer reports the firewall as enabled just because the `ufw.service`
systemd unit is active. The unit only reapplies saved rules at boot and stays
active after `sudo ufw disable`, so `/etc/ufw/ufw.conf`'s `ENABLED` setting is
now the authoritative signal whenever it is present, with the systemd unit's
state used only when `ufw.conf` can't be read.
