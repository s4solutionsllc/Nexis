Resources page history charts (SSO-26718): the Disk I/O, Network, CPU Load
Average and CPU Pressure Stall charts' Y axes used to only ever grow — once a
usage spike happened, the axis stayed scaled to it for the rest of the
session even after activity dropped back down. The axis now shrinks back down
once the spike's data point ages out of the visible window.
