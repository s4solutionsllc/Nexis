# Session Start

Initialize a new work session by gathering project status and presenting a summary.

## Steps

1. **Check for uncommitted work:**
   - Run `git status` to see if there are staged/unstaged changes or untracked files from a previous session
   - If uncommitted work exists, summarize it and ask if it should be committed or stashed

2. **Read open work items — Plane first, MD files as fallback:**
   - Call `list_work_items` for the current project (use project identifier from CLAUDE.md, e.g. `NEX`) filtered to `state_group="unstarted,started"`, ordered by `-priority,-updated_at`, limit 20
   - If the call succeeds and returns results, use those as the authoritative list of open/in-progress items
   - If the call fails (any error) or returns zero results, fall back to the MD files:
     - Read `FEATURE_REQUESTS.md` — count open (`[ ]`), in-progress (`[~]`), and completed (`[x]`) items
     - Read `BUGS.md` — count open, in-progress, and completed items by severity
   - Note in the summary which source was used (Plane or MD files)

3. **Check active work items:**
   - Scan `backlog/` for any in-progress research or plan files (not in `Archive/`)
   - If using Plane: identify items with state `In Progress` or `In Review`
   - If using MD files: identify items marked `[~]` in tracking files

4. **Check build state:**
   - Run `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5` to verify the project builds cleanly
   - Run `ctest --test-dir build --output-on-failure 2>&1 | tail -10` to verify tests pass

5. **Present session summary:**
   Format a clear summary:
   ```
   ## Session Status
   - **Git:** [clean / N uncommitted changes]
   - **Work items (source):** [Plane / MD files]
   - **Features:** X open, Y in-progress, Z completed
   - **Bugs:** X open, Y in-progress, Z completed
   - **Active work:** [list any in-progress items with IDs and titles]
   - **Build:** [passing / failing]
   - **Tests:** [N/N passing]
   ```

6. **Ask what to work on** — present the open/in-progress items and let the user choose.
