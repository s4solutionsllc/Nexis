# Dashboard Redesign Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Split the CPU/Memory hero tile into independent tiles and add a customizable drag-and-drop tile layout system with persistence.

**Architecture:** Enhanced QGridLayout approach — keep the existing `bentoGrid` as the layout engine, add custom mouse-event-driven drag-and-drop and resize handles in a toggle-able edit mode, persist tile positions as JSON in `settings.ini`.

**Tech Stack:** C++17, Qt 6 (QGridLayout, QWidget, QSettings, QJsonDocument, QMouseEvent, QPainter)

---

## Task 1: Add DashboardLayout Setting Key and SettingManager Methods

**Files:**
- Modify: `shared/nexis/Managers/setting_manager.h:7-34` (add key to namespace, add method declarations)
- Modify: `shared/nexis/Managers/setting_manager.cpp` (add method implementations, append at end)

**Step 1: Add the setting key**

In `setting_manager.h`, add to the `SettingKeys` namespace (after line 33):

```cpp
const QString DashboardLayout("DashboardLayout");
```

**Step 2: Add getter/setter/clear declarations**

In `setting_manager.h`, add to the public section (after line 119, before `private:`):

```cpp
void setDashboardLayout(const QString &json);
QString getDashboardLayout() const;
void clearDashboardLayout();
```

**Step 3: Implement the methods**

Append to `setting_manager.cpp` (after line 282):

```cpp
void SettingManager::setDashboardLayout(const QString &json)
{
    mSettings->setValue(SettingKeys::DashboardLayout, json);
}

QString SettingManager::getDashboardLayout() const
{
    return mSettings->value(SettingKeys::DashboardLayout, "").toString();
}

void SettingManager::clearDashboardLayout()
{
    mSettings->remove(SettingKeys::DashboardLayout);
}
```

**Step 4: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build, no errors.

**Step 5: Commit**

```bash
git add shared/nexis/Managers/setting_manager.h shared/nexis/Managers/setting_manager.cpp
git commit -m "feat(settings): add DashboardLayout persistence key and methods"
```

---

## Task 2: Add sigDashboardLayoutReset Signal to SignalMapper

**Files:**
- Modify: `shared/nexis/signal_mapper.h:13-25` (add signal)

**Step 1: Add the signal**

In `signal_mapper.h`, add after line 24 (`sigCleanableSizeChanged`):

```cpp
void sigDashboardLayoutReset();
```

**Step 2: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 3: Commit**

```bash
git add shared/nexis/signal_mapper.h
git commit -m "feat(signals): add sigDashboardLayoutReset to SignalMapper"
```

---

## Task 3: Remove HeroCard and Split CPU/Memory into Independent Tiles

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h:18,79` (remove HeroCard include and member)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:57-104,228-239` (replace HeroCard grid placement, update shadow list)
- Modify: `shared/nexis/static/themes/default/style/style.qss:669-693` (remove heroCard styles)
- Modify: `CMakeLists.txt:229,288` (remove hero_card.cpp and hero_card.h)
- Delete: `shared/nexis/Pages/Dashboard/hero_card.h`
- Delete: `shared/nexis/Pages/Dashboard/hero_card.cpp`

**Step 1: Remove HeroCard include and member from dashboard_page.h**

In `dashboard_page.h`, remove line 18:
```cpp
#include "hero_card.h"
```

Remove line 79:
```cpp
HeroCard *mHeroCard;
```

**Step 2: Update dashboard_page.cpp init() — replace hero card with individual tiles**

Replace lines 59-71 (the hero card grid placement block) with:

```cpp
    // Bento grid layout (default):
    //  Row 0: CPU | Memory | Disk | Network
    //  Row 1: GPU* | Temp* | Battery*
    // * = conditional tiles

    int row = 0;
    int col = 0;

    // Row 0: all four primary tiles
    ui->bentoGrid->addWidget(mCpuTile, 0, 0);
    ui->bentoGrid->addWidget(mMemTile, 0, 1);
    ui->bentoGrid->addWidget(mDiskTile, 0, 2);
    ui->bentoGrid->addWidget(mNetworkTile, 0, 3);
```

Remove lines 73-75 (the Hero display mode setting):
```cpp
    // Set Hero display mode for CPU and Memory tiles
    mCpuTile->setDisplayMode(MetricTile::Hero);
    mMemTile->setDisplayMode(MetricTile::Hero);
```

**Step 3: Update the drop shadow widget list**

Replace lines 229-231:
```cpp
    QList<QWidget*> widgets = {
        mHeroCard, mDiskTile, mNetworkTile
    };
```
With:
```cpp
    QList<QWidget*> widgets = {
        mCpuTile, mMemTile, mDiskTile, mNetworkTile
    };
```

**Step 4: Remove HeroCard QSS styles**

In `style.qss`, remove lines 669-693 (the entire `/* - Hero Card */` section including `#heroCard`, `#heroCard:hover`, `#heroCardLeft`, `#heroCardRight`, `#heroCardDivider`).

**Step 5: Remove HeroCard from CMakeLists.txt**

