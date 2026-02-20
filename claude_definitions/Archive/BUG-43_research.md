# BUG-43 Research: Host Manager — Data Integrity and Security Issues

## Overview

The Host Manager is a GUI editor for `/etc/hosts`. It lives on the Helpers page (accessed via a `QStackedWidget` inside `HelpersPage`). The user clicks "Host Manage" to trigger lazy loading (`loadIfNeeded()`), then can add, edit, delete entries, and finally click "Save Changes" to write the modified content to `/etc/hosts` via a temp-file-then-sudo-mv pattern.

**Primary files:**
- `shared/nexis/Pages/Helpers/host_manage.cpp` (280 lines)
- `shared/nexis/Pages/Helpers/host_manage.h` (65 lines)
- `shared/nexis/Pages/Helpers/host_manage.ui` (234 lines)
- `shared/nexis/Pages/Helpers/helpers_page.cpp` (34 lines) — parent container
- `shared/nexis-core/Utils/file_util.cpp` / `.h` — file I/O utilities
- `shared/nexis-core/Utils/command_util.h` — command execution header
- `shared/nexis-core/Utils/command_util_shared.cpp` — `exec()` and `execWithStatus()`
- `macos/nexis-core/Utils/command_util_platform.cpp` — macOS `sudoExec()` via osascript
- `linux/nexis-core/Utils/command_util_platform.cpp` — Linux `sudoExec()` via pkexec

---

## Data Model

### `mHostFileContent` — The Backing Store

**Declaration** (`host_manage.h:58`):
```cpp
QStringList mHostFileContent;
```

This is a `QStringList` that holds every line of `/etc/hosts` exactly as read from disk. It is the single source of truth for the file content and is what gets written back to disk on save.

**Loading** (`host_manage.cpp:18`):
```cpp
mHostFileContent = FileUtil::readListFromFile("/etc/hosts");
```

`FileUtil::readListFromFile()` (`file_util.cpp:24-28`) reads the entire file, calls `.trimmed()` on the result (stripping leading/trailing whitespace from the whole string), then splits on `"\n"`. The `.trimmed()` call means:
- No trailing empty entry from a final newline
- No leading blank lines if the file starts with whitespace
- The QStringList indices (0-based) map directly to the trimmed line numbers

### `mHostItemList` — The Parsed Host Entries

**Declaration** (`host_manage.h:59`):
```cpp
QMap<int, HostItem> mHostItemList;
```

A `QMap<int, HostItem>` mapping **line number** (index into `mHostFileContent`) to a parsed `HostItem`. Only non-comment, non-empty lines with at least 2 whitespace-separated tokens are included.

**`HostItem` struct** (`host_manage.h:17-23`):
```cpp
class HostItem {
public:
    QString ip;
    QString fullQualified;
    QString aliases;
};
```

### `mItemModel` / `mSortFilterModel` — The Table Model

**Declaration** (`host_manage.h:54-55`):
```cpp
QStandardItemModel *mItemModel;
QSortFilterProxyModel *mSortFilterModel;
```

The `QStandardItemModel` has 3 columns: IP Address, Fully Qualified, Aliases. Each row stores the original line number in `LineNumberRole` (`Qt::UserRole + 2`, defined in `nexis_roles.h:11`) on column 0's `QStandardItem`. The `QSortFilterProxyModel` sits on top for sorting by column headers.

### `updatedLine` — Edit Tracking

**Declaration** (`host_manage.h:61`):
```cpp
int updatedLine;
```

Initialized to `-1` in the constructor (`host_manage.cpp:26`). When editing an existing entry, this is set to the line number (from `LineNumberRole`). When adding a new entry, it stays `-1`. Reset to `-1` after save or cancel.

---

## Loading Flow

### `loadHostItems()` (host_manage.cpp:69-92)

Iterates every line in `mHostFileContent` with an integer counter `i`:

```cpp
int i = 0;
for (const QString &line: mHostFileContent) {
    if (!line.trimmed().startsWith("#") && !line.trimmed().isEmpty()) {
        static const QRegularExpression whitespace("\\s+");
        QStringList lineItems = line.trimmed().split(whitespace);
        if (lineItems.count() > 1) {
            HostItem hItem;
            hItem.ip = lineItems.at(0).trimmed();
            hItem.fullQualified = lineItems.at(1).trimmed();
            hItem.aliases = lineItems.count() > 2 ? lineItems.mid(2).join(" ") : "";
            mHostItemList.insert(i, hItem);
        }
    }
    i++;
}
```

