<!--
Thanks for contributing to Nexis! Please fill in the sections below so the
reviewer has the context they need. Delete sections that don't apply.
-->

## Summary

<!-- 1-3 sentences: what does this PR change and why? -->

## Linked issues

<!--
Reference GitHub issues with `Fixes #NN` (auto-closes on merge) or
`Refs #NN` (related, do not close). Reference Paperclip work items as
`SSO-NNNN`.
-->

- Fixes #
- Refs SSO-

## Changes

<!-- Bulleted list of what changed. Group by area if it helps. -->

-

## Testing

<!--
How did you verify the change? Tick what applies and add notes for anything
non-obvious.
-->

- [ ] Built on Linux (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -jN`)
- [ ] Built on macOS (`cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -jN`)
- [ ] Unit tests (`ctest --test-dir build --output-on-failure`) pass
- [ ] Manually exercised the affected UI / code path

Notes:

## Docs

<!--
Per CLAUDE.md, three living docs must stay in sync. Tick whichever applied
or `N/A` them with one line of justification.
-->

- [ ] `CHANGELOG.md` updated under the current `## [X.Y.Z]` section
- [ ] `docs/APPLICATION_OVERVIEW.md` updated (if features/UI/platform changed)
- [ ] `docs/ARCHITECTURE_REVIEW.md` updated (if signals/singletons/timers/cross-component wiring changed)

## Reviewer checklist

- [ ] Conventional-commit subject line (`type(scope): summary`)
- [ ] No new hardcoded hex colors in C++ (see BUG-47 — use `values.ini` tokens)
- [ ] No new `setFocusPolicy(Qt::NoFocus)` on interactive controls (see [CONTRIBUTING.md](../CONTRIBUTING.md))
- [ ] CI is green
