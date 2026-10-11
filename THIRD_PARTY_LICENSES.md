# Third-Party Licenses

Nexis bundles the following third-party assets. Their license texts are included alongside the relevant files and are installed with the package.

---

## Inter (Regular, Bold, SemiBold)

- **Source:** https://github.com/rsms/inter
- **Copyright:** 2020 The Inter Project Authors
- **License:** SIL Open Font License 1.1
- **License text:** `shared/nexis/static/font/LICENSE-OFL.txt`
- **Installed as:** `/usr/share/doc/nexis/LICENSE-OFL.txt`

---

## JetBrains Mono (Regular)

- **Source:** https://github.com/JetBrains/JetBrainsMono
- **Copyright:** 2020 The JetBrains Mono Project Authors
- **License:** SIL Open Font License 1.1
- **License text:** `shared/nexis/static/font/LICENSE-OFL.txt`
- **Installed as:** `/usr/share/doc/nexis/LICENSE-OFL.txt`

---

## Ubuntu-R (Regular)

- **Source:** https://design.ubuntu.com/font/
- **Copyright:** 2010, 2011 Canonical Ltd.
- **License:** Ubuntu Font Licence 1.0
- **License text:** `shared/nexis/static/font/LICENSE-UFL.txt`
- **Installed as:** `/usr/share/doc/nexis/LICENSE-UFL.txt`

---

## orlp/ed25519 (verify-only subset)

- **Source:** https://github.com/orlp/ed25519
- **Copyright:** 2015 Orson Peters
- **License:** zlib
- **License text:** `macos/nexis-core/Info/vendor/ed25519/LICENSE.txt`
- **Pinned commit / manifest:** `macos/nexis-core/Info/vendor/ed25519/UPSTREAM.md` (SSO-15431)
- **Used by:** `SparkleSignatureVerifier` (macOS) to verify Ed25519 (RFC 8032) Sparkle appcast signatures — replaces a nonexistent macOS `SecKey` EdDSA path.

---

## BleachBit CleanerML cleaner definitions

- **What it is:** 104 unmodified CleanerML XML cleaner definitions, vendored from the BleachBit project. They are Nexis's shipped default cleaner set (compiled into the app via `shared/nexis/cleanerml.qrc`, so they ship in every artifact: deb, AppImage, dmg) and also double as parser test fixtures.
- **Path:** `shared/nexis/cleaners.d/` (shipped). A separate, smaller 10-file subset plus a Nexis-authored `malformed.xml` lives at `tests/fixtures/cleanerml/` for parser unit tests only — it ships in no artifact.
- **Source:** https://github.com/bleachbit/bleachbit
- **Upstream tag / commit:** `v6.0.3` / `9e1654fb32460f034cb384e31a2f447c886fedd8`
- **Copyright:** 2008-2026 Andrew Ziem (per-file headers vary between 2008-2021 and 2008-2026; this range covers every file's span)
- **License:** GPL-3.0-or-later
- **License text:** `shared/nexis/cleaners.d/COPYING.bleachbit` (also compiled into the Qt resource, readable at runtime as `:/cleaners.d/COPYING.bleachbit`)
- **Note:** GPL-3.0-or-later data files bundled unmodified inside a GPL-3.0-only work; see `shared/nexis/cleaners.d/README.md` for the compatibility rationale and refresh procedure.