Key observations:
- Comments (lines starting with `#`) and empty lines are **skipped** but still count toward the line number `i`.
- The line number `i` stored in `mHostItemList` is the direct index into `mHostFileContent`.
- The `static const` regex is compiled once (BUG-06 optimization).

### `loadTableData()` (host_manage.cpp:94-117)

Clears the model, iterates `mHostItemList` (which is a QMap so it iterates in key order = line number order), and appends rows. Each row stores the line number via `LineNumberRole`.

### `createRow()` (host_manage.cpp:119-137)

Creates 3 `QStandardItem` objects. Column 0 gets the `LineNumberRole` data set to `item.first` (the line number). All columns get `SortRole` and `ToolTipRole`.

---

## Issue 1: Predictable Temp File (Security)

### The Code

**`on_btnSaveChanges_clicked()`** (`host_manage.cpp:221-229`):
```cpp
void HostManage::on_btnSaveChanges_clicked()
{
    FileUtil::writeFile("/tmp/nexis_etc_host_new_content", mHostFileContent.join("\n"));
    try {
        CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"});
    } catch (QString ex) {
        qDebug() << ex;
    }
}
```

### Analysis

**The vulnerability:** The hardcoded path `/tmp/nexis_etc_host_new_content` is world-predictable. On a multi-user system, an attacker can:

1. **Symlink attack:** Create a symlink at `/tmp/nexis_etc_host_new_content` pointing to a file the attacker wants overwritten. When `FileUtil::writeFile()` opens the path with `QIODevice::WriteOnly | QIODevice::Truncate`, it follows the symlink and truncates/overwrites the target. The subsequent `sudo mv` then moves the attacker's chosen file content to `/etc/hosts`.

2. **Race condition (TOCTOU):** Between `FileUtil::writeFile()` completing and `CommandUtil::sudoExec("mv", ...)` executing, an attacker can replace the temp file content with malicious hosts entries. The `sudo mv` then installs the attacker's content as `/etc/hosts`.

3. **Pre-creation attack:** The attacker creates `/tmp/nexis_etc_host_new_content` with restrictive permissions (owned by attacker, mode 000). `FileUtil::writeFile()` will fail to open it (returns `false`), but the return value is ignored (Issue 2), and the `sudo mv` still runs — moving whatever content the attacker placed there to `/etc/hosts`.

**The `FileUtil::writeFile()` implementation** (`file_util.cpp:31-46`):
```cpp
bool FileUtil::writeFile(const QString &path, const QString &content, const QIODevice::OpenMode &mode)
{
    QFile file(path);
    if(file.open(mode)) {
        QTextStream stream(&file);
        stream << content.toUtf8() << Qt::endl;
        file.close();
        return true;
    }
    return false;
}
```

Default mode is `QIODevice::WriteOnly | QIODevice::Truncate`. This will follow symlinks. It returns `bool` indicating success/failure, but the caller ignores it.

**Note:** `QFile::open()` does NOT create the file atomically. It opens/creates with standard POSIX semantics — no `O_EXCL`, no `O_NOFOLLOW`.

### `QTemporaryFile` — The Proper Solution

`QTemporaryFile` uses `mkstemp()` internally, which:
- Creates files with mode 0600 (owner-only read/write)
- Generates an unpredictable filename
- Opens the file atomically (no TOCTOU window between name generation and open)

**Current codebase usage:** No `QTemporaryFile` usage exists anywhere in the Nexis codebase (confirmed via grep). This would be the first instance.

### Alternative Pattern in Codebase

The APT Source Tool (`linux/nexis-core/Tools/apt_source_tool.cpp:194,230`) uses `sudoExec("tee", args, data)` to write directly to protected files via sudo, piping the content through stdin. This avoids temp files entirely but requires the data as a `QByteArray` parameter to `sudoExec()`.

---

## Issue 2: No Error Handling on Save

### `FileUtil::writeFile()` Return Value Ignored

At `host_manage.cpp:223`:
```cpp
FileUtil::writeFile("/tmp/nexis_etc_host_new_content", mHostFileContent.join("\n"));
```

