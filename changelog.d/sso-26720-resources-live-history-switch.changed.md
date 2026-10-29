Resources page (SSO-26720): a Live / History segmented control now separates
the 60-second live subsystem grid from the persisted Resource History panel,
which previously sat in the middle of the same scroll with no signal that it
showed recorded, not live, data. History shows an explanatory empty state
with a link to Settings when recording is off (the default) instead of an
empty chart, and offers only the history ranges the configured retention can
actually fill — e.g. "30 d" is hidden once retention is 7 d. The last
selected mode is remembered across sessions, and switching modes never
resets the live buffers.
