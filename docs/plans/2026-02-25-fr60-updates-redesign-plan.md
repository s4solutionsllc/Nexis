# FR-60: System Update Status — Redesign Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the dashboard gauge tile with a sidebar badge on the Homebrew/APT button and an "Available Updates" section on the APT Source Manager page.

**Architecture:** Three surgical changes — (1) strip dashboard tile code, (2) add sidebar badge following the existing cleaner badge pattern, (3) add updates section to APTSourceManagerPage. Core infrastructure (UpdateInfo, DataRefreshService, SettingManager) stays intact.

**Tech Stack:** C++/Qt6, QSS theming, QtConcurrent

---

### Task 1: Remove Updates Tile from Dashboard Header

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h:73` (slot declaration)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h:101` (member variable)

**Step 1: Remove onSystemUpdatesChecked slot declaration**

In `dashboard_page.h`, delete line 73:
```cpp
    void onSystemUpdatesChecked(const UpdateCheckResult &result);
```

**Step 2: Remove mUpdatesTile member variable**

In `dashboard_page.h`, delete line 101:
```cpp
    MetricTileBase *mUpdatesTile;
```

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h
git commit -m "refactor(dashboard): remove updates tile declarations from header (FR-60)"
```

---

### Task 2: Remove Updates Tile from Dashboard Implementation

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:40` (initializer)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:95` (tile creation)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:123-126` (wrapTile)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:296-306` (signal setup)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:964-1016` (slot impl)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:1098` (onResetLayout)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:1193` (defaultLayout)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:1517` (tileTitle)

**Step 1: Remove mUpdatesTile from constructor initializer list**

Delete line 40:
```cpp
    mUpdatesTile(nullptr),
```

**Step 2: Remove updates tile creation**

Delete line 95:
```cpp
    mUpdatesTile = createTile("updates", mTileStyles.value("updates", defaultStyle("updates")));
```

**Step 3: Remove updates wrapTile block**

Delete lines 123-126:
```cpp
    if (im->hasUpdateSources())
        wrapTile("updates", mUpdatesTile);
    else
        mUpdatesTile->hide();
```

**Step 4: Remove updates tile signal setup block**

Delete lines 296-306 (the comment and entire `if (im->hasUpdateSources())` block):
```cpp
    // System updates tile
    if (im->hasUpdateSources()) {
        mUpdatesTile->setSubtitle(tr("Checking..."));
        mUpdatesTile->setValue(0, tr("..."));
        mUpdatesTile->setQuickAction(tr("Check Now"), [this]() {
            mUpdatesTile->setSubtitle(tr("Checking..."));
            mRefresh->triggerUpdateCheck();
        });
        connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
                this, &DashboardPage::onSystemUpdatesChecked);
    }
```

**Step 5: Remove onSystemUpdatesChecked slot implementation**

Delete lines 964-1016 (the entire method):
```cpp
void DashboardPage::onSystemUpdatesChecked(const UpdateCheckResult &result)
{
    ...
    mSettingManager->setUpdateLastCount(count);
}
```

**Step 6: Remove "updates" from onResetLayout**

In `onResetLayout()` around line 1098, delete:
```cpp
            else if (id == "updates") mUpdatesTile = newTile;
```

**Step 7: Remove "updates" from defaultLayout**

In `defaultLayout()` around line 1193, delete:
```cpp
    if (im->hasUpdateSources()) addEntry("updates", 1, col++, 1, 1);
```

**Step 8: Remove "updates" from tileTitle**

In `tileTitle()` around line 1517, delete:
```cpp
    else if (id == "updates") { title = tr("UPDATES"); colorToken = "@updatesColor"; }
```

**Step 9: Build and verify**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -20
```
Expected: Clean build with no errors.

**Step 10: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "refactor(dashboard): strip all updates tile code from implementation (FR-60)"
```

---

### Task 3: Add Sidebar Badge Members and QSS

**Files:**
- Modify: `shared/nexis/app.h:117-118` (add new members after mCleanerBadgeDot)
- Modify: `shared/nexis/static/themes/default/style/style.qss:574` (add QSS rules)

**Step 1: Add member declarations in app.h**

After line 118 (`QLabel *mCleanerBadgeDot;`), add:
```cpp
    QLabel *mUpdatesBadge;
    QLabel *mUpdatesBadgeDot;