`writeFile()` returns `bool` — `true` if the file was opened and written successfully, `false` if `QFile::open()` failed. The Host Manager discards this return value. If the write fails (disk full, permission denied on `/tmp`, symlink to unwritable target), the code proceeds to the `sudo mv` anyway.

### `CommandUtil::sudoExec()` Error Propagation

**macOS implementation** (`macos/nexis-core/Utils/command_util_platform.cpp:5-31`):
```cpp
QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    QString result("");
    // ... builds command string ...
    try {
        result = CommandUtil::exec("osascript",
            {"-e", QString("do shell script \"%1\" with administrator privileges").arg(fullCmd)},
            data);
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return result;
}
```

**Linux implementation** (`linux/nexis-core/Utils/command_util_platform.cpp:5-18`):
```cpp
QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    QString result("");
    args.push_front(cmd);
    try {
        result = CommandUtil::exec("pkexec", args, data);
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return result;
}
```

Both implementations catch the `QString` exception thrown by `CommandUtil::exec()` and log it with `qCritical()`, then return an empty string. They do NOT re-throw.

**`CommandUtil::exec()`** (`command_util_shared.cpp:10-34`):
```cpp
QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);
    // ... write data if present ...
    process->waitForFinished(timeoutMs);
    QTextStream stdOut(process->readAllStandardOutput());
    QString err = process->errorString();
    process->kill();
    process->close();
    if (process->error() != QProcess::UnknownError)
        throw err;
    return stdOut.readAll().trimmed();
}
```

The throw only happens when `QProcess` itself reports an error (like the executable not being found). If `mv` runs but exits with a non-zero code (e.g., permission denied by the user cancelling the auth dialog, or the source file missing), `QProcess` still reports `UnknownError` (which means "no QProcess error"), so no exception is thrown. The `mv` stderr output goes uncaptured (only stdout is read).

**In the Host Manager** (`host_manage.cpp:224-228`):
```cpp
try {
    CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"});
} catch (QString ex) {
    qDebug() << ex;
}
```

Even if an exception IS thrown, it's caught and logged with `qDebug()` — no user-visible feedback. The in-memory model (`mHostFileContent`, `mHostItemList`, `mItemModel`) remains in its edited state regardless of whether the save succeeded or failed. The user has no way to know if `/etc/hosts` was actually updated.

**Failure scenarios with no user feedback:**
1. User cancels the macOS authentication dialog (osascript returns error)
2. User cancels the Linux pkexec dialog
3. The temp file doesn't exist (writeFile failed silently)
4. `/etc/hosts` is immutable (`chattr +i` on Linux)
5. Disk is full
6. SELinux/AppArmor blocks the mv

### The `execWithStatus()` Alternative

The codebase already has `CommandUtil::execWithStatus()` (`command_util_shared.cpp:36-59`) which returns an `ExecResult` struct with `output`, `error`, and `exitCode`. This was added for BUG-31 (GNOME Settings error handling). It could be used here to get proper exit code checking, but it's only available for non-sudo commands and would need a `sudoExecWithStatus()` variant.

---

## Issue 3: Empty Line Placeholders on Delete

### The Deletion Code

**`on_tableViewHosts_customContextMenuRequested()`** (`host_manage.cpp:254-276`):
```cpp
else if (action->data().toString() == "delete") {
    QList<int> sourceRows;
    while (!selectionModel->selectedRows().isEmpty()) {
        QModelIndex proxyIndex = selectionModel->selectedRows().first();
        int lineNumber = mSortFilterModel->index(proxyIndex.row(), 0).data(LineNumberRole).toInt();
        QModelIndex sourceIndex = mSortFilterModel->mapToSource(proxyIndex);
        sourceRows.append(sourceIndex.row());

        mHostFileContent.replace(lineNumber, "");    // <-- REPLACES WITH EMPTY STRING
        mHostItemList.remove(lineNumber);

        selectionModel->select(proxyIndex, QItemSelectionModel::Deselect);
    }
    selectionModel->clearSelection();

    std::sort(sourceRows.begin(), sourceRows.end(), std::greater<int>());
    for (int row : sourceRows)
        mItemModel->removeRow(row);

    ui->lblHostTitle->setText(tr("Hosts (%1)").arg(mHostItemList.count()));
}
```

### Analysis

At line 263:
```cpp
mHostFileContent.replace(lineNumber, "");
```

