Processes: %CPU is now the process's share of the whole machine (0–100%)
on both Linux and macOS, instead of a per-core value that could reach
several hundred percent on multi-core systems. %CPU and %Memory now show
one decimal place. Existing per-process CPU alert thresholds are converted
to the new scale automatically.