Remove these two lines from `CMakeLists.txt`:
- Line 229: `"${GUI_SHARED_DIR}/Pages/Dashboard/hero_card.cpp"`
- Line 288: `"${GUI_SHARED_DIR}/Pages/Dashboard/hero_card.h"`

**Step 6: Delete HeroCard source files**

```bash
rm shared/nexis/Pages/Dashboard/hero_card.h shared/nexis/Pages/Dashboard/hero_card.cpp
```

**Step 7: Clean rebuild to verify**

```bash
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)
```
Expected: Clean build, no errors.

**Step 8: Commit**

```bash
git add -A
git commit -m "refactor(dashboard): split CPU/Memory hero tile into independent tiles"
```

---

## Task 4: Create Edit Mode SVG Icons

**Files:**
- Create: `shared/nexis/static/themes/common/img/grid-edit.svg`
- Create: `shared/nexis/static/themes/common/img/grid-edit-done.svg`

**Step 1: Create the edit (pencil/grid) icon**

Create `shared/nexis/static/themes/common/img/grid-edit.svg`:
```svg
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="1" y="1" width="6" height="6" rx="1"/>
  <rect x="9" y="1" width="6" height="6" rx="1"/>
  <rect x="1" y="9" width="6" height="6" rx="1"/>
  <rect x="9" y="9" width="6" height="6" rx="1"/>
</svg>
```

**Step 2: Create the done (checkmark) icon**

Create `shared/nexis/static/themes/common/img/grid-edit-done.svg`:
```svg
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <polyline points="3 8.5 6.5 12 13 4"/>
</svg>
```

**Step 3: Register in Qt resource file**

Find the `.qrc` file that contains `fullscreen.svg` and add both new icons to the same `<qresource>` block.

**Step 4: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build, icons accessible via `:/static/themes/common/img/grid-edit.svg`.

**Step 5: Commit**

```bash
git add shared/nexis/static/themes/common/img/grid-edit.svg shared/nexis/static/themes/common/img/grid-edit-done.svg
git commit -m "feat(assets): add grid-edit and grid-edit-done SVG icons"
```

Note: Also add to the `.qrc` file if the resource system requires explicit registration.

---

## Task 5: Add Edit Mode Toggle Button and Toolbar to DashboardPage

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` (add members and slots)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (add edit mode UI setup, toggle logic, kiosk mutual exclusion)
- Modify: `shared/nexis/static/themes/default/style/style.qss` (add edit mode toolbar styles)

**Step 1: Add new members and slots to dashboard_page.h**

Add includes at top:
```cpp
#include <QShortcut>
```

Add to private slots (after `on_btnDownloadUpdate_clicked`):
```cpp
void toggleEditMode();
void exitEditMode();
void onResetLayout();
```

Add to private members (after `mKioskButton`):
```cpp
QPushButton *mEditButton;
QWidget *mEditToolbar;
QPushButton *mBtnResetLayout;
QPushButton *mBtnDone;
QShortcut *mEditShortcut;
bool mEditMode;
bool mKioskMode;
```

**Step 2: Initialize edit mode UI in dashboard_page.cpp constructor**

In the member initializer list (after `mKioskButton(new QPushButton(this))`), add:
```cpp
mEditButton(new QPushButton(this)),
mEditToolbar(nullptr),
mBtnResetLayout(nullptr),
mBtnDone(nullptr),
mEditShortcut(nullptr),
mEditMode(false),
mKioskMode(false)
```

**Step 3: Build edit mode button in init()**

After the kiosk button setup block (after line 264), add:

```cpp
    // Edit mode toggle button (floating, to the left of kiosk button)
    mEditButton->setFixedSize(32, 32);
    mEditButton->setIcon(QIcon(":/static/themes/common/img/grid-edit.svg"));
    mEditButton->setIconSize(QSize(16, 16));
    mEditButton->setToolTip(tr("Customize Layout (Ctrl+E)"));
    mEditButton->setCursor(Qt::PointingHandCursor);
    mEditButton->setFocusPolicy(Qt::NoFocus);
    mEditButton->setObjectName("btnEditToggle");
    mEditButton->raise();

    connect(mEditButton, &QPushButton::clicked, this, &DashboardPage::toggleEditMode);

    // Ctrl+E shortcut
    mEditShortcut = new QShortcut(QKeySequence("Ctrl+E"), this);
    connect(mEditShortcut, &QShortcut::activated, this, &DashboardPage::toggleEditMode);

    // Edit mode toolbar (hidden by default, shown above bentoGrid)
    mEditToolbar = new QWidget(this);
    mEditToolbar->setObjectName("editToolbar");
    mEditToolbar->setFixedHeight(40);
    mEditToolbar->hide();

    auto *toolbarLayout = new QHBoxLayout(mEditToolbar);
    toolbarLayout->setContentsMargins(12, 4, 12, 4);

    auto *lblCustomize = new QLabel(tr("Customize Layout"), mEditToolbar);
    lblCustomize->setObjectName("editToolbarLabel");
    toolbarLayout->addWidget(lblCustomize);
    toolbarLayout->addStretch();

    mBtnResetLayout = new QPushButton(tr("Reset Layout"), mEditToolbar);
    mBtnResetLayout->setObjectName("btnResetLayout");
    mBtnResetLayout->setCursor(Qt::PointingHandCursor);
    mBtnResetLayout->setFocusPolicy(Qt::NoFocus);
    toolbarLayout->addWidget(mBtnResetLayout);

    mBtnDone = new QPushButton(tr("Done"), mEditToolbar);
    mBtnDone->setObjectName("btnEditDone");
    mBtnDone->setCursor(Qt::PointingHandCursor);
    mBtnDone->setFocusPolicy(Qt::NoFocus);
    toolbarLayout->addWidget(mBtnDone);

    // Insert toolbar at the top of the main layout (before bentoGrid)
    ui->mainLayout->insertWidget(0, mEditToolbar);

    connect(mBtnDone, &QPushButton::clicked, this, &DashboardPage::exitEditMode);
    connect(mBtnResetLayout, &QPushButton::clicked, this, &DashboardPage::onResetLayout);
