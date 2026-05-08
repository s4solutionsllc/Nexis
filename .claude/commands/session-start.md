# Session Start

Initialize a new work session by gathering project status and presenting a summary.

## Steps

1. **Check for uncommitted work:**
   - Run `git status` to see if there are staged/unstaged changes or untracked files from a previous session
   - If uncommitted work exists, summarize it and ask if it should be committed or stashed

2. **Validate Plane project management:**
   - Read `CLAUDE.md` and confirm the Plane project identifier is `NEX` (workspace `s4`)
   - Call `mcp__plane__list_projects` and confirm project `NEX` exists
   - If the project is not found, warn: *"CLAUDE.md references Plane project `NEX` but it wasn't found in the workspace."*
   - Check whether `FEATURE_REQUESTS.md` or `BUGS.md` still exist in the project root; if either exists, note it and ask if the user wants to migrate the remaining items to Plane

3. **Read open work items from Plane:**
   - Call `list_work_items` for the `NEX` project filtered to `state_groups=["unstarted","started","backlog"]`, ordered by `-priority`, limit 50

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
