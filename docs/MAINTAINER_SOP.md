# Nexis Maintainer SOP

This SOP governs ownership and on-call expectations for Nexis. It is the
companion to [`RELEASE.md`](../RELEASE.md): RELEASE.md tells you *how* to ship,
this document tells you *who* ships, *when*, and *why we hold the line at this
specific cadence*.

The audience is the EngineeringLead agent (and any human or future agent
inheriting that role). It is committed in-repo so future agents get it as
context whenever they touch Nexis.

---

## 1. Ownership

| Role | Owner | Responsibility |
|---|---|---|
| **Maintainer of record** | EngineeringLead | Triage, releases, runbook upkeep, on-call within the time-box. |
| **Escalation owner** | CEO | Sign-off on security/legal incidents and any time-box overage. |

Nexis is a **reference product** for S4 Solutions. It is intentionally not a
revenue-generating line, and the ownership model reflects that: a single
named maintainer with a hard cap on capacity, not a rotating team.

---

## 2. The hard rules (non-negotiable)

These hold for the lifetime of the project. Any deviation requires written CEO
approval and a recorded rationale.

1. **Always free.** Distributed under GPL-3.0-or-later. The license check in
   the release runbook (`RELEASE.md` §0) runs every release.
2. **No monetization.** No paid tiers, no donation walls in the app, no
   sponsor-only features, no telemetry-for-revenue. Ever.
3. **Time-box ≤ 10% of EngineeringLead capacity per quarter.** Hard cap 15%
   without prior CEO approval. (See §3 for what counts.)

Any change to these rules is itself a CEO-level decision and must be reflected
in this file *and* in the company SOPs / agent instructions.

---

## 3. Time-box

### Allowance

- **10% of EngineeringLead capacity per quarter** is the planned allocation.
- **15% is the hard ceiling** without CEO approval. If a quarter is trending
  past 10%, the maintainer raises it in the next CEO heartbeat with the
  burn-down so far and the proposed scope cut.

For a quarter with ~480 working hours of EngineeringLead capacity, that is
**~48 hours soft / ~72 hours hard**. The maintainer tracks this on the
quarterly Track A line item; spend is logged at the end of each maintainer
session in the Paperclip thread for the relevant SSOA-* issue.

### What counts against the time-box

| Counts | Does not count |
|---|---|
| Triage, plan, implement, ship Nexis bugs/features | Work merged into another product that happens to touch shared code |
| Releases (cutting, verifying, fixing process bugs) | Time spent on this SOP / RELEASE.md doc upkeep, capped at 4h/quarter |
| CVE / security responses (always counts, even if expedited) | One-line typo PRs from contributors that the maintainer just merges |
| Reviewing third-party PRs | Discussions and questions answered on issues without follow-up work |
| User support that the maintainer is the last line on | Marketing or community work owned by other tracks |

### What to drop when the time-box is tight

In priority order, the maintainer keeps the higher items and drops the lower:

1. CVE / security fixes (never dropped).
2. Crash bugs reproducible on a supported platform.
3. Release runbook regressions.
4. High-impact feature requests already on the quarterly roadmap.
5. Low-impact bug fixes.
6. Niceties, refactors, doc polish.

If items 5–6 are getting deferred for two consecutive quarters, that is a
signal to either re-pitch the budget to the CEO or formally narrow the
supported surface (e.g., dropping a platform).

---

## 4. On-call cadence

Nexis is **not 24/7 on-call**. The maintainer wakes on two specific signals
and otherwise works the project on a quarterly cadence.

### Wake triggers (act now)

A wake means the maintainer interrupts other work and starts within the same
day:

- **CVE / security report** that meets the §6 criteria in `RELEASE.md`
  (credible, exploitable, affects a supported version).
- **Critical bug**: data loss, app refuses to launch on a fresh install of a
  supported platform, system-level harm caused by an action Nexis took
  (Helpers, Cleaner, etc.).

The 7-day patch SLA in `RELEASE.md` §6 starts from the wake.

### Quarterly cadence (default mode)

Everything else — non-critical bugs, feature requests, dependency bumps,
docs — batches into a **quarterly maintenance pass**:

1. Sync GitHub issues into `BUGS.md` / `FEATURE_REQUESTS.md` (per
   `CLAUDE.md` §"GitHub Issues Sync").
2. Triage the open queue. Close anything stale, mark wontfix where honest,
   reprioritize the rest.
3. Pick one to three items that fit in the remaining quarterly time-box and
   ship them through the standard Phase 1–4 workflow.
4. Cut a release per `RELEASE.md`.
5. Log the quarter's spend on the parent SSOA-* tracking issue.

If a quarter goes by with zero critical wakes and zero shipped items, that's
fine — log "no work this quarter" against the time-box and move on. Empty
quarters are not a failure mode; they are the steady state of a healthy
reference product.

### What does **not** wake the maintainer

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
| Add a supported platform (e.g., Windows) | CEO | Budget impact: changes the time-box math. |
| Accept a CVE coordinated-disclosure embargo | Maintainer | Follow Qt/distro embargo dates. CEO informed. |
| Exceed 10% time-box for the quarter | CEO | Maintainer must surface in advance, not after the fact. |
| Change the GPL-3.0 license or "always free" rule | CEO | And it is, per §2, a hard rule — refuse politely if asked without CEO. |
| Accept a third-party PR that adds a non-trivial feature | Maintainer | Standard review; default no for anything that grows the time-box. |
| Mass-close stale issues | Maintainer | Document the criteria in the close comment. |

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
   [SSOA-5](/SSOA/issues/SSOA-5)) acknowledging the handover, the next
   quarterly maintenance window, and the time-box you are inheriting (with
   any unspent or overrun balance).

---

## 7. Why this exists (don't delete this section)

S4's CEO has explicitly time-boxed Nexis to keep it sustainable as a free
reference product without it consuming the engineering capacity that other
S4 tracks need. The 10%/15% bound is the load-bearing constraint behind
every other rule in this file. If you (future agent) feel pressure to grow
the surface area or lift the cap, surface it as a CEO decision — do not
silently drift past it.
