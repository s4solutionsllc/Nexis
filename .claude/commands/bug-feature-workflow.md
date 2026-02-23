# Bug/Feature Workflow

Execute the three-phase Research → Plan → Implementation workflow for a tracked item.

## Arguments

$ARGUMENTS — Required: The item ID (e.g., "BUG-62" or "FR-52") and optionally a brief description of the issue.

## Phase 1 — Research

1. **Update tracking file:** Mark the item as `[~]` (in progress) in `BUGS.md` or `FEATURE_REQUESTS.md`.

2. **Create research file:** `claude_definitions/{ID}_research.md`

3. **Deep investigation:**
   - Read ALL relevant source files, headers, `.ui` files, QSS, and CMakeLists.txt
   - Trace call chains, signal/slot connections, and data flow end-to-end
   - Understand current behavior, specificities, and edge cases
   - Check platform differences (macOS vs Linux)
   - Review upstream Stacer and QuentiumYT forks for prior art
   - Document file paths, line numbers, code snippets, and architectural context

4. **Write research report** in the research file with all findings.

## Phase 2 — Plan

1. **Create plan file:** `claude_definitions/{ID}_plan.md`

2. **Write implementation plan:**
   - Numbered tasks/phases with specific files and changes
   - Checkboxes (`[ ]`) for each task
   - Clear acceptance criteria per task
   - Build verification steps
   - Rollback notes if applicable

3. **STOP and wait for user approval before proceeding.**

## Phase 3 — Implementation

Only after user approves the plan:

1. Implement all tasks. Do not stop until all phases are complete.
2. Mark each task `[x]` in the plan as it's completed.
3. Run incremental builds after each significant change.
4. **Code quality:**
   - No unnecessary comments
   - No hardcoded colors (use theme tokens)
   - Verify no new regressions introduced
5. Run full test suite: `ctest --test-dir build --output-on-failure`
6. Update tracking file: mark item `[x]`, add resolution notes with commit hash.
7. Update `docs/APPLICATION_OVERVIEW.md` and `docs/ARCHITECTURE_REVIEW.md` if the change affects documented behavior.
8. Commit with conventional format: `type(scope): description (ID)`
9. Move research/plan files to `claude_definitions/Archive/`.
