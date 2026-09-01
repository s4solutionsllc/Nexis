Uninstaller Packages tab (GH#426): the pacman package list was empty on
systems running a non-English locale, because `pacman -Qi`'s field labels
(`Name`, `Description`, `Groups`) are translated and no longer matched the
parser. `pacman -Qi` is now invoked with `LANG=C` to force English output,
matching every other locale-sensitive command in the Linux package tool.