```

**Step 4: Implement toggle/exit edit mode**

```cpp
void DashboardPage::toggleEditMode()
{
    if (mKioskMode)
        return;

    if (mEditMode)
        exitEditMode();
    else {
        mEditMode = true;
        mEditToolbar->show();
        mKioskButton->hide();
        mEditButton->setIcon(QIcon(":/static/themes/common/img/grid-edit-done.svg"));
        mEditButton->setToolTip(tr("Finish Editing (Ctrl+E)"));
        // TODO: Task 7 will enable drag handles on tiles here
    }
}

void DashboardPage::exitEditMode()
{
    mEditMode = false;
    mEditToolbar->hide();
    mKioskButton->show();
    mEditButton->setIcon(QIcon(":/static/themes/common/img/grid-edit.svg"));
    mEditButton->setToolTip(tr("Customize Layout (Ctrl+E)"));
    // TODO: Task 8 will save layout here
    // TODO: Task 7 will disable drag handles on tiles here
}

void DashboardPage::onResetLayout()
{
    mSettingManager->clearDashboardLayout();
    // TODO: Task 8 will call rebuildLayout() here
    exitEditMode();
}
```

**Step 5: Update onKioskModeChanged to enforce mutual exclusion**

Replace the existing `onKioskModeChanged` method entirely:

```cpp
void DashboardPage::onKioskModeChanged(bool enabled)
{
    mKioskMode = enabled;
    if (enabled) {
        if (mEditMode)
            exitEditMode();
        mEditButton->hide();
        mEditShortcut->setEnabled(false);
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen-exit.svg"));
        mKioskButton->setToolTip(tr("Exit Kiosk Mode (ESC)"));
    } else {
        mEditButton->show();
        mEditShortcut->setEnabled(true);
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen.svg"));
        mKioskButton->setToolTip(tr("Enter Kiosk Mode (F11)"));
    }
}
```

**Step 6: Update resizeEvent to position both buttons**

Replace the existing `resizeEvent`:

```cpp
void DashboardPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mKioskButton->move(width() - mKioskButton->width() - 10, 10);
    mEditButton->move(width() - mKioskButton->width() - mEditButton->width() - 18, 10);
}
```

**Step 7: Add QSS styles for edit toolbar and buttons**

In `style.qss`, add after the existing kiosk button styles (or at the end of the Dashboard section):

```qss
/* - Edit Mode Toolbar - */

#editToolbar {
    background-color: @cardBg;
    border: 1px solid @borderColor;
    border-radius: 8;
}

#editToolbarLabel {
    color: @color05;
    font-size: 10pt;
    font-weight: 600;
}

#btnEditToggle {
    border: 0;
    background-color: transparent;
    border-radius: 6;
    padding: 4;
}

#btnEditToggle:hover {
    background-color: @color02;
}

#btnResetLayout {
    font-size: 9pt;
    padding: 4 12;
    border-radius: 6;
    border: 1px solid @borderColor;
    color: @color05;
    background-color: transparent;
}

#btnResetLayout:hover {
    background-color: @color02;
}

#btnEditDone {
    font-size: 9pt;
    padding: 4 12;
    border-radius: 6;
    border: 1px solid @accentColor;
    color: @color07;
    background-color: @accentColor;
}

#btnEditDone:hover {
    background-color: @accentColorHover;
}
```

**Step 8: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 9: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp shared/nexis/static/themes/default/style/style.qss
git commit -m "feat(dashboard): add edit mode toggle with toolbar and kiosk mutual exclusion"
```

---

## Task 6: Create DashboardTileWrapper for Drag-and-Drop and Resize

**Files:**
- Create: `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h`
- Create: `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp`
- Modify: `CMakeLists.txt` (add new files to build)

This wrapper widget wraps each tile (MetricTile, DiskTile, NetworkTile) and handles edit-mode mouse events for drag and resize.

**Step 1: Create dashboard_tile_wrapper.h**

