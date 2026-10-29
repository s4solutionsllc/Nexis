Resources page (SSO-26721): the Memory card now plots Used, Swap and one
platform series — Compressed on macOS, Cached on Linux — with the dropped
Wired/Active values shown in a header tooltip instead; Linux's Available
line is removed (it tracked close to the inverse of Used). The Load &
Pressure card's CPU Pressure Stall status line is replaced with charted
`some avg10` lines for CPU, memory and io pressure on a secondary axis
(Linux only, each line hides independently if its `/proc/pressure/*` file is
missing), and a dashed reference line marks the host's core count so load
average reads the same way `uptime`/`top` do regardless of core count.
