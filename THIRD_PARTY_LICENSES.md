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
