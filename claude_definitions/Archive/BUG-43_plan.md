# BUG-43 Implementation Plan: Host Manager Security & Data Integrity

**Phase:** Roadmap Phase 1 — Security & Data Integrity
**Research:** `claude_definitions/BUG-43_research.md`
**Primary file:** `shared/nexis/Pages/Helpers/host_manage.cpp` / `.h`
**Release target:** v1.2.2 (patch)

---

## Task 1: Replace predictable temp file with `sudo tee` pattern

**Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`

**What:** Eliminate the hardcoded `/tmp/nexis_etc_host_new_content` temp file entirely. Use the `sudo tee` stdin-pipe pattern already proven in `apt_source_tool.cpp`, which pipes content through `CommandUtil::sudoExec("tee", {"/etc/hosts"}, data)`. This is the most secure approach — no temp file, no symlink risk, no TOCTOU window. It also fixes the file ownership problem (Issue 8 in the research) because `tee` writes as root, preserving the original file's ownership.

**Changes:**
- In `on_btnSaveChanges_clicked()`, replace the `FileUtil::writeFile()` + `sudoExec("mv")` two-step with a single `CommandUtil::sudoExec("tee", {"/etc/hosts"}, content.toUtf8())` call
- The content is `mHostFileContent.join("\n") + "\n"` (ensure trailing newline)

**Acceptance criteria:**
- [ ] No temp file is created in `/tmp/`
- [ ] `/etc/hosts` is written directly via sudo tee
- [ ] File ownership and permissions are preserved (tee overwrites in-place)
- [ ] Build passes

---

## Task 2: Add error handling on save with user feedback

**Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`, `shared/nexis/Pages/Helpers/host_manage.h`

**What:** The current save has no error handling — `writeFile()` return value is ignored and `sudoExec()` swallows all exceptions. Add proper error detection and user-visible feedback.

