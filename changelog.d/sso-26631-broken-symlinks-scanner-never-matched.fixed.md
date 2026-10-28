System Cleaner (SSO-26631, GH#532): the "Broken Symlinks" category always
reported 0 items, even with dangling symlinks present in the scanned
directories — Qt only enumerates broken symlinks when `QDir::System` is set,
and the Linux and macOS scanners both omitted it. Fixed on both platforms,
paired with an exclusion list (Flatpak internals, icon caches, Trash, and
known intentionally-dangling locks like browser `SingletonLock`/`lock`
files) so the now-working scan doesn't surface symlinks that are unsafe to
delete. Also corrected the card subtitle, which claimed a `~/` recursive
scan when the scanner only covers `~/.local`, `~/bin`, and `/usr/local/bin`
(Homebrew prefix on macOS).
