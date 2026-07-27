# SSO-17776 — Implementation Brief & Acceptance Criteria

**Parent tracker:** SSO-15508 (Wire `SparkleSignatureVerifier` into the actual download+install path)
**Security gate:** SSO-17775 — **CLEARED 2026-07-27**, verdict **GO-WITH-CONDITIONS** (CISO reviewed, CTO co-signed)
**Design of record:** [`sparkle-update-download-verify-execute-design.md`](./sparkle-update-download-verify-execute-design.md)
**Owner:** NexisMaintainer  **Priority:** high  **Author:** Product Owner
**Last updated:** 2026-07-27

This brief is the product-side contract for SSO-17776. The design doc says *how*; this
says *what must be true before the work is accepted*. Where the two disagree, the design
doc's security text wins and this brief is the defect.

## Scope decision (binding)

Nexis downloads, verifies, and executes updates **for signed entries only** — an entry
with an `edSignature` whose app bundle carries a local `SUPublicEDKey`.

Apps without signature metadata **cannot** be made safe: with no signature and no local
public key there is no cryptographic basis for verification at all, and transport
hardening is not a substitute. This is the answer to SSO-15508's open question, and it is
final for this issue.

For every entry outside that set — unsigned, DSA-only, or an enclosure format outside the
supported list — Nexis **retains exactly today's behavior**:
`QDesktopServices::openUrl(entry.enclosureUrl)`. Unchanged. Not refactored, not
"improved," not tidied while passing through.

**Deferred, explicitly, each needing its own design review:** `.dmg` support, in-place
bundle swap, any elevated/privileged install. None of these may be folded into this issue.

## Acceptance criteria

Each of the eight security gates below is an acceptance criterion, not optional polish.
Every one uses *must*. A PR that satisfies seven of eight is not accepted.

- [ ] **AC1 — Trust anchor is local-only.** The verification key is read solely from the
  local app bundle's `SUPublicEDKey`. No appcast-supplied key is accepted under any
  condition, and **no key field is added** to `EnclosureInfo` or the appcast parser.
  (`macos/nexis-core/Info/sparkle_update_scanner.cpp:153` already sources the key from the
  local bundle via `PlistUtil` — keep it that way.)
- [ ] **AC2 — Transport hardening on both fetches.** An explicit HTTPS scheme check runs
  **before** any request is issued, plus a no-downgrade redirect policy, a response size
  cap, and a timeout — on the appcast fetch **and** the new enclosure fetch. Note there is
  currently **no** scheme check anywhere in the scanner; `NoLessSafeRedirectPolicy` at
  `macos/nexis-core/Info/sparkle_update_scanner.cpp:48` covers only part of AC2. This is real work.
- [ ] **AC3 — No reopen-by-path gap.** Download to memory → verify in memory → write the
  verified bytes to a private, unpredictable `QTemporaryFile` → execute that exact path.
  The bytes that are executed are the bytes that were verified; nothing is re-read from a
  user-writable location between verify and exec.
- [ ] **AC4 — Fail closed at the call site.** Only `Result::Valid` proceeds. Every other
  `Result` — missing, invalid, internal error — blocks execution and falls back to
  browser-open. A **new spy-launcher regression test must prove zero launcher invocations
  across every non-`Valid` fixture**; relying on the verifier's internal fail-closed tests
  does not satisfy this.
- [ ] **AC5 — Downgrade control.** A persisted floor version, ratcheted **only** on a
  verified install, **plus** a post-verification cross-check of the artifact's embedded
  version against the appcast's claimed version, both evaluated before execution. The
  scanner's existing `availableVer <= installedVer` filter compares *unsigned* appcast
  strings — it is a UX filter and **must not** be treated as satisfying this gate.
- [ ] **AC6 — Unprivileged execution only.** No `sudoExecWithStatus` and no
  `osascript … with administrator privileges` path is wired into this feature in v1.
- [ ] **AC7 — Enclosure format scope.** Flat `.pkg` only, plus `.zip` as verified-reveal
  per design §6 option 2. Everything else falls back to browser-open. Rationale to keep in
  view: the `.zip` path verifies perfectly and then leaves `/Applications` untouched —
  every security property holds and the user is still not updated. Do not present it to
  the user as a completed update.
- [ ] **AC8 — Quarantine, one way or the other.** `com.apple.quarantine` is explicitly set
  via `setxattr` on the verified artifact before handoff, **or** the Gatekeeper
  defense-in-depth claim is removed from the design doc. Not both. Self-downloaded bytes
  carry no quarantine xattr automatically. **Warning:** the repo's
  `MacOsXattrUtil::stripQuarantine()` (`shared/nexis-core/Utils/macos_xattr_util.h`) does
  the *opposite* — calling it here silently deletes a control instead of adding one.

Product-side criteria carried over from SSO-15508:

- [ ] **AC9 — Real call site.** `SparkleSignatureVerifier::verify()` is invoked on
  downloaded bytes from production code. The verifier is no longer dead code.
- [ ] **AC10 — UI copy matches reality.** The `signatureMetadataPresent`-only copy
  ("Nexis does not verify installer signatures") is corrected for the signed-update path
  so the UI neither understates the protection now provided nor implies verification on
  the unsigned entries that still browser-open. Both states must be distinguishable to the
  user.
- [ ] **AC11 — Docs trio updated** per `CLAUDE.md`: `CHANGELOG.md`,
  `docs/APPLICATION_OVERVIEW.md`, `docs/ARCHITECTURE_REVIEW.md`.

## Accepted residual risk (recorded, not to be re-litigated in the PR)

- Availability DoS on the update mechanism by whoever controls the feed or host. Not
  fixable from Nexis's side, and it is DoS, not RCE.
- Unsigned / DSA-only third-party apps remaining on manual browser-open indefinitely.
  Publisher-side gap.
- A local attacker already holding the user's privilege level — the irreducible baseline
  every comparable updater accepts.

## Review path

Design gate cleared implementation, **not merge**. The resulting PR requires:

1. **Reviewer** — code correctness.
2. **CISO** — confirmation that AC1–AC8 landed as specified, not merely as claimed.
3. **DevOps** — merge; DevOps is the terminal stage and closes only on a real merge.
4. **Product Owner** — acceptance of SSO-15508 against AC9–AC11 once SSO-17776 lands.

Reviewer note: AC2, AC4, AC5, and AC8 are the four most likely to be reported as done
while actually being partially done, because each has a nearby existing mechanism that
resembles the control but does not implement it. Verify those against the code, not the
PR description.
