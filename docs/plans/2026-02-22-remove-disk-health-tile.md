# Remove Disk Health Tile Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Remove the standalone Disk Health MetricTile from the Dashboard and enhance the existing Disk Tile health badges to include the numeric health percentage.

**Architecture:** The Disk Tile already displays per-drive health verdict badges via `setDriveHealth()`. We add a `healthPercent` parameter so the badge reads `"Apple SSD: Good (92%)"`. Then we remove all references to `mDiskHealthTile` from the dashboard. The tray alert system and Settings page toggle are unaffected — they don't depend on the tile.

**Tech Stack:** C++/Qt6, QSS

---

### Task 1: Enhance `DiskTile::setDriveHealth()` to accept health percentage

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/disk_tile.h:22`
- Modify: `shared/nexis/Pages/Dashboard/disk_tile.cpp:75-96`

**Step 1: Update the header signature**

In `disk_tile.h`, change the `setDriveHealth` declaration:

```cpp
// OLD
void setDriveHealth(const QString &driveName, const QString &status, bool healthy = true);

// NEW
void setDriveHealth(const QString &driveName, const QString &status, int healthPercent, bool healthy = true);
```

**Step 2: Update the implementation**

In `disk_tile.cpp`, update `setDriveHealth()` to format the status text with the percentage when available:

```cpp
void DiskTile::setDriveHealth(const QString &driveName, const QString &status, int healthPercent, bool healthy)
{
    auto *driveLabel = new QLabel(driveName + ": ", mHealthContainer);
    driveLabel->setObjectName("diskTileSubtitle");

    // Format: "Good (92%)" when percent available, "Good" otherwise
    QString statusText = status;
    if (healthPercent >= 0)
        statusText += QString(" (%1%)").arg(healthPercent);

    auto *statusLabel = new QLabel(statusText, mHealthContainer);

    QSettings *sv = AppManager::ins()->getStyleValues();
    QString healthColor = sv ? sv->value(healthy ? "@successColor" : "@destructiveColor").toString() : (healthy ? "#2ec27e" : "#c01c28");
    statusLabel->setStyleSheet(QString("color: %1; font-size: 9pt; font-weight: 600;").arg(healthColor));

    mHealthEntries.append({statusLabel, healthy});

    auto *pair = new QHBoxLayout();
    pair->setContentsMargins(0, 0, 0, 0);
    pair->setSpacing(2);
    pair->addWidget(driveLabel);
    pair->addWidget(statusLabel);
    mHealthLayout->addLayout(pair);

    mHealthContainer->show();
}
```

**Step 3: Build to confirm compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build fails — the call site in `dashboard_page.cpp:668` still uses the old 3-arg signature. That's expected; we fix it in Task 2.

**Step 4: Commit (partial — header + implementation only)**

Hold this commit — combine with Task 2 since the build is intentionally broken.

---

### Task 2: Update call site in `onDiskHealthUpdated()` and pass `healthPercent`

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:662-671`

**Step 1: Update the `setDriveHealth()` call**

In `onDiskHealthUpdated()`, change the existing call at line ~668 to pass `d.healthPercent`:

```cpp
// OLD (line 668)
mDiskTile->setDriveHealth(name, d.healthVerdict, good);

// NEW
mDiskTile->setDriveHealth(name, d.healthVerdict, d.healthPercent, good);
```

**Step 2: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: PASS — `[100%] Built target nexis`

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/disk_tile.h shared/nexis/Pages/Dashboard/disk_tile.cpp shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): add health percentage to disk tile badges"
```

---

### Task 3: Remove `mDiskHealthTile` from Dashboard

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h:78`
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:39,103-108,189-193,248-249,659-660`

**Step 1: Remove the member variable from the header**

In `dashboard_page.h`, delete line 78:

```cpp
// DELETE this line:
MetricTile *mDiskHealthTile;
```

**Step 2: Remove all references in `dashboard_page.cpp`**

There are 6 locations to clean up:

**(a) Constructor initializer list (line 39)** — delete:
```cpp
mDiskHealthTile(new MetricTile(tr("DISK HEALTH"), "@diskHealthColor", this)),
```

**(b) Grid placement (lines 103-108)** — delete the entire block:
```cpp
if (im->hasDiskHealth()) {
    mDiskHealthTile->setDisplayMode(MetricTile::Large);
    ui->bentoGrid->addWidget(mDiskHealthTile, row, col++);
} else {
    mDiskHealthTile->hide();
}
```

**(c) Signal connection (lines 189-193)** — keep the connection to `onDiskHealthUpdated` but remove the `hasDiskHealth()` guard since the slot still populates the disk tile badges and fires tray alerts. Change to:
```cpp
// Disk health data (populates disk tile badges + tray alerts)
connect(mRefresh, &DataRefreshService::diskHealthUpdated,
        this, &DashboardPage::onDiskHealthUpdated);
```
Note: remove the `if (im->hasDiskHealth())` wrapper — if no health data exists, the signal simply never fires, so the guard is unnecessary.

**(d) Drop shadow list (lines 248-249)** — delete:
```cpp
if (im->hasDiskHealth())
    widgets.append(mDiskHealthTile);
```

**(e) Health tile update in `onDiskHealthUpdated()` (lines 659-660)** — delete:
```cpp
mDiskHealthTile->setValue(displayPercent, QString("%1%").arg(displayPercent));
mDiskHealthTile->setSubtitle(worstVerdict);
```

**Step 3: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: PASS — `[100%] Built target nexis`

**Step 4: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "refactor(dashboard): remove standalone Disk Health tile"
```

---

### Task 4: Update FEATURE_REQUESTS.md and documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md` (add FR-47)
- Modify: `docs/APPLICATION_OVERVIEW.md` (update Dashboard section)
- Modify: `docs/ARCHITECTURE_REVIEW.md` (update tile inventory if applicable)

**Step 1: Add FR-47 to FEATURE_REQUESTS.md**

Append before the Notes section:
```markdown
- [x] **FR-47: Remove Disk Health tile, enhance Disk Tile health badges** — Removed the standalone Disk Health MetricTile from the Dashboard. Enhanced `DiskTile::setDriveHealth()` to display the numeric health percentage alongside the verdict (e.g., "Apple SSD: Good (92%)"). Tray alerts and Settings toggle unchanged. **Resolved:** Added `healthPercent` param to `setDriveHealth()`, removed `mDiskHealthTile` from grid/shadows/signal wiring.
```

**Step 2: Update `docs/APPLICATION_OVERVIEW.md`**

Find the Dashboard tile description and:
- Update the Disk tile description to mention the enhanced health badges with percentages
- Remove any mention of a standalone "Disk Health" tile

**Step 3: Update `docs/ARCHITECTURE_REVIEW.md`**

If the Disk Health tile is mentioned in the tile inventory or signal flow, remove/update those references.

**Step 4: Commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update for Disk Health tile removal (FR-47)"
```

---

### Task 5: Final verification and push

**Step 1: Clean rebuild**

Run: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Full build succeeds with no warnings related to disk health.

**Step 2: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass.

**Step 3: Push**

```bash
git push
```
