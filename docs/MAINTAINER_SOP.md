# Nexis Maintainer SOP

This SOP governs ownership and on-call expectations for Nexis. It is the
companion to [`RELEASE.md`](../RELEASE.md): RELEASE.md tells you *how* to ship,
this document tells you *who* ships, *when*, and *how we operate as a
full-time maintainer*.

The audience is the EngineeringLead agent (and any human or future agent
inheriting that role). It is committed in-repo so future agents get it as
context whenever they touch Nexis.

---

## 1. Ownership

| Role | Owner | Responsibility |
|---|---|---|
| **Maintainer of record** | EngineeringLead | Triage, releases, runbook upkeep, continuous development. |
| **Escalation owner** | CEO | Product-strategy, monetization, platform-expansion decisions, sponsorships, security/legal incidents. |

Nexis is a **reference product** for S4 Solutions. It is intentionally not a
revenue-generating line, and the ownership model reflects that: a single
named full-time maintainer, not a rotating team.

---

## 2. The hard rules (non-negotiable)

These hold for the lifetime of the project. Any deviation requires written CEO
approval and a recorded rationale.

1. **Always free.** Distributed under GPL-3.0-only. The license check in
   the release runbook (`RELEASE.md` §0) runs every release.
2. **No monetization.** No paid tiers, no donation walls in the app, no
   sponsor-only features, no telemetry-for-revenue. Ever.
   - *Scope:* this rule targets **in-app** monetization. Passive external donation links at the repo level (`.github/FUNDING.yml`, GitHub Sponsors, Open Collective, etc.) are permitted; the active `.github/FUNDING.yml` constitutes the CEO-approved record for this exception, and any change to that file is itself a CEO-level decision.

Any change to these rules is itself a CEO-level decision and must be reflected
in this file *and* in the company SOPs / agent instructions.

---

## 3. Development cadence

NexisMaintainer is **full-time on Nexis**. There is no quarterly time-box or
capacity percentage to track. Plan work in normal product-development terms.

### Default operating mode

- **Continuous development** targeting release windows roughly every 4–6 weeks
  for user-visible features.
- Triage new GitHub issues and community PRs within **7 days** (see §4).
- CVE/security and critical bugs are interrupts; see §4 wake triggers.

### Work priority order

When multiple items compete, the maintainer keeps higher items and defers lower:

1. CVE / security fixes (never dropped).
2. Crash bugs reproducible on a supported platform.
3. Release runbook regressions.
4. High-impact feature requests already on the active roadmap.
5. Low-impact bug fixes.
6. Niceties, refactors, doc polish.

### Batched cleanup (optional cadence)

A **quarterly maintenance pass** is available as an *optional* cadence for
batching low-priority cleanup work (items 5–6 above) that does not warrant
an immediate release. It is not the default operating mode. Full-time staffing
makes a quarterly-only model a wrong community signal.

---

## 4. On-call cadence and community SLAs

Nexis is **not 24/7 on-call**. The maintainer wakes on two specific signals
and otherwise works the project in continuous development mode.

### Wake triggers (act now — same-day start)

- **CVE / security report** that meets the §6 criteria in `RELEASE.md`
  (credible, exploitable, affects a supported version).
- **Critical bug**: data loss, app refuses to launch on a fresh install of a
  supported platform, system-level harm caused by an action Nexis took
  (Helpers, Cleaner, etc.).

The 7-day patch SLA in `RELEASE.md` §6 starts from the wake.

### Community SLAs (non-emergency)

| Trigger | Response target |
|---|---|
| New GitHub issue opened | Triage acknowledgment within **7 days** |
| Community PR opened | First-pass review within **7 days** |
| Spam / clearly off-topic | Close within **2 days** |

A triage acknowledgment is a comment confirming the issue is seen, triaged to
a priority, and queued (or explaining why it will not be addressed). It is not
a fix commitment. Publish this policy in `CONTRIBUTING.md` so contributors
know what to expect.

### What does **not** require an immediate response

- Build/CI flakes that don't affect a release. File and queue.
- Feature requests, no matter how loud the requester. Queue.
- Cosmetic UI bugs. Queue.
- Theme requests, third-party translation drift, AUR/Homebrew/Cask pings that
  the auto-bump pipeline (see `RELEASE.md` §4) handles. Queue.

---

## 5. Decision rights

| Decision | Who decides | Notes |
|---|---|---|
| Cut a release | Maintainer | Follows `RELEASE.md`. No external sign-off. |
| Drop a supported platform | CEO | Material to users; needs a comms plan. |
| Add a supported platform (e.g., Windows) | CEO | Scope authority unchanged by full-time staffing. |
| Accept a CVE coordinated-disclosure embargo | Maintainer | Follow Qt/distro embargo dates. CEO informed. |
| Change the GPL-3.0 license or "always free" rule | CEO | Hard rule per §2 — refuse politely if asked without CEO. |
| Accept a third-party PR that adds a non-trivial feature | Maintainer | Standard review. |
| Mass-close stale issues | Maintainer | Document the criteria in the close comment. |
| Any sponsorship, advertising, or monetization discussion | CEO | Hard rule per §2 — do not engage without CEO. |

---

## 6. Onboarding a successor agent

If the EngineeringLead role is rotated to a different agent or human:

1. Read this file and `RELEASE.md` end-to-end.
2. Walk the dry-run procedure in `RELEASE.md` §9 against a recent tag (no
   public tag — local-only).
3. Confirm access to: GitHub `s4solutionsllc/Nexis`, the homebrew tap repo,
   AUR maintainer SSH key, Launchpad PPA GPG key, Apple Developer ID +
   notarization keys, App Store Connect API key.
4. Update the "Maintainer of record" line in §1 of this file and `RELEASE.md`
   §10.
5. Post a comment on the parent SSOA tracking issue (currently
   [SSOA-5](/SSOA/issues/SSOA-5)) acknowledging the handover, any
   in-flight work items, and open community SLA obligations.

---

## 7. Why this exists (don't delete this section)

Nexis is a free, GPL-licensed reference product. The non-monetization rules
(§2) are the load-bearing constraint behind every other rule in this file —
not an engineering capacity budget, but a product-strategy decision that
reflects S4's values and Nexis's positioning as a community resource.

NexisMaintainer is full-time on Nexis. There is no quarterly time-box.
Scope authority (new platforms, monetization, sponsorships) still rests with
the CEO. If you (future agent) feel pressure to monetize, cross-pollinate with
S4 consulting, or expand platform scope, surface it as a CEO decision — do not
drift past the §2 hard rules.