The deleted entry's line in `mHostFileContent` is replaced with an empty string `""` rather than being removed from the list. This means:

1. **File bloat:** Each delete adds a blank line to the saved file. Repeatedly deleting and re-adding entries accumulates blank lines in `/etc/hosts`.

2. **Ghost lines in the file:** When saved, `mHostFileContent.join("\n")` produces consecutive `\n\n` sequences for each deleted entry, resulting in blank lines in the hosts file. While blank lines are valid in `/etc/hosts` and ignored by resolvers, they're ugly and unexpected.

3. **Preserved line number stability:** The reason this approach was used is to maintain the correspondence between `mHostFileContent` indices and the `LineNumberRole` values stored in the model. If `removeAt()` were used instead, all line numbers after the deleted entry would shift, invalidating every `LineNumberRole` stored in model rows below the deletion point.

**Why empty string and not removal:** The entire data model relies on stable line-number-to-index mapping. `mHostItemList` (QMap<int, HostItem>) keys are indices into `mHostFileContent`. The model's `LineNumberRole` stores these indices. Removing an element from `mHostFileContent` would shift all subsequent indices, breaking the mapping for all entries below the deleted one. The empty string placeholder preserves index stability at the cost of file cleanliness.

---

## Issue 4: Line Number Drift After Mixed Add/Delete

### The Add Flow

**`on_btnSave_clicked()` — new entry branch** (`host_manage.cpp:179-184`):
```cpp
if (updatedLine == -1) {
    int lineNum = mHostFileContent.size();
    mHostFileContent.append(line);
    mHostItemList.insert(lineNum, hItem);
    mItemModel->appendRow(createRow(QPair<int, HostItem>(lineNum, hItem)));
}
```

When adding a new entry:
- `lineNum = mHostFileContent.size()` — gets the current size as the new line number
- `mHostFileContent.append(line)` — adds the new line at the end
- The new entry's `LineNumberRole` is `lineNum`

### The Problem

**Scenario demonstrating drift:**

Given an initial `/etc/hosts` with 5 lines (indices 0-4):
```
127.0.0.1   localhost          # line 0
::1         localhost          # line 1
192.168.1.1 myserver           # line 2
10.0.0.1    database           # line 3
10.0.0.2    cache              # line 4
```

`mHostFileContent.size() = 5`

1. **Delete line 2** (myserver): `mHostFileContent[2] = ""`. Size is still 5.
2. **Delete line 3** (database): `mHostFileContent[3] = ""`. Size is still 5.
3. **Add new entry** (10.0.0.5 newserver): `lineNum = 5`, appended at index 5. Size becomes 6.
4. **Add another entry** (10.0.0.6 another): `lineNum = 6`, appended at index 6. Size becomes 7.

`mHostFileContent` is now:
```
[0] "127.0.0.1 localhost"
[1] "::1 localhost"
[2] ""                          <-- ghost from delete
[3] ""                          <-- ghost from delete
[4] "10.0.0.2 cache"
[5] "10.0.0.5 newserver"
[6] "10.0.0.6 another"
```

The saved file will be:
```
127.0.0.1 localhost
::1 localhost


10.0.0.2 cache
10.0.0.5 newserver
10.0.0.6 another
```

**This is actually mostly correct** — line numbers don't truly "drift" because the empty-placeholder approach preserves index stability. The line numbers in the model remain valid indices into `mHostFileContent`. The issue is cosmetic (blank lines in the file) rather than a data corruption bug.

However, there IS a subtle issue: **re-loading from disk after a save breaks the mapping.** If the user saves (writes the file with blank lines), then the page is navigated away and back (triggering `loadIfNeeded()` which does nothing because `mLoaded = true`), the in-memory state is still consistent. But if the app is restarted and the file is re-read, the blank lines are parsed as empty entries that get skipped by `loadHostItems()`, so they just waste vertical space in the file.

**The real drift scenario** would require `removeAt()` instead of `replace()`, which is not what the current code does. The current approach avoids drift but at the cost of file bloat.

---

## Issue 5: No Input Validation

### Add/Edit Dialog

The Add/Edit form is embedded in the `host_manage.ui` as `widgetAddEditHost`, containing:
- `txtIP` — `QLineEdit` with placeholder "IP Address *"
- `txtFullyQualified` — `QLineEdit` with placeholder "Fully Qualified Name *"
- `txtAliases` — `QLineEdit` with placeholder "Aliases"
- `btnSave` — saves the entry to the in-memory model
- `btnCancel` — hides the form

