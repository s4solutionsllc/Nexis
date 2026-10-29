Resources page (SSO-26719): the 13 full-width, single-column cards are now
six subsystem cards — CPU, Load & Pressure, Memory, GPU, Disk I/O, Network —
in a responsive 2-column grid, each leading with a current-value header
instead of a verbose legend. Per-Core Detail is now a "Per-core" toggle on
the CPU card; Package Power (RAPL) and Disk Temperature show as values in
the CPU and Disk I/O headers; CPU Pressure Stall (PSI) and OOM kills show as
compact status lines on the Load & Pressure and Memory cards. A Live/History
switch replaces the always-visible persisted-history panel. The Disk Usage
Launcher is unchanged, below the grid.
