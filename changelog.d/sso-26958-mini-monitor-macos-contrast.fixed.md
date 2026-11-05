Mini Monitor (SSO-26958): the CPU/MEM/DSK values were unreadable on macOS
because the window never forced the themed background to paint and the
value labels relied on the inherited system text color instead of the
theme token. Both now render explicitly from the active theme, matching
the existing Linux behavior.