### Validation Code

**`on_btnSave_clicked()`** (`host_manage.cpp:162-211`):
```cpp
void HostManage::on_btnSave_clicked()
{
    if (ui->txtIP->text().isEmpty() || ui->txtFullyQualified->text().isEmpty()) {
        ui->lblErrorMsg->setText(tr("The IP and Fully Qualified fields are required."));
        ui->lblErrorMsg->show();
    }
    else {
        QString ip = ui->txtIP->text().trimmed();
        QString fq = ui->txtFullyQualified->text().trimmed();
        QString aliases = ui->txtAliases->text();       // NOTE: no .trimmed()
        QString line = QString("%1 %2 %3").arg(ip, fq, aliases);
        // ... proceeds to add/edit ...
    }
}
```

**The only validation is non-empty check.** There is:
- No IP address format validation (IPv4 or IPv6)
- No hostname format validation (RFC 952/1123)
- No alias format validation
- No check for whitespace-only input (`.isEmpty()` returns false for `"  "`)
- No check for tab/newline characters in input (which could inject extra lines)
- No check for `#` characters (which would create an unintended comment)
- No length limits
- No duplicate entry detection

**What can go wrong:**
1. Entering `not.an.ip` as IP produces a valid-looking but broken hosts entry
2. Entering `my server` (with space) as hostname — spaces in the hostname field are valid in the QLineEdit but would be parsed as two separate fields when the file is re-read
3. Entering `hostname\nmalicious 127.0.0.1` could inject a newline (though QLineEdit normally prevents newlines)
4. Entering `# comment` as IP would create a comment line that loses the entry on reload
5. The aliases field is NOT trimmed (line 171: `ui->txtAliases->text()` without `.trimmed()`), so trailing spaces create trailing whitespace in the file

**The constructed line** at line 172:
```cpp
QString line = QString("%1 %2 %3").arg(ip, fq, aliases);
```
If aliases is empty, this produces `"ip fqdn "` with a trailing space.

---

## Issue 6: No Backup Before Write

### Confirmed: No Backup Mechanism

Searching the entire Host Manager codebase, there is:
- No `QFile::copy()` of `/etc/hosts` before writing
- No backup file creation (e.g., `/etc/hosts.bak`, `/etc/hosts.nexis-backup`)
- No undo/revert functionality
- No diff view before saving
- No way to restore the original file if the save introduces errors

The `on_btnSaveChanges_clicked()` function goes directly from the in-memory model to disk:

```cpp
void HostManage::on_btnSaveChanges_clicked()
{
    FileUtil::writeFile("/tmp/nexis_etc_host_new_content", mHostFileContent.join("\n"));
    try {
        CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"});
    } catch (QString ex) {
        qDebug() << ex;
    }
}
```

No pre-save snapshot. No backup. No recovery path.

**Comparison with other tools:**
- `visudo` creates a temp copy and only moves it if validation passes
- `hostctl` maintains backup files
- NetworkManager preserves the previous file as `.bak`

---

## Issue 7: No Confirmation Dialog

### Confirmed: Immediate Write

The "Save Changes" button (`btnSaveChanges` in `host_manage.ui:52-65`) directly triggers `on_btnSaveChanges_clicked()` via Qt's auto-connect mechanism (name-based slot connection). There is:

- No `QMessageBox::question()` confirmation
- No diff preview showing what changed
- No "Are you sure?" prompt
- No summary of changes (N entries added, M entries deleted, K entries modified)

The button is located at the bottom-right of the Host Manager widget (row 5, column 1, right-aligned in the grid layout). It has `accessibleName="primary"` for themed styling but no guard against accidental clicks.

**Risk:** A single accidental click on "Save Changes" immediately writes to `/etc/hosts` with elevated privileges. If the user has made unintended changes (accidental deletes, typos in IP addresses), those changes are committed without review.

---

## Utility API Details

### `FileUtil::writeFile()` (`file_util.cpp:31-46`)

```cpp
bool FileUtil::writeFile(const QString &path, const QString &content, const QIODevice::OpenMode &mode)
{
    QFile file(path);
    if(file.open(mode)) {
        QTextStream stream(&file);
        stream << content.toUtf8() << Qt::endl;
        file.close();
        return true;
    }
    return false;
}
```