```cpp
#ifndef DASHBOARD_TILE_WRAPPER_H
#define DASHBOARD_TILE_WRAPPER_H

#include <QWidget>
#include <QMouseEvent>
#include <QPaintEvent>

class DashboardTileWrapper : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardTileWrapper(const QString &tileId, QWidget *innerWidget, QWidget *parent = nullptr);

    QString tileId() const;
    QWidget *innerWidget() const;

    void setEditMode(bool enabled);
    bool isEditMode() const;

    int gridRow() const;
    int gridCol() const;
    int gridRowSpan() const;
    int gridColSpan() const;
    void setGridPosition(int row, int col, int rowSpan = 1, int colSpan = 1);

signals:
    void dragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void dragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void dragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void resizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString mTileId;
    QWidget *mInnerWidget;
    bool mEditMode;
    bool mDragging;
    bool mResizing;
    QPoint mDragStartPos;
    QPoint mDragStartGlobal;

    int mGridRow;
    int mGridCol;
    int mGridRowSpan;
    int mGridColSpan;

    static const int DRAG_THRESHOLD = 5;
    static const int RESIZE_HANDLE_SIZE = 16;

    bool isInResizeHandle(const QPoint &pos) const;
};

#endif // DASHBOARD_TILE_WRAPPER_H
```

**Step 2: Create dashboard_tile_wrapper.cpp**

```cpp
#include "dashboard_tile_wrapper.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QApplication>

DashboardTileWrapper::DashboardTileWrapper(const QString &tileId, QWidget *innerWidget, QWidget *parent)
    : QWidget(parent),
      mTileId(tileId),
      mInnerWidget(innerWidget),
      mEditMode(false),
      mDragging(false),
      mResizing(false),
      mGridRow(0),
      mGridCol(0),
      mGridRowSpan(1),
      mGridColSpan(1)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    innerWidget->setParent(this);
    layout->addWidget(innerWidget);
}

QString DashboardTileWrapper::tileId() const { return mTileId; }
QWidget *DashboardTileWrapper::innerWidget() const { return mInnerWidget; }

void DashboardTileWrapper::setEditMode(bool enabled)
{
    mEditMode = enabled;
    if (enabled)
        setCursor(Qt::OpenHandCursor);
    else
        unsetCursor();
    update();
}

bool DashboardTileWrapper::isEditMode() const { return mEditMode; }

int DashboardTileWrapper::gridRow() const { return mGridRow; }
int DashboardTileWrapper::gridCol() const { return mGridCol; }
int DashboardTileWrapper::gridRowSpan() const { return mGridRowSpan; }
int DashboardTileWrapper::gridColSpan() const { return mGridColSpan; }

void DashboardTileWrapper::setGridPosition(int row, int col, int rowSpan, int colSpan)
{
    mGridRow = row;
    mGridCol = col;
    mGridRowSpan = rowSpan;
    mGridColSpan = colSpan;
}

bool DashboardTileWrapper::isInResizeHandle(const QPoint &pos) const
{
    QRect handleRect(width() - RESIZE_HANDLE_SIZE, height() - RESIZE_HANDLE_SIZE,
                     RESIZE_HANDLE_SIZE, RESIZE_HANDLE_SIZE);
    return handleRect.contains(pos);
}

void DashboardTileWrapper::mousePressEvent(QMouseEvent *event)
{
    if (!mEditMode || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    mDragStartPos = event->pos();
    mDragStartGlobal = event->globalPosition().toPoint();

    if (isInResizeHandle(event->pos())) {
        mResizing = true;
        setCursor(Qt::SizeFDiagCursor);
    } else {
        setCursor(Qt::ClosedHandCursor);
    }
}

void DashboardTileWrapper::mouseMoveEvent(QMouseEvent *event)
{
    if (!mEditMode) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!(event->buttons() & Qt::LeftButton))
        return;

    QPoint delta = event->pos() - mDragStartPos;

    if (!mDragging && !mResizing && delta.manhattanLength() >= DRAG_THRESHOLD) {
        mDragging = true;
        emit dragStarted(this, event->globalPosition().toPoint());
    }

    if (mDragging)
        emit dragMoved(this, event->globalPosition().toPoint());
}

void DashboardTileWrapper::mouseReleaseEvent(QMouseEvent *event)
{
    if (!mEditMode || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (mDragging) {
        mDragging = false;
        emit dragFinished(this, event->globalPosition().toPoint());
    }

    if (mResizing) {
        mResizing = false;
    }

    setCursor(Qt::OpenHandCursor);
}

void DashboardTileWrapper::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (!mEditMode)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dashed border overlay
    QPen pen(QColor(150, 150, 150, 120), 2, Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

    // Resize grip triangle at bottom-right
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(150, 150, 150, 160));
    QPolygon triangle;
    int s = RESIZE_HANDLE_SIZE;
    triangle << QPoint(width(), height())
             << QPoint(width() - s, height())
             << QPoint(width(), height() - s);
    painter.drawPolygon(triangle);
}
```

**Step 3: Add to CMakeLists.txt**