```

**Step 2: Add QSS rules for updates badge**

In `shared/nexis/static/themes/default/style/style.qss`, after line 574 (end of `#sidebarBadgeDot` block), add:
```css

#updatesBadge {
    background-color: @updatesColor;
    color: #ffffff;
    font-size: 7pt;
    font-weight: 700;
    border-radius: 8;
    padding: 1 6;
    min-width: 16;
    max-height: 16;
}

#updatesBadgeDot {
    background-color: @updatesColor;
    border-radius: 4;
    min-width: 8;
    max-width: 8;
    min-height: 8;
    max-height: 8;
}
```

**Step 3: Build and verify**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -20
```

**Step 4: Commit**

```bash
git add shared/nexis/app.h shared/nexis/static/themes/default/style/style.qss
git commit -m "feat(sidebar): add updates badge member declarations and QSS (FR-60)"
```

---

### Task 4: Create Sidebar Badge in buildSidebar() and Wire Signal in init()

**Files:**
- Modify: `shared/nexis/app.cpp:218-228` (buildSidebar — add badge after cleaner badge)
- Modify: `shared/nexis/app.cpp:297-304` (init — add signal connection after APT page creation)
- Modify: `shared/nexis/app.cpp:695-699` (applySidebarCollapse — add badge toggle)

**Step 1: Create badge labels in buildSidebar()**

After line 228 (`mCleanerBadgeDot->hide();`), add:
```cpp

    // Updates badge overlay on Homebrew/APT button
    mUpdatesBadge = new QLabel(ui->sidebar);
    mUpdatesBadge->setObjectName("updatesBadge");
    mUpdatesBadge->setAlignment(Qt::AlignCenter);
    mUpdatesBadge->setFixedSize(32, 16);
    mUpdatesBadge->hide();

    mUpdatesBadgeDot = new QLabel(ui->sidebar);
    mUpdatesBadgeDot->setObjectName("updatesBadgeDot");
    mUpdatesBadgeDot->setFixedSize(8, 8);
    mUpdatesBadgeDot->hide();
```

**Step 2: Wire DataRefreshService signal in init()**

After the APT Source Manager block (around line 304, after `btnAptSourceManager->hide();` and its closing `}`), add:
```cpp

    // Updates badge on Homebrew/APT sidebar button
    connect(DataRefreshService::ins(), &DataRefreshService::systemUpdatesChecked,
            this, [this](const UpdateCheckResult &result) {
        int count = result.success ? result.totalCount : 0;
        if (count > 0) {
            mUpdatesBadge->setText(QString::number(count));
            if (!mSidebarCollapsed) {
                mUpdatesBadge->show();
                mUpdatesBadgeDot->hide();
            } else {
                mUpdatesBadge->hide();
                mUpdatesBadgeDot->show();
            }
            QPoint btnPos = btnAptSourceManager->pos();
            int btnW = btnAptSourceManager->width();
            mUpdatesBadge->move(btnPos.x() + btnW - mUpdatesBadge->width() - 8,
                                btnPos.y() + 2);
            mUpdatesBadgeDot->move(btnPos.x() + btnW - 16,
                                   btnPos.y() + 4);
        } else {
            mUpdatesBadge->hide();
            mUpdatesBadgeDot->hide();
        }

        // Tray alert when updates go from 0 to >0
        int lastCount = SettingManager::ins()->getUpdateLastCount();
        if (SettingManager::ins()->getUpdateAlertEnabled() && count > 0 && lastCount == 0) {
            mTrayIcon->showMessage(
                tr("System Updates Available"),
                tr("%1 %2 available").arg(count).arg(count == 1 ? tr("update") : tr("updates")),
                QSystemTrayIcon::Information);
        }
        SettingManager::ins()->setUpdateLastCount(count);
    });
```

**Step 3: Add badge toggle in applySidebarCollapse()**

After line 699 (end of the `if (mCleanerBadge ...)` block), add:
```cpp

    // Toggle updates badge vs dot
    if (mUpdatesBadge && mUpdatesBadge->text().length() > 0) {
        mUpdatesBadge->setVisible(!collapsed);
        mUpdatesBadgeDot->setVisible(collapsed);
    }
```

**Step 4: Add `#include "update_info.h"` to app.cpp if not already present**

