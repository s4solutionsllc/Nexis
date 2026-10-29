Resources page (SSO-26717): the Network history chart no longer latches at
an absurd ~16 EiB/s scale for the rest of the session when the RX/TX byte
counters step backwards (e.g. an interface re-enumerating). The rate is now
clamped to 0 for that sample instead of wrapping, using the same guard
already applied to the Disk Read/Write chart.