- **Returns:** `bool` — `true` on success, `false` if `QFile::open()` fails
- **Default mode:** `QIODevice::WriteOnly | QIODevice::Truncate`
- **Encoding note:** `content.toUtf8()` converts to UTF-8 bytes, then `QTextStream <<` writes them. The `Qt::endl` appends a newline and flushes the stream.
- **Symlink behavior:** `QFile::open()` follows symlinks by default
- **Atomicity:** Not atomic. File is truncated on open, then written. A crash or power loss during write leaves a partial file.
- **Permission:** Writes as the current user (not root). For `/tmp/` this is fine; for `/etc/hosts` this would fail without sudo.

### `FileUtil::readListFromFile()` (`file_util.cpp:24-28`)

```cpp
QStringList FileUtil::readListFromFile(const QString &path, const QIODevice::OpenMode &mode)
{
    QStringList list = FileUtil::readStringFromFile(path, mode).trimmed().split("\n");
    return list;
}
```

- **Returns:** `QStringList` of lines
- **Important:** `.trimmed()` is called on the entire file content BEFORE splitting. This strips leading and trailing whitespace/newlines from the whole file, meaning:
  - No trailing empty entry from the file's final newline
  - Leading blank lines at the top of the file are stripped
  - The returned list indices are NOT true 1:1 line numbers if the file had leading/trailing blank lines

### `CommandUtil::sudoExec()` — macOS (`macos/nexis-core/Utils/command_util_platform.cpp:5-31`)

```cpp
QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    QString result("");
    QString fullCmd = cmd;
    for (const QString &arg : args) {
        QString escaped = arg;
        escaped.replace("'", "'\\''");
        fullCmd += " '" + escaped + "'";
    }
    fullCmd.replace("\\", "\\\\");
    fullCmd.replace("\"", "\\\"");
    try {
        result = CommandUtil::exec("osascript",
            {"-e", QString("do shell script \"%1\" with administrator privileges").arg(fullCmd)},
            data);
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return result;
}
```

- Uses `osascript` with `do shell script ... with administrator privileges` to get root access
- Prompts the user with a macOS authentication dialog
- Shell-escapes arguments with single quotes, then escapes backslashes and double quotes for the AppleScript string
- Catches exceptions and logs with `qCritical()`, returns empty string on failure
- **Does NOT re-throw** — the caller cannot distinguish success from failure based on return value alone (empty string could be valid output)

### `CommandUtil::sudoExec()` — Linux (`linux/nexis-core/Utils/command_util_platform.cpp:5-18`)

```cpp
QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    QString result("");
    args.push_front(cmd);
    try {
        result = CommandUtil::exec("pkexec", args, data);
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return result;
}
```

- Uses `pkexec` (PolicyKit) for privilege escalation
- Prepends the command to the args list and runs through pkexec
- Same error swallowing pattern as macOS

### `CommandUtil::exec()` (`command_util_shared.cpp:10-34`)

```cpp
QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);
    // ... data write ...
    process->waitForFinished(timeoutMs);
    QTextStream stdOut(process->readAllStandardOutput());
    QString err = process->errorString();
    process->kill();
    process->close();
    if (process->error() != QProcess::UnknownError)
        throw err;
    return stdOut.readAll().trimmed();
}
```

- Only throws when `QProcess` itself has an error (executable not found, crashed, etc.)
- Does NOT throw when the child process exits with non-zero status
- stderr is NOT captured — only stdout is returned
- The `process->kill()` after `waitForFinished()` is defensive cleanup

---

## The Complete Save Flow (End-to-End)

1. User clicks "Save Changes" button (`btnSaveChanges`)
2. Qt auto-connects to `on_btnSaveChanges_clicked()` (line 221)
3. `mHostFileContent.join("\n")` assembles the file content from the QStringList, including any empty-string placeholders from deletions
4. `FileUtil::writeFile("/tmp/nexis_etc_host_new_content", ...)` writes to the predictable temp path:
   - Opens file with `WriteOnly | Truncate`
   - Writes content as UTF-8 via QTextStream
   - Appends `Qt::endl` (trailing newline)
   - Returns `true`/`false` (IGNORED by caller)
