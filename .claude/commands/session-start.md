# Session Start

Initialize a new work session by gathering project status and presenting a summary.

## Steps

1. **Check for uncommitted work:**
   - Run `git status` to see if there are staged/unstaged changes or untracked files from a previous session
   - If uncommitted work exists, summarize it and ask if it should be committed or stashed

2. **Read tracking files:**
   - Read `FEATURE_REQUESTS.md` — count open (`[ ]`), in-progress (`[~]`), and completed (`[x]`) items
   - Read `BUGS.md` — count open, in-progress, and completed items by severity

3. **Check active work items:**
   - Scan `backlog/` for any in-progress research or plan files (not in `Archive/`)
   - Identify items marked `[~]` in tracking files

4. **Check build state:**
   - Run `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5` to verify the project builds cleanly
   - Run `ctest --test-dir build --output-on-failure 2>&1 | tail -10` to verify tests pass

5. **Present session summary:**
   Format a clear summary:
   ```
   ## Session Status
   - **Git:** [clean / N uncommitted changes]
   - **Features:** X open, Y in-progress, Z completed
   - **Bugs:** X open, Y in-progress, Z completed
   - **Active work:** [list any in-progress items with IDs]
   - **Build:** [passing / failing]
   - **Tests:** [N/N passing]
   ```

6. **Ask what to work on** — present the open/in-progress items and let the user choose.