Add to the source list (near line 230, after `disk_tile.cpp`):
```
"${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_tile_wrapper.cpp"
```

Add to the header list (near line 289, after `disk_tile.h`):
```
"${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_tile_wrapper.h"
```

**Step 4: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp CMakeLists.txt
git commit -m "feat(dashboard): create DashboardTileWrapper with drag/resize mouse handling"
```

---

## Task 7: Integrate Wrappers into DashboardPage and Wire Edit Mode

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` (add wrapper members, include, helper methods)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (wrap tiles, connect drag signals, build grid from wrappers)

**Step 1: Update dashboard_page.h**

Add include:
```cpp
#include "dashboard_tile_wrapper.h"
```

Add to private members:
```cpp
QList<DashboardTileWrapper*> mTileWrappers;
QWidget *mDragIndicator;
DashboardTileWrapper *mDragSource;
```

Add to private methods:
```cpp
void buildGrid();
void rebuildLayout();
DashboardTileWrapper *wrapTile(const QString &id, QWidget *tile);
void applyDisplayModeForSpan(DashboardTileWrapper *wrapper);
QJsonArray serializeLayout() const;
void deserializeLayout(const QString &json);
QJsonArray defaultLayout() const;
int gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const;
```

Add to private slots:
```cpp
void onTileDragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos);
void onTileDragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos);
void onTileDragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos);
```

**Step 2: Wrap tiles instead of adding them directly**

In `init()`, replace the direct `addWidget` calls for the bento grid with wrapping:

```cpp
    // Wrap each tile
    auto *cpuWrap = wrapTile("cpu", mCpuTile);
    auto *memWrap = wrapTile("memory", mMemTile);
    auto *diskWrap = wrapTile("disk", mDiskTile);
    auto *netWrap = wrapTile("network", mNetworkTile);

    // Conditional tiles
    DashboardTileWrapper *gpuWrap = nullptr;
    DashboardTileWrapper *tempWrap = nullptr;
    DashboardTileWrapper *batWrap = nullptr;

    if (im->hasGpu())
        gpuWrap = wrapTile("gpu", mGpuTile);
    else
        mGpuTile->hide();

    if (im->hasThermalSensors())
        tempWrap = wrapTile("temp", mTempTile);
    else
        mTempTile->hide();

    if (im->hasBattery())
        batWrap = wrapTile("battery", mBatteryTile);
    else
        mBatteryTile->hide();

    // Load saved layout or use default
    QString savedLayout = mSettingManager->getDashboardLayout();
    if (savedLayout.isEmpty())
        deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    else
        deserializeLayout(savedLayout);

    buildGrid();
```

**Step 3: Implement wrapTile**

```cpp
DashboardTileWrapper *DashboardPage::wrapTile(const QString &id, QWidget *tile)
{
    auto *wrapper = new DashboardTileWrapper(id, tile, this);

    connect(wrapper, &DashboardTileWrapper::dragStarted,
            this, &DashboardPage::onTileDragStarted);
    connect(wrapper, &DashboardTileWrapper::dragMoved,
            this, &DashboardPage::onTileDragMoved);
    connect(wrapper, &DashboardTileWrapper::dragFinished,
            this, &DashboardPage::onTileDragFinished);

    mTileWrappers.append(wrapper);
    return wrapper;
}
```

**Step 4: Implement defaultLayout**

```cpp
QJsonArray DashboardPage::defaultLayout() const
{
    QJsonArray arr;
    auto addEntry = [&](const QString &id, int row, int col, int rs, int cs) {
        QJsonObject obj;
        obj["id"] = id;
        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = rs;
        obj["colSpan"] = cs;
        arr.append(obj);
    };

    addEntry("cpu", 0, 0, 1, 1);
    addEntry("memory", 0, 1, 1, 1);
    addEntry("disk", 0, 2, 1, 1);
    addEntry("network", 0, 3, 1, 1);

    int col = 0;
    if (im->hasGpu()) addEntry("gpu", 1, col++, 1, 1);
    if (im->hasThermalSensors()) addEntry("temp", 1, col++, 1, 1);
    if (im->hasBattery()) addEntry("battery", 1, col++, 1, 1);

    return arr;
}
```

**Step 5: Implement deserializeLayout**

```cpp
void DashboardPage::deserializeLayout(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr = doc.array();

    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        QString id = obj["id"].toString();
        int row = obj["row"].toInt();
        int col = obj["col"].toInt();
        int rowSpan = obj["rowSpan"].toInt(1);
        int colSpan = obj["colSpan"].toInt(1);

        for (DashboardTileWrapper *w : mTileWrappers) {
            if (w->tileId() == id) {
                w->setGridPosition(row, col, rowSpan, colSpan);
                break;
            }
        }
    }
}
```

**Step 6: Implement serializeLayout**

```cpp
QJsonArray DashboardPage::serializeLayout() const
{
    QJsonArray arr;
    for (const DashboardTileWrapper *w : mTileWrappers) {
        QJsonObject obj;
        obj["id"] = w->tileId();
        obj["row"] = w->gridRow();
        obj["col"] = w->gridCol();
        obj["rowSpan"] = w->gridRowSpan();
        obj["colSpan"] = w->gridColSpan();
        arr.append(obj);
    }
    return arr;
}
```

