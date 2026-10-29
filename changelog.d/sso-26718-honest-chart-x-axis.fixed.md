Resources page history charts (SSO-26718): the X axis is now labeled
"Seconds" and plots real elapsed time. Previously it counted samples with no
label, which silently stopped matching the "last 60 seconds" it visually
implied whenever the sampling rate slowed down (on battery or while the
window was unfocused).