Check if `update_info.h` is included (needed for `UpdateCheckResult`). If not, add near top of app.cpp:
```cpp
#include "Info/update_info.h"
```

**Step 5: Build and verify**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -20
```

**Step 6: Commit**

```bash
git add shared/nexis/app.cpp
git commit -m "feat(sidebar): create updates badge and wire DataRefreshService signal (FR-60)"
```

---

### Task 5: Add Updates Section to APTSourceManagerPage Header

**Files:**
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h`

**Step 1: Add forward declaration and includes**

At top of file, add the include for DataRefreshService (after line 9, the existing includes):
```cpp
class DataRefreshService;
```
Note: DataRefreshService is already forward-declared style. Also add the `UpdateCheckResult` struct:
```cpp
#include "Info/update_info.h"
```

**Step 2: Add DataRefreshService parameter to constructor**

Change the constructor declaration (line 31) from:
```cpp
    explicit APTSourceManagerPage(QWidget *parent = nullptr,
                                  ToolManager *toolManager = nullptr,
                                  SignalMapper *signalMapper = nullptr);
```
to:
```cpp
    explicit APTSourceManagerPage(QWidget *parent = nullptr,
                                  ToolManager *toolManager = nullptr,
                                  SignalMapper *signalMapper = nullptr,
                                  DataRefreshService *refreshService = nullptr);
```

**Step 3: Add slot declaration**

After the existing private slots (before `private:` on line 63), add:
```cpp
    void onSystemUpdatesChecked(const UpdateCheckResult &result);
```

**Step 4: Add member variables**

After the `#ifdef Q_OS_MAC` members block (before the closing `};` of the class), add:
```cpp

    // Available Updates section
    QWidget *mUpdatesSection = nullptr;
    QLabel *mLblUpdatesTitle = nullptr;
    QPushButton *mBtnCheckNow = nullptr;
    QTreeWidget *mUpdatesTree = nullptr;
    DataRefreshService *mRefresh = nullptr;
```

**Step 5: Build and verify**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -20
```

**Step 6: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h
git commit -m "feat(apt-page): add updates section declarations to header (FR-60)"
```

---

### Task 6: Implement Updates Section in APTSourceManagerPage