**Step 7: Implement buildGrid**

```cpp
void DashboardPage::buildGrid()
{
    // Remove all existing widgets from bentoGrid
    while (ui->bentoGrid->count() > 0) {
        QLayoutItem *item = ui->bentoGrid->takeAt(0);
        // Don't delete the widget, just remove from layout
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }

    // Add wrappers to grid according to their stored positions
    for (DashboardTileWrapper *w : mTileWrappers) {
        w->setParent(this);
        ui->bentoGrid->addWidget(w, w->gridRow(), w->gridCol(),
                                  w->gridRowSpan(), w->gridColSpan());
        applyDisplayModeForSpan(w);
        w->show();
    }

    for (int c = 0; c < 4; ++c)
        ui->bentoGrid->setColumnStretch(c, 1);
}
```

**Step 8: Implement applyDisplayModeForSpan**

```cpp
void DashboardPage::applyDisplayModeForSpan(DashboardTileWrapper *wrapper)
{
    // Only MetricTile has display modes
    auto *metric = qobject_cast<MetricTile*>(wrapper->innerWidget());
    if (!metric)
        return;

    int area = wrapper->gridRowSpan() * wrapper->gridColSpan();
    if (area >= 4)
        metric->setDisplayMode(MetricTile::Hero);
    else if (area >= 2)
        metric->setDisplayMode(MetricTile::Large);
    else
        metric->setDisplayMode(MetricTile::Normal);
}
```

**Step 9: Implement rebuildLayout**

```cpp
void DashboardPage::rebuildLayout()
{
    QString saved = mSettingManager->getDashboardLayout();
    if (saved.isEmpty())
        deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    else
        deserializeLayout(saved);
    buildGrid();
}
```

**Step 10: Wire up toggleEditMode to enable/disable wrappers**

Update `toggleEditMode()` — replace the TODO comments:

```cpp
void DashboardPage::toggleEditMode()
{
    if (mKioskMode)
        return;

    if (mEditMode)
        exitEditMode();
    else {
        mEditMode = true;
        mEditToolbar->show();
        mKioskButton->hide();
        mEditButton->setIcon(QIcon(":/static/themes/common/img/grid-edit-done.svg"));
        mEditButton->setToolTip(tr("Finish Editing (Ctrl+E)"));
        for (DashboardTileWrapper *w : mTileWrappers)
            w->setEditMode(true);
    }
}
```

Update `exitEditMode()`:

```cpp
void DashboardPage::exitEditMode()
{
    mEditMode = false;
    mEditToolbar->hide();
    mKioskButton->show();
    mEditButton->setIcon(QIcon(":/static/themes/common/img/grid-edit.svg"));
    mEditButton->setToolTip(tr("Customize Layout (Ctrl+E)"));
    for (DashboardTileWrapper *w : mTileWrappers)
        w->setEditMode(false);
    // Save layout
    QJsonDocument doc(serializeLayout());
    mSettingManager->setDashboardLayout(QString(doc.toJson(QJsonDocument::Compact)));
}
```

Update `onResetLayout()`:

```cpp
void DashboardPage::onResetLayout()
{
    mSettingManager->clearDashboardLayout();
    deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    buildGrid();
}
```

**Step 11: Update drop shadow list to use wrappers**

Replace the shadow widget list to iterate wrappers:
```cpp
    QList<QWidget*> widgets;
    for (DashboardTileWrapper *w : mTileWrappers)
        widgets.append(w);
    Utilities::addDropShadow(widgets, 80);
```

**Step 12: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 13: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): integrate tile wrappers with layout serialization and edit mode"
```

---

## Task 8: Implement Drag-and-Drop Swap Logic

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` (mDragIndicator already added in Task 7)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (implement drag slots)

**Step 1: Create the drag indicator widget in init()**

After the edit toolbar setup:

```cpp
    mDragIndicator = new QWidget(this);
    mDragIndicator->setObjectName("dragIndicator");
    mDragIndicator->hide();
    mDragIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    mDragSource = nullptr;
```

**Step 2: Implement gridCellAtPos**

```cpp
int DashboardPage::gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const
{
    for (DashboardTileWrapper *w : mTileWrappers) {
        QRect tileRect = QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
        if (tileRect.contains(globalPos)) {
            outRow = w->gridRow();
            outCol = w->gridCol();
            return 1;
        }
    }
    return 0;
}
```

**Step 3: Implement drag started**

```cpp
void DashboardPage::onTileDragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    Q_UNUSED(globalPos)
    mDragSource = wrapper;
    wrapper->setWindowOpacity(0.5);
    wrapper->raise();
}
```

**Step 4: Implement drag moved**

