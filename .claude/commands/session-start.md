# Session Start

Initialize a new work session by gathering project status and presenting a summary.

## Steps

1. **Check for uncommitted work:**
   - Run `git status` to see if there are staged/unstaged changes or untracked files from a previous session
   - If uncommitted work exists, summarize it and ask if it should be committed or stashed

2. **Scan on-disk work items (`backlog/`):**
   - List the `backlog/` directory for `*_research.md` / `*_plan.md` artifacts; each is named by its issue ID (e.g. `NEX-123_plan.md`, `GH64_plan.md`)
   - Paperclip is the system of record and has no MCP — reference issues by their `NEX-<number>` ID and ask the user for an issue's scope when needed. Do **not** call any `mcp__plane__*` tools; the Plane instance is decommissioned
   - Check whether `FEATURE_REQUESTS.md` or `BUGS.md` still exist in the project root; if either exists, note it

3. **Check GitHub issues:**
   - Run: `gh issue list --repo s4solutionsllc/Nexis --state open --limit 100 --json number,title,body,labels`
   - Classify: bugs (labels contain "bug") vs features
   - Report open issues grouped by label

4. **Check active work items:**
   - Identify `backlog/` items that have a `_plan.md` but no corresponding completed/archived artifact (work that looks in progress)

5. **Check build state:**
   - Run `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5` to verify the project builds cleanly
   - Run `ctest --test-dir build --output-on-failure 2>&1 | tail -10` to verify tests pass

6. **Present session summary:**
   Format a clear summary:
   ```
   ## Session Status
   - **Git:** [clean / N uncommitted changes]
   - **Work items (source):** backlog/ MD files (Paperclip = system of record, no MCP)
   - **Open:** X items (list all with NEX-ID, title)
   - **In Progress:** [list with NEX-ID and title, or "none"]
   - **GitHub issues:** [N open, grouped by label]
   - **Build:** [passing / failing]
   - **Tests:** [N/N passing]
   ```

7. **Ask what to work on** — present the open/in-progress items and let the user choose.