5. `CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"})`:
   - On macOS: runs `osascript -e 'do shell script "mv '/tmp/nexis_etc_host_new_content' '/etc/hosts'" with administrator privileges'`
   - On Linux: runs `pkexec mv /tmp/nexis_etc_host_new_content /etc/hosts`
   - User sees an authentication dialog
   - If user cancels: exception might be thrown by `exec()` if osascript/pkexec itself fails, caught by `sudoExec()`, logged with `qCritical()`, empty string returned
   - If mv fails (source missing, permission denied): `exec()` may NOT throw (QProcess ran fine), returns empty/error string
6. Exception handler at line 226-228 catches `QString`, logs with `qDebug()` (note: weaker than `qCritical()` used in `sudoExec()`)
7. No UI feedback to user regardless of outcome
8. In-memory state remains in edited form regardless of disk state

---

## The Complete Add Flow (End-to-End)

1. User clicks "New Host" button (`btnNewHost`) -> `on_btnNewHost_clicked()` (line 139)
2. Shows `widgetAddEditHost`, clears all text fields, sets `updatedLine = -1`
3. User fills in IP, FQDN, optionally aliases
4. User clicks "Save" button -> `on_btnSave_clicked()` (line 162)
5. Validation: only checks if IP and FQDN are non-empty
6. Constructs line: `"%1 %2 %3".arg(ip, fq, aliases)` (aliases may be empty, producing trailing space)
7. `lineNum = mHostFileContent.size()` — index of next append position
8. `mHostFileContent.append(line)` — adds to backing store
9. `mHostItemList.insert(lineNum, hItem)` — adds to parsed map
10. `mItemModel->appendRow(...)` — adds to table model with `LineNumberRole = lineNum`
11. Hides the add/edit widget, updates host count label
12. **Entry is only in memory** — not yet on disk. User must click "Save Changes" to persist.

---

## The Complete Edit Flow (End-to-End)

1. User right-clicks a row in the table -> context menu appears
2. User clicks "Edit" -> `on_tableViewHosts_customContextMenuRequested()` (line 241-253)
3. `updatedLine` is set to the `LineNumberRole` value of the selected row
4. Text fields populated from `mHostItemList.value(updatedLine)`
5. `widgetAddEditHost` shown
6. User modifies fields and clicks "Save" -> `on_btnSave_clicked()` (line 162)
7. Since `updatedLine != -1`, takes the edit branch (lines 185-204):
   - `mHostFileContent.replace(updatedLine, line)` — updates backing store at the correct index
   - `mHostItemList[updatedLine] = hItem` — updates parsed map
   - Finds the model row with matching `LineNumberRole` and updates the 3 column texts + data roles
8. Resets `updatedLine = -1`, hides form

---

## The Complete Delete Flow (End-to-End)

1. User right-clicks row(s) in table -> context menu
2. User clicks "Delete" -> `on_tableViewHosts_customContextMenuRequested()` (line 254-276)
3. Iterates selected rows:
   - Gets `lineNumber` from `LineNumberRole`
   - Gets source model row index
   - `mHostFileContent.replace(lineNumber, "")` — blanks the backing store entry
   - `mHostItemList.remove(lineNumber)` — removes from parsed map
   - Deselects the proxy row
4. Sorts source rows descending, removes from model in reverse order (preserves indices)
5. Updates host count label
6. **Entry removed from model and map, but backing store retains empty placeholder**

---

## Additional Observations

### No Enable/Disable Functionality

The Host Manager has no toggle to comment/uncomment entries (enable/disable). The only operations are Add, Edit, Delete. The original bug description mentions "Enable/Disable flows" but these do not exist in the current code.

### Stale `isAddHost` Member

`host_manage.h:51` declares `bool isAddHost;` but this member is **never used** in `host_manage.cpp`. It appears to be a leftover from the original Stacer codebase. The add/edit distinction is handled entirely by `updatedLine` (-1 = add, >= 0 = edit).

### `readListFromFile` Trimming Caveat

`FileUtil::readListFromFile()` calls `.trimmed()` on the entire file content before splitting. This means if `/etc/hosts` has leading blank lines or a trailing newline (virtually all hosts files end with `\n`), the trimming adjusts the content. The resulting QStringList may not have indices that correspond to physical line numbers in the file. For example:

```
<blank line>
127.0.0.1 localhost
::1 localhost
<trailing newline>
```