```cpp
void DashboardPage::onTileDragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    Q_UNUSED(wrapper)

    int targetRow, targetCol;
    if (gridCellAtPos(globalPos, targetRow, targetCol)) {
        // Find the widget at the target position to show the indicator
        for (DashboardTileWrapper *w : mTileWrappers) {
            if (w->gridRow() == targetRow && w->gridCol() == targetCol && w != mDragSource) {
                QPoint local = w->mapToParent(QPoint(0, 0));
                mDragIndicator->setGeometry(local.x(), local.y(), w->width(), w->height());
                mDragIndicator->show();
                mDragIndicator->raise();
                return;
            }
        }
    }
    mDragIndicator->hide();
}
```

**Step 5: Implement drag finished (swap logic)**

```cpp
void DashboardPage::onTileDragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    wrapper->setWindowOpacity(1.0);
    mDragIndicator->hide();

    if (!mDragSource)
        return;

    int targetRow, targetCol;
    if (!gridCellAtPos(globalPos, targetRow, targetCol)) {
        mDragSource = nullptr;
        return;
    }

    // Find target wrapper
    DashboardTileWrapper *target = nullptr;
    for (DashboardTileWrapper *w : mTileWrappers) {
        if (w->gridRow() == targetRow && w->gridCol() == targetCol && w != mDragSource) {
            target = w;
            break;
        }
    }

    if (!target) {
        mDragSource = nullptr;
        return;
    }

    // Check if swap is valid (spans must fit)
    int srcRow = mDragSource->gridRow(), srcCol = mDragSource->gridCol();
    int srcRS = mDragSource->gridRowSpan(), srcCS = mDragSource->gridColSpan();
    int tgtRow = target->gridRow(), tgtCol = target->gridCol();
    int tgtRS = target->gridRowSpan(), tgtCS = target->gridColSpan();

    // Verify both tiles fit in each other's positions
    bool srcFitsAtTarget = (tgtCol + srcCS <= 4);
    bool tgtFitsAtSource = (srcCol + tgtCS <= 4);

    if (srcFitsAtTarget && tgtFitsAtSource) {
        mDragSource->setGridPosition(tgtRow, tgtCol, srcRS, srcCS);
        target->setGridPosition(srcRow, srcCol, tgtRS, tgtCS);
        buildGrid();
    }

    mDragSource = nullptr;
}
```

**Step 6: Add QSS for drag indicator**

In `style.qss`:
```qss
#dragIndicator {
    background-color: @accentColor;
    opacity: 0.2;
    border: 2px solid @accentColor;
    border-radius: 12;
}
```

**Step 7: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 8: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp shared/nexis/static/themes/default/style/style.qss
git commit -m "feat(dashboard): implement drag-and-drop tile swap logic with visual indicator"
```

---

## Task 9: Implement Snap-to-Grid Resize Logic

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h` (add resize signal refinement)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp` (implement resize drag calculation)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` (add resize slot)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (implement resize handler)

**Step 1: Update wrapper mouseMoveEvent to handle resize**

In `dashboard_tile_wrapper.cpp`, update `mouseMoveEvent` to differentiate resize from drag:

```cpp
void DashboardTileWrapper::mouseMoveEvent(QMouseEvent *event)
{
    if (!mEditMode) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!(event->buttons() & Qt::LeftButton))
        return;

    QPoint delta = event->pos() - mDragStartPos;

    if (mResizing) {
        // Calculate desired col/row span based on mouse position relative to grid cell size
        int cellWidth = width() / mGridColSpan;
        int cellHeight = height() / mGridRowSpan;
        if (cellWidth > 0 && cellHeight > 0) {
            int newColSpan = qBound(1, (event->pos().x() + cellWidth / 2) / cellWidth, 2);
            int newRowSpan = qBound(1, (event->pos().y() + cellHeight / 2) / cellHeight, 2);
            if (newColSpan != mGridColSpan || newRowSpan != mGridRowSpan)
                emit resizeRequested(this, newColSpan, newRowSpan);
        }
        return;
    }

    if (!mDragging && delta.manhattanLength() >= DRAG_THRESHOLD) {
        mDragging = true;
        emit dragStarted(this, event->globalPosition().toPoint());
    }

    if (mDragging)
        emit dragMoved(this, event->globalPosition().toPoint());
}
```

**Step 2: Add resize slot to dashboard_page.h**

In private slots:
```cpp
void onTileResizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan);
```

**Step 3: Connect resize signal in wrapTile**

In `wrapTile()`, add:
```cpp
    connect(wrapper, &DashboardTileWrapper::resizeRequested,
            this, &DashboardPage::onTileResizeRequested);
```

**Step 4: Implement resize handler**

```cpp
void DashboardPage::onTileResizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan)
{
    int row = wrapper->gridRow();
    int col = wrapper->gridCol();

    // Check bounds
    if (col + newColSpan > 4)
        return;

    // Check for collisions with other tiles
    for (DashboardTileWrapper *other : mTileWrappers) {
        if (other == wrapper)
            continue;

        // Check if any cell of the resized wrapper overlaps any cell of another tile
        for (int r = row; r < row + newRowSpan; ++r) {
            for (int c = col; c < col + newColSpan; ++c) {
                int oRow = other->gridRow(), oCol = other->gridCol();
                int oRS = other->gridRowSpan(), oCS = other->gridColSpan();
                if (r >= oRow && r < oRow + oRS && c >= oCol && c < oCol + oCS)
                    return; // Collision — reject resize
            }
        }
    }

    wrapper->setGridPosition(row, col, newRowSpan, newColSpan);
    buildGrid();
}
```

