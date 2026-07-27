# Sparkle Update Download + Verify + Execute — Security Design Decision

**Status:** GO-WITH-CONDITIONS. Gate for [SSO-15508](/SSO/issues/SSO-15508) —
implementation must satisfy every "Hard gate" item below before merge; the CTO
sign-off in the issue thread on [SSO-17775](/SSO/issues/SSO-17775) is the
record of co-approval.

**Owner:** CISO. **Co-signer:** CTO. **Audience:** whoever implements
SSO-15508 (currently unassigned — likely GeneralCoder/NexisMaintainer). This
document is the thing to build against; it is not a retrospective.

**Scope:** the macOS Sparkle-appcast update path only (`macos/nexis-core/Info/
sparkle_*`, `macos/nexis/Pages/Homebrew/homebrew_page.cpp`). Homebrew cask
updates (`brew upgrade --cask`) are out of scope — they already run through
Homebrew's own signature/checksum handling and `HomebrewPage::onUpdateSelectedClicked()`.

## 0. Current state (verified against the tree at time of review)

- `HomebrewPage::onSparkleUpdateItemChanged` / `onSparkleUpdateSelectedClicked`
  (`macos/nexis/Pages/Homebrew/homebrew_page.cpp:693`) only calls
  `QDesktopServices::openUrl(entry.enclosureUrl)` today. Nexis does not
  download or execute anything yet.
- `SparkleSignatureVerifier::verify()` (`macos/nexis-core/Info/
  sparkle_signature_verifier.{h,cpp}`) is fully implemented, RFC 8032 KAT
  tested, and fail-closed by construction (`Result::Valid` is the only
  success value; every other `Result` — including `InternalError` — must be
  treated as failure). It has **no call site**.
- The trust anchor is already correctly wired at the *scanning* layer:
  `SparkleUpdateScanner::scan()` (`sparkle_update_scanner.cpp:153`) reads
  `SUPublicEDKey` from the target app's own local `Info.plist`
  (`PlistUtil::readAppBundleInfo`) — never from the appcast XML. The appcast
  parser (`SparkleAppcastParser::EnclosureInfo`) has no key field at all. This
  is the correct design; §2 below makes it a binding rule so it isn't
  regressed when the download/execute path is added.
- No installer-execution code (dmg mount, pkg install, zip extraction) exists
  anywhere in the codebase today. This is genuinely greenfield — there is no
  prior art in-repo to diverge from, only the `CommandUtil::sudoExecWithStatus`
  / `osascript ... with administrator privileges` pattern used elsewhere for
  *unrelated* elevated operations (SMART reads, `rm -rf`), which §6 explicitly
  says not to reuse here for v1.

## 1. Threat model

