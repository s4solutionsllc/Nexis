APT Repository Manager (SSO-26503, GH#524): phased Ubuntu updates are now
detected reliably via `apt-cache policy`'s Phased-Update-Percentage field
instead of string-matching `apt list --upgradable` output, which missed many
packages apt was actually holding back. A phased package now shows as
"Phased update (N%)" in Available Updates instead of looking like a normal,
installable update, and reverts to a normal entry once its rollout reaches
100%.