**Files:**
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`

**Step 1: Add includes**

At top, add:
```cpp
#include "Managers/data_refresh_service.h"
#include "Info/update_info.h"
```

**Step 2: Update constructor to accept DataRefreshService**

Change the constructor (line 24) to accept and store `refreshService`:
```cpp
APTSourceManagerPage::APTSourceManagerPage(QWidget *parent, ToolManager *toolManager, SignalMapper *signalMapper, DataRefreshService *refreshService) :
    QWidget(parent),
    ui(new Ui::APTSourceManagerPage),
    mToolManager(toolManager ? toolManager : ToolManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mRefresh(refreshService ? refreshService : DataRefreshService::ins())
```

**Step 3: Build the updates section in init()**

At the very beginning of `init()` (line 35, before the `#ifdef Q_OS_MAC`), insert:
```cpp
    // --- Available Updates section ---
    mUpdatesSection = new QWidget(this);
    mUpdatesSection->setObjectName("updatesSection");
    mUpdatesSection->hide();

    QVBoxLayout *updLayout = new QVBoxLayout(mUpdatesSection);
    updLayout->setContentsMargins(0, 0, 0, 10);
    updLayout->setSpacing(5);

    // Header row: title + check now button
    QHBoxLayout *updHeader = new QHBoxLayout();
    mLblUpdatesTitle = new QLabel(tr("Available Updates"), mUpdatesSection);
    mLblUpdatesTitle->setObjectName("lblUpdatesTitle");
    QFont updFont = mLblUpdatesTitle->font();
    updFont.setPointSize(11);
    mLblUpdatesTitle->setFont(updFont);
    updHeader->addWidget(mLblUpdatesTitle);

    updHeader->addStretch();

    mBtnCheckNow = new QPushButton(tr("Check Now"), mUpdatesSection);
    mBtnCheckNow->setObjectName("btnCheckNow");
    mBtnCheckNow->setCursor(Qt::PointingHandCursor);
    mBtnCheckNow->setFocusPolicy(Qt::NoFocus);
    mBtnCheckNow->setAccessibleName("primary");
    mBtnCheckNow->setFixedHeight(28);
    updHeader->addWidget(mBtnCheckNow);
    updLayout->addLayout(updHeader);

    // Updates tree widget: Source | Package | Version
    mUpdatesTree = new QTreeWidget(mUpdatesSection);
    mUpdatesTree->setObjectName("treeWidgetUpdates");
    mUpdatesTree->setHeaderLabels({ tr("Source"), tr("Package"), tr("Version") });
    mUpdatesTree->setColumnCount(3);
    mUpdatesTree->setRootIsDecorated(false);
    mUpdatesTree->setFocusPolicy(Qt::NoFocus);
    mUpdatesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mUpdatesTree->setSelectionMode(QAbstractItemView::NoSelection);
    mUpdatesTree->header()->setStretchLastSection(true);
    mUpdatesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mUpdatesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mUpdatesTree->setMaximumHeight(200);
    updLayout->addWidget(mUpdatesTree);

    // Insert updates section into page layout BEFORE the main content
    // The page's root layout is verticalLayout_2, and verticalWidget is its first item
    ui->verticalLayout_2->insertWidget(0, mUpdatesSection);

    // Wire signal and button
    connect(mBtnCheckNow, &QPushButton::clicked, this, [this]() {
        mBtnCheckNow->setEnabled(false);
        mBtnCheckNow->setText(tr("Checking..."));
        mRefresh->triggerUpdateCheck();
    });
    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, &APTSourceManagerPage::onSystemUpdatesChecked);
```

**Step 4: Implement onSystemUpdatesChecked slot**

Add at the end of the file (before the final closing, after `on_btnEditAptSource_clicked()`):
```cpp
void APTSourceManagerPage::onSystemUpdatesChecked(const UpdateCheckResult &result)
{
    mBtnCheckNow->setEnabled(true);
    mBtnCheckNow->setText(tr("Check Now"));

    if (!result.success || result.totalCount == 0) {
        mUpdatesSection->hide();
        return;
    }

    mLblUpdatesTitle->setText(tr("Available Updates (%1)").arg(result.totalCount));
    mUpdatesTree->clear();

    for (const UpdateEntry &entry : result.entries) {
        QTreeWidgetItem *item = new QTreeWidgetItem(mUpdatesTree);
        item->setText(0, entry.source);
        item->setText(1, entry.name);
        item->setText(2, entry.version);
    }

    mUpdatesSection->show();
}
```

**Step 5: Add necessary includes for QVBoxLayout, QHBoxLayout, QTreeWidget, QHeaderView**

At top of file, ensure these includes exist (add any missing):
```cpp
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
```

**Step 6: Build and verify**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -20
```

**Step 7: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp
git commit -m "feat(apt-page): implement Available Updates section with tree view (FR-60)"
```

---

### Task 7: Run Tests and Final Verification

**Files:**
- None (verification only)

**Step 1: Run full test suite**

```bash
ctest --test-dir build --output-on-failure
```
Expected: 6/7 passing (ScreenshotTests is a pre-existing failure).

**Step 2: Launch app and verify visually (manual)**

- Sidebar badge appears on Homebrew/APT button when updates exist
- Badge toggles between text (expanded) and dot (collapsed)
- Navigating to Homebrew page shows "Available Updates" section at top
- "Check Now" button triggers a refresh
- Section hides when no updates are available

**Step 3: Commit if any fixes were needed**

---

### Task 8: Update Tracking Files and Documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md` (update FR-60 status)
- Modify: `docs/APPLICATION_OVERVIEW.md` (update dashboard and APT page sections)
- Modify: `docs/ARCHITECTURE_REVIEW.md` (note signal flow change)

**Step 1: Update FR-60 in FEATURE_REQUESTS.md**

Change FR-60 from `[~]` to `[x]` and add resolution note.

**Step 2: Update APPLICATION_OVERVIEW.md**

- Remove mention of updates tile from Dashboard section
- Add "Available Updates" section to the APT Source Manager / Homebrew page description
- Note the sidebar badge indicator

**Step 3: Update ARCHITECTURE_REVIEW.md**

- Note that `DataRefreshService::systemUpdatesChecked` signal is consumed by both `App` (badge) and `APTSourceManagerPage` (list), not by Dashboard

**Step 4: Commit and push**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update tracking and docs for FR-60 updates redesign"
git push
```
