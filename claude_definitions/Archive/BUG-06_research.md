# BUG-06 Research: Slow Startup with Large /etc/hosts File

## Bug Summary

Entire hosts file is read and parsed into UI model at startup with no lazy loading or pagination. Systems with large hosts files (ad-blockers, Pi-hole exports with 10,000+ entries) experience UI freezing. Upstream: [oguzhaninan/Stacer#492](https://github.com/oguzhaninan/Stacer/issues/492).

## Files Investigated

| File | Role |
|---|---|
| `shared/nexis/Pages/Helpers/host_manage.h` | Header — HostManage class, HostItem struct |
| `shared/nexis/Pages/Helpers/host_manage.cpp` | Implementation — file read, parse, model population |
| `shared/nexis/Pages/Helpers/helpers_page.h/.cpp` | Parent page — instantiates HostManage eagerly |
| `shared/nexis/app.cpp:48` | App constructor — creates HelpersPage at startup |
| `shared/nexis-core/Utils/file_util.cpp` | FileUtil::readListFromFile — synchronous I/O |
| `shared/nexis/Pages/Services/services_page.cpp` | Reference — existing deferred loading pattern |

## Architecture & Data Flow

### Startup Chain (All Synchronous, Main Thread)

```
App::App()                          app.cpp:48
  → new HelpersPage(mSlidingStacked)
    → HelpersPage::HelpersPage()    helpers_page.cpp:9-17
      → new HostManage()
        → HostManage::HostManage()  host_manage.cpp:12-22
          → init()                  host_manage.cpp:24-61
            → FileUtil::readListFromFile("/etc/hosts")   // BLOCKING I/O
            → loadTableData()
              → loadHostItems()     // parse all lines with regex
              → appendRow() × N    // one per host entry
```

Every step is synchronous on the main thread. The Helpers page is never visited by most users, yet its full data load happens before the app window even appears.

### Data Structures

- **`mHostFileContent`** (`QStringList`): Raw file lines — preserved for edits/deletes. The full file is kept in memory to maintain line-number correspondence.
- **`mHostItemList`** (`QMap<int, HostItem>`): Parsed entries keyed by original line number. Used for bidirectional mapping between table rows and file lines.
- **`LineNumberRole`** (custom data role): Stored on column 0 of each row so context-menu edit/delete can map back to the correct file line.
- **`QSortFilterProxyModel`** with `setDynamicSortFilter(true)`: Re-sorts/re-filters on every model change, including each individual `appendRow()` call.

## Performance Bottlenecks

### 1. Eager Loading at App Startup (PRIMARY)

`app.cpp:48` creates `HelpersPage` which immediately creates `HostManage` which immediately reads and parses the file. The user hasn't navigated to this page and may never do so.

### 2. Per-Row appendRow() with Dynamic Sort Filter

`host_manage.cpp:95-98`:
```cpp
while (itemIterator.hasNext()) {
    itemIterator.next();
    mItemModel->appendRow(createRow(...));
}
```

Each `appendRow()` call triggers:
1. `QStandardItemModel::rowsInserted` signal
2. `QSortFilterProxyModel` re-evaluates the new row (dynamic sort filter is enabled)
3. `QTableView` updates its layout

For 10,000 entries, that's 10,000 signal/slot round trips and 10,000 proxy evaluations.

### 3. Per-Line Regex Parsing

`host_manage.cpp:72`:
```cpp
QStringList lineItems = line.trimmed().split(QRegularExpression("\\s+"));
```

A new `QRegularExpression` object is constructed on every iteration. `QRegularExpression` compiles to a PCRE pattern internally. For 10,000 lines, that's 10,000 regex compilations. Should be a static or member variable compiled once.

### 4. Full Reload After Every Edit/Delete

- `on_btnSave_clicked()` (line 166): calls `loadTableData()` — re-parses entire file, rebuilds entire model
- `on_btnSaveChanges_clicked()` (line 185): writes file, then calls `loadTableData()` — full reload
- Delete action (line 226): calls `loadTableData()` — full reload

Every single-row edit triggers a complete teardown and rebuild of the model.

### 5. Full File Rewrite on Save

`on_btnSaveChanges_clicked()` (line 182):
```cpp
FileUtil::writeFile("/tmp/nexis_etc_host_new_content", mHostFileContent.join("\n"));
CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"});
```

Writes all lines to a temp file then `sudo mv` to `/etc/hosts`. This is actually reasonable for a config file — atomic writes via temp+rename is the correct pattern. The issue is the `loadTableData()` call afterward which re-parses everything.

## Memory Impact (10,000 entries)

| Component | Estimate |
|---|---|
| `mHostFileContent` (QStringList, 10K lines) | ~1 MB |
| `mHostItemList` (QMap, 10K HostItem structs) | ~2 MB |
| QStandardItemModel (30K QStandardItem objects, 3 cols × 10K rows) | ~4 MB |
| QSortFilterProxyModel internal mappings | ~1 MB |
| **Total** | **~8 MB** |

Not a huge memory issue, but the synchronous load time is the problem.

## Existing Deferred Pattern (ServicesPage)

`services_page.cpp:24-25`:
```cpp
connect(this, &ServicesPage::loadServicesS, this, &ServicesPage::loadServices);
(void)QtConcurrent::run([this]() { getServices(); });
```

Worker thread does I/O, emits signal, main thread builds UI. This same pattern can be adapted for HostManage, though the main bottleneck (10K `appendRow()` calls) is on the main thread and needs batched insertion instead.

## Recommended Fixes

### Fix A: Defer loading until page is shown (CRITICAL)
Move the file read + model population out of the constructor. Only load when the user navigates to the Helpers page. Options:
1. Override `showEvent()` in HostManage with a `mLoaded` flag
2. Have HelpersPage call a `loadIfNeeded()` method on HostManage from `on_btnHostManage_clicked()`

### Fix B: Batch model insertion (HIGH)
Instead of calling `appendRow()` 10,000 times (each triggering signals + proxy re-evaluation):
1. Disable dynamic sort filter before insertion: `mSortFilterModel->setDynamicSortFilter(false)`
2. Use `mItemModel->insertRows()` + `mItemModel->setItem()` or build all rows first, then insert in one batch
3. Re-enable dynamic sort filter after

### Fix C: Pre-compile regex (LOW)
Move the `QRegularExpression("\\s+")` to a static local or member variable so it's compiled once instead of per-line.

### Fix D: Avoid full reload on single-row edits (MODERATE)
For add/edit/delete operations, update only the affected rows in the model instead of calling `loadTableData()` which tears down and rebuilds everything. The `LineNumberRole` mapping already tracks the correspondence.