**Step 5: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): implement snap-to-grid tile resizing with collision detection"
```

---

## Task 10: Add Reset Dashboard Layout to Settings Page

**Files:**
- Modify: `shared/nexis/Pages/Settings/settings_page.h` (add slot)
- Modify: `shared/nexis/Pages/Settings/settings_page.cpp` (add Dashboard group with reset button)

**Step 1: Add slot to settings_page.h**

Add to private slots:
```cpp
void onResetDashboardLayout();
```

**Step 2: Add Dashboard group in settings_page.cpp init()**

Find where other `QGroupBox` sections are created and add after the last group (before any closing logic):

```cpp
    // Dashboard group
    {
        auto *grpDashboard = new QGroupBox(tr("Dashboard"), ui->scrollContent);
        grpDashboard->setObjectName("settingsGroup");
        auto *dashLayout = new QVBoxLayout(grpDashboard);

        auto *lblDesc = new QLabel(tr("Restore the default tile arrangement"), grpDashboard);
        lblDesc->setObjectName("settingsDescription");
        dashLayout->addWidget(lblDesc);

        auto *btnReset = new QPushButton(tr("Reset Dashboard Layout"), grpDashboard);
        btnReset->setObjectName("btnResetDashboardLayout");
        btnReset->setCursor(Qt::PointingHandCursor);
        btnReset->setFocusPolicy(Qt::NoFocus);
        btnReset->setEnabled(!mSettingManager->getDashboardLayout().isEmpty());
        dashLayout->addWidget(btnReset);

        connect(btnReset, &QPushButton::clicked, this, &SettingsPage::onResetDashboardLayout);

        ui->scrollContentLayout->addWidget(grpDashboard);
    }
```

**Step 3: Implement onResetDashboardLayout**

```cpp
void SettingsPage::onResetDashboardLayout()
{
    mSettingManager->clearDashboardLayout();
    emit SignalMapper::ins()->sigDashboardLayoutReset();

    // Disable the button since we're now at defaults
    auto *btn = findChild<QPushButton*>("btnResetDashboardLayout");
    if (btn)
        btn->setEnabled(false);
}
```

**Step 4: Connect sigDashboardLayoutReset in DashboardPage**

In `dashboard_page.cpp` `init()`, add:
```cpp
    connect(mSignalMapper, &SignalMapper::sigDashboardLayoutReset,
            this, &DashboardPage::rebuildLayout);
```

Note: `rebuildLayout` already calls `deserializeLayout(defaultLayout())` then `buildGrid()`.

**Step 5: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 6: Commit**

```bash
git add shared/nexis/Pages/Settings/settings_page.h shared/nexis/Pages/Settings/settings_page.cpp shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(settings): add Reset Dashboard Layout button in Settings page"
```

---

## Task 11: Update Tracking Files and Documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md` (add new FR entry)
- Modify: `docs/APPLICATION_OVERVIEW.md` (update dashboard section)
- Modify: `docs/ARCHITECTURE_REVIEW.md` (note new patterns)

**Step 1: Add feature request entries to FEATURE_REQUESTS.md**

Add two new entries:
- `FR-XX: Split CPU/Memory hero tile into independent tiles` → `[x]`
- `FR-YY: Customizable drag-and-drop dashboard tile layout with persistence` → `[x]`

(Use the next sequential FR IDs.)

**Step 2: Update APPLICATION_OVERVIEW.md**

Update the Dashboard page section to reflect:
- CPU and Memory are now independent tiles (no hero card)
- Edit mode for customizing tile layout
- Drag-and-drop reordering with grid-snap resizing
- Layout persistence across sessions
- Reset layout option in edit toolbar and Settings

**Step 3: Update ARCHITECTURE_REVIEW.md**

Note:
- New `DashboardTileWrapper` pattern for edit-mode mouse handling
- New signal: `sigDashboardLayoutReset`
- JSON layout persistence via `SettingManager`
- `HeroCard` removed (simplification)

**Step 4: Commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update tracking files and documentation for dashboard redesign"
```

---

## Task 12: Archive Research and Plan Files

**Files:**
- Move any `claude_definitions/` files for this feature to `claude_definitions/Archive/`

**Step 1: Move files**

```bash
mkdir -p claude_definitions/Archive
mv claude_definitions/FR-*_research.md claude_definitions/Archive/ 2>/dev/null || true
mv claude_definitions/FR-*_plan.md claude_definitions/Archive/ 2>/dev/null || true
```

(Adjust based on actual FR IDs used.)

**Step 2: Commit**

```bash
git add claude_definitions/
git commit -m "chore: archive dashboard redesign research/plan files"
```