**Changes:**
- After the `sudoExec("tee", ...)` call, verify success. Since `sudoExec()` catches exceptions internally and returns an empty string on failure, we need to verify the write by re-reading `/etc/hosts` and comparing. Alternatively, switch to using `execWithStatus()` with a sudo wrapper — but this would require a new `sudoExecWithStatus()` function.
- **Simpler approach:** After the `tee` call, read back `/etc/hosts` (just the first line or a hash) to verify the write took effect. If the content doesn't match, show a `QMessageBox::critical()` error dialog.
- **Even simpler approach (recommended):** Wrap the `sudoExec` in a try-catch (it's already there). Check if the return value of `sudoExec("tee")` equals the written content (tee echoes stdin to stdout). If the return is empty and content was non-empty, the write likely failed. Show `QMessageBox::warning()` with an appropriate message.
- Add `#include <QMessageBox>` to the includes
- Show success feedback: update `lblChangesMsg` (already exists in the UI at row 5, column 0) with a transient "Changes saved successfully" message
- Show failure feedback: `QMessageBox::warning()` dialog explaining the save failed

**Acceptance criteria:**
- [ ] Failed saves (user cancelled auth, permission denied) show an error dialog
- [ ] Successful saves show a brief success message in `lblChangesMsg`
- [ ] Build passes

---

## Task 3: Fix deletion logic — remove empty line accumulation

**Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`

**What:** Replace the `mHostFileContent.replace(lineNumber, "")` approach with actual removal plus line number recalculation. The current approach leaves empty-string placeholders that produce blank lines in the saved file.

**Changes:**
- In the delete branch of `on_tableViewHosts_customContextMenuRequested()`:
  1. Collect all line numbers to delete (from `LineNumberRole`)
  2. Sort them descending (so removals don't shift earlier indices)
  3. For each: call `mHostFileContent.removeAt(lineNumber)` instead of `replace(lineNumber, "")`
  4. After all removals, rebuild the line number mapping by calling a new `rebuildLineNumbers()` method
- New method `rebuildLineNumbers()`:
  1. Clear `mHostItemList`
  2. Clear `mItemModel`
  3. Call `loadTableData()` to re-parse `mHostFileContent` and rebuild the model
  - This is the simplest correct approach. Performance is fine because the hosts file is small (typically < 100 entries, max ~10k for ad-blocker files per BUG-06).

**Acceptance criteria:**
- [ ] Deleting entries removes them from `mHostFileContent` (no empty placeholders)
- [ ] Line numbers in the model stay in sync after delete
- [ ] Subsequent adds, edits, and deletes after a delete still work correctly
- [ ] Multi-select delete works correctly
- [ ] Saved file has no spurious blank lines from deletions
- [ ] Build passes

---

## Task 4: Add input validation for host entries

**Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`, `shared/nexis/Pages/Helpers/host_manage.h`

**What:** Validate IP address and hostname format before accepting entries. The current code only checks for non-empty fields.

**Changes:**
- Add two private static validation methods:
  - `static bool isValidIP(const QString &ip)` — validates IPv4 (`QHostAddress::parseSubnet` or regex) and IPv6 formats. Use `QHostAddress` to parse; if it's valid it returns true.
  - `static bool isValidHostname(const QString &hostname)` — validates per RFC 1123: alphanumeric + hyphens, labels separated by dots, each label 1-63 chars, total ≤ 253 chars, no leading/trailing hyphens per label
- Add `#include <QHostAddress>` for IP validation
- In `on_btnSave_clicked()`, after the non-empty check:
  1. Trim aliases as well (currently not trimmed)
  2. Validate IP with `isValidIP()` — show inline error via `lblErrorMsg` if invalid
  3. Validate FQDN with `isValidHostname()` — show inline error if invalid
  4. If aliases is non-empty, validate each space-separated alias with `isValidHostname()`
  5. Fix the trailing space: change `QString("%1 %2 %3").arg(ip, fq, aliases)` to only include aliases when non-empty: `aliases.isEmpty() ? QString("%1 %2").arg(ip, fq) : QString("%1 %2 %3").arg(ip, fq, aliases)`

**Acceptance criteria:**
- [ ] Invalid IPv4 addresses (e.g. `999.999.999.999`, `abc`, `192.168.1`) are rejected with clear message
- [ ] Invalid IPv6 addresses are rejected
- [ ] Valid IPv4 and IPv6 addresses are accepted (including `127.0.0.1`, `::1`, `fe80::1`)
- [ ] Invalid hostnames (spaces, special chars, leading hyphens, >253 chars) are rejected
- [ ] Valid hostnames are accepted (including `localhost`, `my-server.local`, `sub.domain.example.com`)
- [ ] Empty aliases are accepted (aliases are optional)
- [ ] Error messages are shown in `lblErrorMsg`
- [ ] No trailing space in constructed line when aliases are empty
- [ ] Build passes

---

## Task 5: Create backup before write

**Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`

**What:** Before overwriting `/etc/hosts`, create a backup at `/etc/hosts.nexis-backup` so the user can recover from bad edits.

**Changes:**
- In `on_btnSaveChanges_clicked()`, before the `tee` write:
  - Run `CommandUtil::sudoExec("cp", {"-p", "/etc/hosts", "/etc/hosts.nexis-backup"})` to create a permission-preserving backup
  - The `-p` flag preserves ownership, permissions, and timestamps
  - Only keep the most recent backup (each save overwrites the previous backup)
  - If the backup copy fails, warn the user but still allow them to proceed (don't block the save — the backup is a safety net, not a gate)

**Acceptance criteria:**
- [ ] `/etc/hosts.nexis-backup` is created before each save
- [ ] Backup preserves original file permissions and ownership
- [ ] Backup failure shows a warning but does not block the save
- [ ] Build passes

---

## Task 6: Add confirmation dialog before save

**Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`

**What:** Show a summary of pending changes and require confirmation before writing to `/etc/hosts`.

**Changes:**
- In `on_btnSaveChanges_clicked()`, before performing the save:
  1. Compare current `mHostFileContent` against the originally loaded content (need to store original at load time)
  2. Count additions, modifications, and deletions
  3. Show `QMessageBox::question()` with summary, e.g.:
     ```
     Save changes to /etc/hosts?

     2 entries added, 1 entry modified, 1 entry deleted.

     A backup will be created at /etc/hosts.nexis-backup.
     ```
  4. If user clicks Cancel, return without saving
- Add a new member `QStringList mOriginalHostFileContent` to store the content as loaded from disk
- Set `mOriginalHostFileContent = mHostFileContent` in `loadIfNeeded()` and after a successful save (so subsequent saves compare against the last saved state)
- Add a helper `bool hasUnsavedChanges() const` that compares `mHostFileContent` to `mOriginalHostFileContent`

**Acceptance criteria:**
- [ ] "Save Changes" shows a confirmation dialog with change summary
- [ ] User can cancel the save from the confirmation dialog
- [ ] Change counts (added, modified, deleted) are accurate
- [ ] Backup note is mentioned in the dialog
- [ ] After a successful save, the "original" baseline is updated
- [ ] Build passes

---

## Task 7: Clean up stale code

**Files:** `shared/nexis/Pages/Helpers/host_manage.h`

**What:** Remove the unused `isAddHost` member variable identified in the research.

**Changes:**
- Remove `bool isAddHost;` from the private section of `host_manage.h` (line 51)

**Acceptance criteria:**
- [ ] `isAddHost` removed from header
- [ ] Build passes (confirms it was truly unused)

---

## Task 8: Build verification + update tracking files

**What:** Verify the full build and update all tracking documents.

**Changes:**
- [ ] Run clean rebuild: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] Update BUG-43 status to `[x]` in `BUGS.md` with resolution notes
- [ ] Update Phase 1 task checkboxes in `docs/IMPLEMENTATION_ROADMAP.md`
- [ ] Update `docs/APPLICATION_OVERVIEW.md` — Helpers section: note validation, backup, confirmation dialog
- [ ] Update `docs/ARCHITECTURE_REVIEW.md` if any architectural patterns changed
- [ ] Move `claude_definitions/BUG-43_research.md` and `claude_definitions/BUG-43_plan.md` to `claude_definitions/Archive/`
- [ ] Commit and push

---

## Implementation Order

Tasks should be implemented in this order due to dependencies:

```
Task 7 (cleanup)         ← Quick, independent
Task 3 (deletion fix)    ← Changes data model, affects subsequent tasks
Task 4 (validation)      ← Independent after Task 3
Task 1 (temp file → tee) ← Changes save mechanism
Task 5 (backup)          ← Builds on new save mechanism
Task 6 (confirmation)    ← Requires original content tracking + save mechanism
Task 2 (error handling)  ← Final layer on top of save mechanism
Task 8 (build + docs)    ← Final verification
```

---

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| `sudo tee` prompts auth dialog twice (backup + write) | Combine into a single sudo command or accept two prompts — users already expect one auth prompt per save |
| Hostname validation too strict (rejects valid but unusual entries) | Use `QHostAddress` for IP (handles all valid formats); keep hostname regex permissive (allow underscores, which some systems accept despite RFC) |
| `rebuildLineNumbers()` after delete causes UI flicker | Use `blockSignals(true)` pattern from existing `loadTableData()` code |
| User edits `/etc/hosts` externally between load and save | Out of scope — the Host Manager has always been a full-file-replace tool. Could add a "file changed on disk" detection in a future enhancement. |