| # | Threat actor / vector | Attack | Primary control |
|---|---|---|---|
| T1 | Hostile appcast (compromised publisher server, or feed served over plain HTTP and MITM'd) | Inject a fake `<enclosure>` pointing at attacker infrastructure, with an attacker-chosen `edSignature`/URL | Signature is checked against the **locally-read** app-bundle key, never a key from the feed (§2). An attacker without the publisher's private key cannot produce a `Result::Valid` signature no matter what they inject into the feed. Worst case without the private key: `Invalid`/`MissingSignature` → fail-closed (§5) → falls back to today's browser-open. |
| T2 | Hostile/MITM enclosure host (download-time tampering, on-path or off-path) | Serve modified installer bytes | Same as T1 — verification is over the full downloaded byte stream, independent of transport. TLS (§3) is defense-in-depth, not the control. |
| T3 | Downgrade/rollback via a legitimately-signed old installer | Serve an old (but validly-signed) installer with the appcast's *unsigned* `sparkle:version` field lied about to look newer than the floor | Two-part control, §4: (a) persisted per-app **floor version**, ratcheted only after a real verified+installed update, never from unverified appcast metadata; (b) post-verification **embedded-version cross-check** against the appcast's claimed version, read from the verified bytes themselves, not trusted from XML. Both are required — (a) alone is defeated by exactly this attack, since the appcast version string is unsigned. |
| T4 | Replay of an old signed installer (same bytes, verbatim) | Re-serve a byte-identical old installer/appcast entry captured from a prior legitimate response | Subsumed by T3's floor-version check once the embedded real version is extracted and compared — a replayed old artifact's *real* version will be at or below the floor and gets rejected regardless of what the appcast claims. |
| T5 | Local attacker (or buggy co-resident process) with write access to the download path | Swap bytes between verify and exec (TOCTOU) | §4 — download to a private, unpredictable, non-shared path (or verify fully in memory) and execute the exact verified bytes with no re-open-by-path gap. |
| T6 (not in the AC list, called out because it's adjacent) | Malicious appcast supplying a crafted `SUPublicEDKey`-shaped value in the feed itself | Attempt to get Nexis to trust a feed-supplied key instead of the bundle's own | Structurally impossible today — `EnclosureInfo` carries no key field — and §2 makes it a standing rule that this must never be added. |

DREAD-style prioritization: T1–T4 are the RCE-adjacent items (Damage: high —
arbitrary code execution at user privilege; Reproducibility: high once a
MITM/compromised-host position exists) and are treated as hard gates, not
recommendations. T5 is high-damage but requires local code-exec-or-write
access the attacker likely already has at that point (lower marginal
Discoverability/Exploitability than T1–T4) — still a hard gate because it's
cheap to close correctly and it's exactly the concern the CTO flagged in
SSO-15419.

## 2. Trust-anchor decision

**Public key source:** the target application's own `SUPublicEDKey`, read
locally from that app's installed `Info.plist` at scan time
(`sparkle_update_scanner.cpp:153`, unchanged). This is a local, filesystem
read of a bundle already present on disk before any network activity for
that app occurs — the attacker's feed/host cannot influence which key is
used.

**Appcast-supplied keys: never accepted, recommendation confirmed.** TOFU or
feed-supplied keys give zero protection against the exact hostile-appcast /
MITM-host threats this feature exists to defend against (T1/T2) — the same
attacker who can forge the enclosure can forge an accompanying "trust me"
key. Binding rule for the implementer: `SparkleSignatureVerifier::verify()`'s
`publicKeyB64` parameter must only ever be populated from
`PlistUtil::AppBundleInfo::suPublicEDKey` (a local bundle read). Do not add a
key field to `SparkleAppcastParser::EnclosureInfo`, and do not accept a key
argument anywhere in the new download/execute call path.

**Rotation:** publisher-controlled, out of Nexis's hands, and requires no
Nexis-side mechanism. Because the key is re-read from the local bundle on
every scan (never cached/pinned in a Nexis-owned store across sessions), a
publisher rotating their key simply ships the new key in their next release;
the *next* Nexis scan picks it up automatically. Do not add any
persistent key cache — always re-read fresh from the bundle on disk.

**No Nexis-side master key for third parties.** Each Sparkle-feed app is
responsible for its own key; there is no N:1 relationship for Nexis to
manage. (Out of scope: a hypothetical future Nexis self-update signing key —
different problem, not addressed here.)

## 3. Transport requirements

- **HTTPS-only, both fetches.** The appcast fetch (`SparkleUpdateScanner::
  fetchFeed`) does not currently reject non-HTTPS URLs — this must be added.
  The new enclosure/installer download must reject any `enclosureUrl` whose
  `QUrl::scheme()` is not exactly `"https"` *before* issuing any network
  request (no fallback, no silent downgrade, no `file://`/`data:`/other
  scheme accepted from feed-supplied data).
- **Redirect policy:** reuse `QNetworkRequest::NoLessSafeRedirectPolicy`
  (already used in `fetchFeed`) for the enclosure download. An https→http
  redirect must abort the download, not follow it.
- **Max size:** cap the enclosure download, same pattern as the existing 2
  MiB appcast guard (`SparkleAppcastParser::kMaxFeedBytes`) but sized for
  installer artifacts. Recommend **500 MiB**, abort-mid-stream once exceeded
  (defends against a malicious/compromised host streaming unbounded bytes as
  a resource-exhaustion vector, and bounds the in-memory verify design in
  §4).
- **Timeout:** the existing 10 s appcast timeout is too short for installer
  downloads. Recommend a total ceiling (e.g. 10 minutes) plus a stall
  detector (abort if zero bytes arrive for 30 s) so a hung connection can't
  block the worker thread indefinitely. Must run off the GUI thread — same
  `QEventLoop`-driven pattern `fetchFeed` already uses.
- **No ambient credentials.** Enclosure hosts are untrusted, feed-supplied
  data — send only the existing `User-Agent` header, nothing that could leak
  session/auth material to a publisher-controlled (and, under T1/T2, possibly
  attacker-controlled) host.

## 4. TOCTOU-safe design (per CTO note, SSO-15419)

**Invariant: the exact bytes verified are the exact bytes executed.**

Recommended implementation:

1. Download the full enclosure into memory (`QByteArray`), bounded by the
   §3 size cap. `SparkleSignatureVerifier::verify()` already takes
   `const QByteArray &fileData` — no interface change needed.
2. Call `verify()` on those in-memory bytes. Do not write anything to disk
   before this returns.
3. Only on `Result::Valid` (§5), write that *same* `QByteArray` to a freshly
   created `QTemporaryFile` under `QStandardPaths::TempLocation` (on macOS
   this resolves to the per-user `$TMPDIR`, mode 0700 — not the shared
   `/tmp`). `QTemporaryFile` gives an unpredictable filename and creates the
   file with restrictive permissions atomically (`O_EXCL`-equivalent) — do
   not use a fixed or guessable filename.
4. Hand the OS opener (§6) *that exact path*, held open by our process for
   the duration, with no intervening re-open-by-name. Nothing between "write
   the verified bytes" and "hand the path to the opener" re-reads the file
   from an untrusted source, and the private per-user temp directory means a
   different-user local attacker cannot pre-stage or race the path (a
   same-user local attacker is the accepted T5 residual — see §7).
5. Never mark the temp file executable/openable before step 3 completes.

   **Cleanup — corrected at CTO co-sign (§10.3).** The handoff in §6 is
   `QDesktopServices::openUrl`, which delegates to LaunchServices and returns
   immediately with **no child-process handle to wait on**. There is no
   "installer process exited" event available, so cleanup must not be
   specified as waiting for one. Required behavior instead:
   - keep the `QTemporaryFile` object alive with auto-remove ON for the life
     of the Nexis process, so a normal quit cleans it up; and
   - sweep stale Nexis-owned update artifacts from the private temp dir on
     startup, covering the crash / force-quit case.

   Do not use `setAutoRemove(false)` plus an unbacked promise to delete
   later. Do not delete immediately after `openUrl` returns either — the
   opener may not have read the file yet.

If a future enclosure legitimately exceeds a size where full in-memory
buffering is practical, the fallback is: write to the same kind of private
`QTemporaryFile` first, `verify()` against bytes re-read from the *same open
fd* (not a re-open by path), and only then proceed — never verify one handle
and execute a different one.

## 5. Fail-closed behavior (confirmed, extended to the call site)

`SparkleSignatureVerifier`'s contract already states `Result::Valid` is the
only safe-to-install outcome. This review makes it a binding requirement on
the **caller**, not just the verifier:

- The only branch that proceeds to §4 step 3 / §6 execution is
  `result == Result::Valid`.
- Every other `Result` — `Invalid`, `MissingKey`, `MissingSignature`,
  `DecodingError`, `InternalError`, `UnsupportedLegacyDsa` — must: (a) not
  write or execute anything, (b) discard the downloaded bytes, (c) show a
  plain, non-alarming message ("this update couldn't be verified — opening
  the publisher's page instead"), and (d) fall back to exactly today's
  behavior: `QDesktopServices::openUrl(entry.enclosureUrl)`.
- `UnsupportedLegacyDsa` in particular is *not* a special case that's
  "probably fine" — a DSA-only appcast gets the identical fail-closed
  treatment as `Invalid`. (Note: `entry.signatureMetadataPresent` today is
  `true` for a DSA-only entry that also has a public key present — see
  `sparkle_update_scanner.cpp:157` — so a DSA-only entry will legitimately
  enter the new download+verify path and only get blocked at the `verify()`
  call. That's correct and intentional: `signatureMetadataPresent` is a UX
  signal for "worth attempting," not the security boundary. The security
  boundary is the `Result` returned by `verify()`. No parser/scanner change
  required here.)
- **Hard gate — new test required.** `tests/core/test_sparkle_signature_verifier.cpp`
  pins the verifier's own contract but there is no caller yet to test.
  Implementation must add a call-site-level test (e.g. inject a spy/mock
  "installer launcher" seam) asserting that for every non-`Valid` fixture
  Result, the launcher is invoked **zero** times, and that the
  browser-open fallback fires exactly once. This is the regression test that
  encodes T1/T2's fail-closed guarantee at the layer that actually matters.

## 6. Privilege / execution decision

- **Unprivileged only for v1.** Do not wire `CommandUtil::sudoExecWithStatus`
  (the `osascript ... with administrator privileges` pattern used elsewhere
  in this codebase for SMART reads and elevated `rm -rf`) into this feature.
  Elevation is explicitly **out of scope**. Rationale: any bug in the ed25519
  verification, the TOCTOU handling, or the version cross-check in §7
  currently caps the blast radius at "code runs as the logged-in user." Wiring
  in silent/prompted admin elevation for a freshly-downloaded, third-party
  (not Nexis-signed) binary turns the same class of bug into root-level RCE.
  Keep these two concerns decoupled — ship the unprivileged, verified path
  first; elevation is a separate, separately-reviewed follow-up if ever
  pursued, not a v1 default.
- **What "execute" means:** hand the verified, on-disk temp file to the OS's
  normal unprivileged open/launch mechanism (`QDesktopServices::
  openUrl(QUrl::fromLocalFile(path))` or equivalent), rather than Nexis
  itself parsing/mounting/copying installer internals. This keeps Nexis's own
  code surface small (no dmg-mounting, no pkg-payload execution logic to get
  wrong). The user still sees a
  native installer/mount dialog — acceptable UX cost for v1 given the RCE
  stakes; that is strictly better than fully-silent unattended execution and
  is a deliberate choice, not an oversight.
- **Quarantine must be set explicitly — corrected at CTO co-sign (§10.1).**
  A file Nexis writes itself does **not** automatically carry
  `com.apple.quarantine`; that xattr is applied by the *downloading* agent.
  Bytes we fetch with `QNetworkAccessManager` and write via `QTemporaryFile`
  therefore arrive **unquarantined**, and Gatekeeper's
  first-launch/translocation check will not fire on them. Implementation
  **must** explicitly set `com.apple.quarantine` on the verified artifact
  (`setxattr`, before handing the path to the opener) if the Gatekeeper layer
  is to exist at all. Note the codebase already has
  `MacOsXattrUtil::stripQuarantine()` whose job is to *strip* quarantine
  (`shared/nexis-core/Utils/macos_xattr_util.h`, used for launchd plists at
  `startup_app_edit.cpp:185` and `schedule_manager.cpp:436`) —
  it is the wrong helper here and must not be reached for. Getting this
  backwards silently deletes a control rather than adding one.
- **Elevation, precisely stated.** "Unprivileged only" binds *Nexis's own
  process*: Nexis must never invoke `sudoExecWithStatus`/`osascript ... with
  administrator privileges`. It does **not** mean no admin prompt ever
  appears — a flat `.pkg` opened in Installer.app will request admin rights
  itself when its payload targets system locations. That is acceptable and
  in scope: the prompt is OS-mediated, user-consented, and attributed to
  Apple's installer rather than silently brokered by Nexis. Gate 6 in §8 is
  about *who asks*, and it is not violated by Installer.app prompting.
- **Enclosure format scope, v1:** support only formats that admit a cheap,
  *non-executing* way to introspect the real embedded version (needed for
  §7's cross-check) — `.zip` containing a `.app` (read `Info.plist` directly
  from the archive's central directory, no extraction needed) and flat
  `.pkg` (`pkgutil --expand` / read `Distribution` or `PackageInfo` XML,
  which does not run any installer scripts). **`.dmg` is out of scope for
  v1** — mounting requires `hdiutil attach`, which is its own nontrivial
  attack surface (disk-image-format parsing is a historically bug-prone area
  in the OS) and mounting an untrusted, if-signature-checked, disk image adds
  complexity this review isn't in a position to sign off on yet. Any
  enclosure whose format isn't in the supported set falls back to today's
  browser-open behavior, same as a failed verification.
- **`.zip` does not self-install — corrected at CTO co-sign (§10.2).**
  Handing a `.zip` to `QDesktopServices::openUrl` invokes Archive Utility,
  which expands a `.app` into the *private temp directory* and leaves the
  installed copy in `/Applications` untouched. The user gets a stray app
  bundle in a hidden folder and no update. `.pkg` genuinely installs;
  `.zip` does not. The verify-then-execute chain is sound for both — the
  gap is that "execute" delivers no user value in the zip case. Binding
  resolution for v1, pick one and state it in the PR:
  1. **Preferred — scope v1 to `.pkg` only.** `.zip` keeps today's
     browser-open fallback. Smallest correct thing that ships.
  2. **Acceptable — `.zip` as verified-reveal.** Download, verify, write,
     then reveal the expanded bundle in Finder with copy-to-Applications
     left to the user. Must not be described in the UI as an applied update.

  What is **not** in scope for v1: Nexis performing the in-place bundle swap
  itself (quit target app, replace `/Applications/Foo.app`, relaunch). That
  is the operation Sparkle's own updater exists to do carefully, it can
  destroy a working install on partial failure, and it needs its own design
  review — same treatment as `.dmg`. Do not let it slip in as "just a file
  copy."

## 7. Downgrade/rollback and replay mitigation (binding on implementation)

This is the concrete mechanism behind T3/T4 in §1 — restated here as an
explicit hard gate because it's easy to under-scope:

1. **Persisted per-app floor version**, keyed by the app's `CFBundleIdentifier`
   (not display name — a name is spoofable/collidable across apps). This is
   already available as `PlistUtil::AppBundleInfo::bundleId`
   (`macos/nexis-core/Utils/plist_util.h:14`, populated from
   `CFBundleIdentifier` at `plist_util.cpp:32`) — no new plumbing needed.
   Store the floor in Nexis's own app-support location — not anywhere the
   target app's own installer could reset it.
2. Initialize the floor to the currently-installed version on first
   observation of that app, so the very first check can't be tricked into
   accepting something below what's already installed.
3. **Ratchet the floor upward only after a real `Result::Valid` verify
   *and* successful install** — never from an appcast's unsigned
   `sparkle:version` string alone. (Ratcheting on unverified appcast
   metadata would let an attacker "poison" the floor with a fake high
   version number and cause a later *legitimate* update to be rejected as a
   downgrade — a self-inflicted DoS. Don't do that.)
4. **Embedded-version cross-check:** after `verify()` returns `Valid`, before
   invoking the OS opener, extract the real version from the verified bytes
   themselves (zip: `Info.plist` entry; pkg: `Distribution`/`PackageInfo`)
   and confirm it is strictly greater than the current floor. Do not trust
   the appcast's `sparkle:version` field as the final gate — it is unsigned
   metadata and is exactly what T3 lies about while replaying genuinely
   old-but-validly-signed bytes. If the real version is at or below the
   floor, treat it the same as a failed verification (§5): block, fall back
   to browser-open.

This closes the specific gap where signature validity alone (over the raw
installer bytes) says nothing about *which* version those bytes are — an
attacker doesn't need to forge a signature to mount a downgrade attack, they
only need to replay an old signed artifact and lie about its version number
in the unsigned part of the feed.

## 8. GO / NO-GO verdict

**GO-WITH-CONDITIONS.**

Not every app can be made safe: apps without `signatureMetadataPresent`
(no `edSignature`, or no local `SUPublicEDKey` in the bundle) have no
cryptographic basis for verification at all. For those, and for any
enclosure format outside the §6 scope (`.dmg`, anything else), **SSO-15508
must retain exactly today's behavior — `QDesktopServices::
openUrl(entry.enclosureUrl)`** — unchanged, not touched by this feature. The
new download+verify+execute path applies *only* to entries that pass every
one of the following, and where the enclosure is `.zip` or `.pkg`.

**Hard gates before merge (all required):**

1. Trust anchor is the local app-bundle `SUPublicEDKey` only; no
   appcast-supplied key is ever accepted; no key field added to the appcast
   parser (§2).
2. HTTPS-only enforcement (scheme check before any request), no-downgrade
   redirect policy, size cap, and timeout on **both** the appcast fetch and
   the new enclosure fetch (§3).
3. Verify-then-exec with no reopen-by-path gap: in-memory download → verify
   → write verified bytes to a private, unpredictable `QTemporaryFile` →
   execute that exact path (§4).
4. Fail-closed at the call site, not just in the verifier: only
   `Result::Valid` proceeds; every other `Result` blocks and falls back to
   browser-open; new spy-launcher regression test proves zero invocations on
   every non-`Valid` fixture (§5).
5. Persisted, verified-install-only floor-version ratcheting, plus
   post-verification embedded-version cross-check against the appcast's
   claimed version, before execution (§7).
6. Unprivileged execution only — no `sudoExecWithStatus`/admin-privilege path
   wired into this feature for v1 (§6).
7. Enclosure format scope limited to flat `.pkg` (and, if option 2 in §6 is
   taken, `.zip` as verified-reveal only); `.dmg`, in-place bundle swap, and
   anything else falls back to browser-open (§6).
8. Quarantine (`com.apple.quarantine`) explicitly set via `setxattr` on the
   verified artifact before handoff, or the Gatekeeper defense-in-depth claim
   dropped from the design — not assumed (§6, §10.1).

**Residual risk after GO-WITH-CONDITIONS (accepted, not blocking):**

- **Availability, not integrity:** an attacker who controls the feed/host
  can always prevent updates from working (withhold a valid response, break
  the connection) — that's a DoS on the update mechanism, not RCE, and isn't
  fixable from Nexis's side.
- **Unsigned/DSA-only third-party apps stay on manual browser-open
  indefinitely.** This is a publisher-side gap (they haven't shipped Ed25519
  signing), not something Nexis can close; no plan to change this.
- **OS Gatekeeper/quarantine is a second layer _only if_ the implementation
  explicitly sets the quarantine xattr** (§6, §10.1). It is not automatic for
  bytes Nexis downloads and writes itself. If gate 8 is not implemented, this
  layer does not exist and the ed25519 check is the sole control — still the
  primary one, but the design must not claim depth it doesn't have.
- **Local attacker already at the user's privilege level** (T5, given full
  §4 compliance) is the standard, essentially-irreducible baseline every
  comparable updater (Sparkle itself, browser auto-updaters, etc.) accepts —
  once an attacker can already execute code as the same user, the update
  path is not meaningfully more valuable to them than anything else on the
  system.
- **`.dmg` support and elevated install are explicitly deferred**, not
  silently dropped — either would need its own follow-up design review
  before implementation, not folded into SSO-15508.

## 9. Sign-off

- **CISO:** GO-WITH-CONDITIONS per §8, this document. — GeneralCoder-facing
  implementation spec is §§2–7; treat every "Hard gate" item as an
  acceptance criterion on SSO-15508 itself, not optional polish.
- **CTO:** **CO-SIGNED, GO-WITH-CONDITIONS**, 2026-07-27, subject to the four
  corrections in §10 (folded into §§4, 6, 7 and gates 7–8 of §8 in this same
  revision). The security core — local-bundle trust anchor, fail-closed at
  the call site, verify-then-execute with no reopen-by-path, floor-version +
  embedded-version cross-check — is correct as written and I am not asking
  for changes to it.

## 10. CTO co-sign — corrections and rationale

I re-derived the document's code claims against the tree rather than
accepting its citations. Confirmed accurate: the verifier's fail-closed
enum and `verify()` signature taking `const QByteArray &fileData`
(`sparkle_signature_verifier.h`); the trust anchor reading
`bundleInfo.suPublicEDKey` from the local bundle
(`sparkle_update_scanner.cpp:153`); `EnclosureInfo` carrying **no** key field
(`sparkle_appcast_parser.h:29`); `fetchFeed` already using
`NoLessSafeRedirectPolicy` but **not** enforcing an https scheme
(`sparkle_update_scanner.cpp:46`); and the call site being browser-open only
(`homebrew_page.cpp:711`). §§1–5 and §7 stand.

Four defects found, all now corrected in-place:

**10.1 — Gatekeeper was assumed, not obtained (security).** §6 originally
counted the OS quarantine prompt as a second independent control. Files an
app downloads and writes itself carry no `com.apple.quarantine` xattr, so
that control would never have fired. This is the dangerous kind of error —
it inflates the perceived defense depth of the design while contributing
nothing, and the repo's existing `MacOsXattrUtil` *strips* quarantine, so an
implementer pattern-matching on "quarantine helper" would find the exact
wrong tool. Now an explicit gate (§8.8): set the xattr, or drop the claim.

**10.2 — the `.zip` path verifies correctly and then does nothing useful
(product correctness).** `openUrl` on a zip expands a `.app` into the private
temp dir; `/Applications` is untouched. Every security property holds and the
user is still not updated. Resolved by scoping v1 to `.pkg` (preferred) or
`.zip`-as-verified-reveal, and by explicitly fencing off the in-place bundle
swap — that operation is what Sparkle's updater exists to do carefully and it
can destroy a working install on partial failure. It needs its own review,
same as `.dmg`.

**10.3 — the cleanup rule was unimplementable.** §4 said to keep Nexis alive
until "the installer/opener process has exited," but `QDesktopServices::
openUrl` hands off to LaunchServices and returns no waitable handle. An
instruction that cannot be followed gets improvised at 5pm on a Friday;
replaced with auto-remove-on-quit plus a startup sweep for the crash case.

**10.4 — stale plumbing note.** §7 hedged that `AppBundleInfo` "should
expose" `CFBundleIdentifier`; it already does (`plist_util.h:14`). Corrected
so the implementer doesn't add a redundant field.

**One thing to preserve, flagged so it isn't optimized away:** the scanner's
`availableVer <= installedVer` filter (`sparkle_update_scanner.cpp:139`)
compares *unsigned* appcast version strings. It is a UX filter, not a
security control. §7's post-verification embedded-version check is the real
downgrade gate and must not be skipped on the grounds that "the scanner
already compares versions."

**Delegation Quality Bar / lens notes.** Alternatives considered and rejected
(T4): appcast-supplied or TOFU keys — rejected, no protection against the
T1/T2 attacker this feature exists to stop; elevated install via
`sudoExecWithStatus` — rejected for v1, converts any verification bug into
root RCE instead of user-level; Nexis-owned dmg mounting and in-place bundle
swap — rejected, large parsing/failure surface for the delivery value, and
**Build vs Buy** says let Apple's Installer own the install. **Blast Radius**:
unprivileged-only caps a verification bug at user-level code exec.
**Delivery Velocity**: `.pkg`-only v1 ships the whole verified path sooner
than blocking on the bundle-swap design.

## 11. Implementer constraints (binding on SSO-17776 — NexisMaintainer)

Where this document and the SSO-17776 issue description differ, **this
document wins**. Every §8 hard gate is an acceptance criterion, not polish.

**Stack**

- C++17, Qt6, existing Nexis structure. **No new third-party dependency** for
  download, verification, or archive handling. The vendored ed25519 in
  `SparkleSignatureVerifier` is the only crypto in this feature.
- Network work **must** run off the GUI thread with a cancellable progress
  state. The scanner's existing blocking-`QEventLoop`-around-
  `QNetworkAccessManager` shape (`sparkle_update_scanner.cpp:44`) **must not**
  be copied into the enclosure fetch.
- macOS-only path. The Linux build **must** still compile and `ctest` **must**
  stay green.
- UI copy follows the Nexis Design Anchor and the theme-token rule — no
  hardcoded hex colors (BUG-47).

**Security** — threat model in §1, mitigations in §§2–7, gates in §8. No
hardcoded secrets, keys, or URLs in source.

**Performance / resource**

- The size cap (§3) **must** be enforced against a bounded in-memory buffer as
  bytes arrive. A hostile host **must not** be able to advertise a small
  `EnclosureInfo::length` and then stream unbounded bytes to drive Nexis to
  OOM — the cap is on *bytes received*, not on the advertised length.
- Connection and overall-transfer timeouts **must** be set; no unbounded wait.
  The GUI **must** stay responsive with cancel available throughout.
- Verified temp artifacts: auto-remove on quit plus a startup sweep for the
  crash case (§4). Do **not** attempt to wait on the opener process.

**Testing**

- A spy-launcher regression test **must** prove zero launcher invocations for
  every non-`Valid` verifier result. This is the single most important test in
  the change.
- Tests **should** also cover https-scheme rejection, the size cap, and the
  floor-version ratchet.

**Conflict check.** This design deliberately *keeps* the browser-open path
rather than removing it, which reads as dead code against the usual cleanup
instinct. It is intentional and **must not** be refactored away — it is the
safe fallback for every entry that cannot be cryptographically verified.
