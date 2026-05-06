# Session Start

Initialize a new work session by gathering project status and presenting a summary.

## Steps

1. **Check for uncommitted work:**
   - Run `git status` to see if there are staged/unstaged changes or untracked files from a previous session
   - If uncommitted work exists, summarize it and ask if it should be committed or stashed

2. **Read open work items — Plane first, MD files as fallback:**
   - Call `list_work_items` for the NEX project filtered to `state_groups=["unstarted","started","backlog"]`, ordered by `-priority`, limit 50
   - You will need to resolve state UUIDs via `list_states` on first use; cache the mapping for the session
   - If the call succeeds and returns results, use those as the authoritative list of open/in-progress items
   - If the call fails (any error) or returns zero results, fall back to the MD files (BUGS.md / FEATURE_REQUESTS.md) if they exist
   - Note in the summary which source was used (Plane or MD files)

3. **Sync GitHub issues to Plane:**
   - Run: `gh issue list --repo s4solutionsllc/Nexis --state open --limit 100 --json number,title,body,labels`
   - For each open issue, search Plane: `search_work_items(project_identifier="NEX", query="<issue title keywords>")`
   - If no match is found, create a Plane work item:
     ```
     create_work_item(
       project_id=<NEX project UUID>,
       name="<issue title>",
       description_html="<p>GitHub: <a href='...'>s4solutionsllc/Nexis#N</a></p><p><issue body></p>",
       priority="medium",
       state=<Todo state UUID>
     )
     ```
   - Classify: bugs (labels contain "bug") get priority "medium" or "high"; features get "none" or "low"
   - Report how many new work items were created, or "GitHub issues up to date"

4. **Check active work items:**
   - Identify Plane items with state `In Progress`

5. **Check build state:**
   - Run `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5` to verify the project builds cleanly
   - Run `ctest --test-dir build --output-on-failure 2>&1 | tail -10` to verify tests pass

6. **Present session summary:**
   Format a clear summary:
   ```
   ## Session Status
   - **Git:** [clean / N uncommitted changes]
   - **Work items (source):** [Plane / MD files]
   - **Open:** X items (list all with NEX-ID, priority, title)
   - **In Progress:** [list with NEX-ID and title, or "none"]
   - **GitHub sync:** [N new items created / up to date]
   - **Build:** [passing / failing]
   - **Tests:** [N/N passing]
   ```

7. **Ask what to work on** — present the open/in-progress items and let the user choose.