After `readStringFromFile().trimmed()`, becomes `"127.0.0.1 localhost\n::1 localhost"`, which splits into a 2-element list with indices 0 and 1. But in the actual file, `127.0.0.1 localhost` is on line 2 (1-indexed). This is acceptable because the Host Manager only uses its own indices and never needs to reference physical line numbers — it re-writes the entire file.

### The `tee` Pattern as Alternative Save Strategy

The APT Source Tool uses `CommandUtil::sudoExec("tee", args, data)` to write to protected files. The content is passed via the `data` parameter (stdin to the process). This eliminates the temp file entirely and writes directly to the target with root privileges. However:
- The `data` parameter flows through `QProcess::write()`, so the content is piped to `tee`'s stdin
- `tee` writes to the file path specified in `args`
- This is the most secure approach — no temp file, no symlink risk
- The main downside is that `tee` also echoes to stdout, so the return value of `sudoExec()` would be the entire file content

### File Ownership/Permissions After `mv`

When `sudo mv /tmp/nexis_etc_host_new_content /etc/hosts` runs:
- The new `/etc/hosts` will have the ownership of the temp file (current user, not root)
- The permissions will be whatever `QFile::open()` used (typically 0644 based on umask)
- The original `/etc/hosts` permissions (typically `root:root 0644` or `root:wheel 0644` on macOS) are lost
- Some systems require specific ownership on `/etc/hosts`

This is another security/correctness issue not listed in the original bug but worth noting. Using `tee` or `sudo cp` + `sudo chown`/`sudo chmod` would preserve proper ownership.

### `QTextStream << content.toUtf8() << Qt::endl` Double Encoding Risk

In `FileUtil::writeFile()`, `content.toUtf8()` produces a `QByteArray`, and `QTextStream <<` for `QByteArray` writes the raw bytes. Then `Qt::endl` writes a newline. However, `QTextStream` by default uses the system locale codec. Since the content is already explicitly UTF-8 encoded, this could theoretically cause double-encoding on systems where the locale is not UTF-8. In practice, most modern systems use UTF-8 locales, so this is a minor concern.

---

## Summary of Issues by Severity

| # | Issue | Severity | Root Cause |
|---|-------|----------|------------|
| 1 | Predictable temp file path | HIGH (security) | Hardcoded `/tmp/nexis_etc_host_new_content` |
| 2 | No error handling on save | MEDIUM | `writeFile()` return ignored; `sudoExec()` swallows errors |
| 3 | Empty line placeholders on delete | LOW | `replace(lineNumber, "")` instead of `removeAt()` |
| 4 | Line number drift (cosmetic) | LOW | Consequence of #3; indices stay valid but file gets blank lines |
| 5 | No input validation | MEDIUM | Only `.isEmpty()` check; no format/content validation |
| 6 | No backup before write | MEDIUM | No `QFile::copy()` or backup mechanism |
| 7 | No confirmation dialog | LOW | Direct write on button click |
| 8 | File ownership lost after mv | MEDIUM | `mv` preserves temp file ownership, not original /etc/hosts ownership |
| 9 | Stale `isAddHost` member | TRIVIAL | Unused declaration in header |
| 10 | Aliases not trimmed | TRIVIAL | `ui->txtAliases->text()` without `.trimmed()` |

---

## Recommended Fix Approaches (High-Level)

1. **Temp file:** Replace hardcoded path with `QTemporaryFile` (or eliminate temp file entirely using the `tee` stdin-pipe pattern from `apt_source_tool.cpp`)
2. **Error handling:** Check `writeFile()` return value; add a `sudoExecWithStatus()` or parse sudoExec return; show `QMessageBox` on failure
3. **Delete cleanup:** Either compact empty entries before save, or switch to `removeAt()` with full line number recalculation
4. **Input validation:** Add regex validators for IPv4/IPv6, hostname RFC compliance; use `QLineEdit::setValidator()` or manual validation in `on_btnSave_clicked()`
5. **Backup:** Create `/etc/hosts.nexis-backup` (via sudo cp) before overwriting
6. **Confirmation:** Add `QMessageBox::question()` before `on_btnSaveChanges_clicked()` proceeds
7. **File permissions:** After write, ensure `/etc/hosts` has correct ownership (`root:root` or `root:wheel`) and permissions (`0644`)
