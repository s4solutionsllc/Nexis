# FR-71 System Log Viewer & FR-73 Health Score Tile — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a System Log Viewer page (FR-71) to the sidebar and a Health Score tile (FR-73) to the Dashboard.

**Architecture:** FR-71 uses a `QProcess`-based log provider abstraction (`LogProvider` base → `LogProviderLinux`/`LogProviderMacOS`) feeding a `QStandardItemModel` displayed in a `QTableView` with proxy filtering. FR-73 adds a `HealthScoreTile` (inheriting `MetricTileBase`) with a `HealthScoreCalculator` helper that aggregates existing `DataRefreshService` signals into a weighted 0–100 composite score.

**Tech Stack:** C++17, Qt6, QProcess, QStandardItemModel, QSortFilterProxyModel, QPainter (for breakdown bars)

---

## Task 1: FR-73 — HealthScoreCalculator helper class

**Files:**
- Create: `shared/nexis/Pages/Dashboard/health_score_calculator.h`
- Create: `shared/nexis/Pages/Dashboard/health_score_calculator.cpp`

**Step 1: Create `health_score_calculator.h`**

```cpp
#ifndef HEALTH_SCORE_CALCULATOR_H
#define HEALTH_SCORE_CALCULATOR_H

#include <QList>
#include <QPair>
#include <QString>

struct HealthComponent {
    QString id;       // "cpu", "memory", "disk", "temp", "battery", "smart"
    QString label;    // "CPU", "MEM", "DSK", "TMP", "BAT", "HDD"
    int score;        // 0–100
    double weight;    // default weight (before redistribution)
    bool available;   // false if hardware not present
};

class HealthScoreCalculator
{
public:
    HealthScoreCalculator();

    void setCpuScore(int score);
    void setMemoryScore(int score);
    void setDiskScore(int score);
    void setTempScore(int score);
    void setBatteryScore(int score);
    void setSmartScore(int score);

    void setComponentAvailable(const QString &id, bool available);

    int compositeScore() const;
    QString scoreLabel() const;          // "Excellent", "Good", "Fair", "Poor"
    QList<HealthComponent> components() const;

private:
    QList<HealthComponent> mComponents;
    int indexOfComponent(const QString &id) const;
};

#endif // HEALTH_SCORE_CALCULATOR_H
```

**Step 2: Create `health_score_calculator.cpp`**

```cpp
#include "health_score_calculator.h"
#include <QtMath>

HealthScoreCalculator::HealthScoreCalculator()
{
    mComponents = {
        {"cpu",     "CPU", 100, 0.15, true},
        {"memory",  "MEM", 100, 0.20, true},
        {"disk",    "DSK", 100, 0.25, true},
        {"temp",    "TMP", 100, 0.15, false},
        {"battery", "BAT", 100, 0.10, false},
        {"smart",   "HDD", 100, 0.15, false}
    };
}

int HealthScoreCalculator::indexOfComponent(const QString &id) const
{
    for (int i = 0; i < mComponents.size(); ++i)
        if (mComponents[i].id == id)
            return i;
    return -1;
}

void HealthScoreCalculator::setCpuScore(int score)
{
    int idx = indexOfComponent("cpu");
    if (idx >= 0) mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setMemoryScore(int score)
{
    int idx = indexOfComponent("memory");
    if (idx >= 0) mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setDiskScore(int score)
{
    int idx = indexOfComponent("disk");
    if (idx >= 0) mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setTempScore(int score)
{
    int idx = indexOfComponent("temp");
    if (idx >= 0) mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setBatteryScore(int score)
{
    int idx = indexOfComponent("battery");
    if (idx >= 0) mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setSmartScore(int score)
{
    int idx = indexOfComponent("smart");
    if (idx >= 0) mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setComponentAvailable(const QString &id, bool available)
{
    int idx = indexOfComponent(id);
    if (idx >= 0) mComponents[idx].available = available;
}

int HealthScoreCalculator::compositeScore() const
{
    double totalWeight = 0;
    double weightedSum = 0;

    for (const auto &c : mComponents) {
        if (!c.available) continue;
        totalWeight += c.weight;
        weightedSum += c.weight * c.score;
    }

    if (totalWeight < 0.001) return 100;
    return qRound(weightedSum / totalWeight);
}

QString HealthScoreCalculator::scoreLabel() const
{
    int score = compositeScore();
    if (score >= 80) return QStringLiteral("Excellent");
    if (score >= 65) return QStringLiteral("Good");
    if (score >= 50) return QStringLiteral("Fair");
    return QStringLiteral("Poor");
}

QList<HealthComponent> HealthScoreCalculator::components() const
{
    QList<HealthComponent> result;
    for (const auto &c : mComponents)
        if (c.available) result.append(c);
    return result;
}
```

**Step 3: Add to CMakeLists.txt**

Add to `GUI_SHARED_SRCS` (after `vumeter_tile.cpp` line ~247):
```cmake
"${GUI_SHARED_DIR}/Pages/Dashboard/health_score_calculator.cpp"
```

Add to `GUI_SHARED_HDRS` (after `vumeter_tile.h` line ~314):
```cmake
"${GUI_SHARED_DIR}/Pages/Dashboard/health_score_calculator.h"
```

**Step 4: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: 100% built, no errors

**Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/health_score_calculator.{h,cpp} CMakeLists.txt
git commit -m "feat(dashboard): add HealthScoreCalculator helper class (FR-73)"
```

---

## Task 2: FR-73 — HealthScoreTile widget

**Files:**
- Create: `shared/nexis/Pages/Dashboard/health_score_tile.h`
- Create: `shared/nexis/Pages/Dashboard/health_score_tile.cpp`
- Modify: `CMakeLists.txt` — add new source/header entries

**Step 1: Create `health_score_tile.h`**

```cpp
#ifndef HEALTH_SCORE_TILE_H
#define HEALTH_SCORE_TILE_H

#include "metric_tile_base.h"
#include "health_score_calculator.h"

#include <QLabel>
#include <QToolButton>

class HealthScoreTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit HealthScoreTile(const QString &colorToken, QWidget *parent = nullptr);

    void setValue(int percent, const QString &valueText) override;
    void addDataPoint(double value) override;
    void setSubtitle(const QString &text) override;
    void setTrendDirection(TrendDirection dir) override;
    void setSecondaryValue(const QString &text) override;
    void setDisplayMode(DisplayMode mode) override;
    void setQuickAction(const QString &text, std::function<void()> callback) override;
    QToolButton *gearButton() override;
    void setGearVisible(bool visible) override;
    void refreshThemeColors() override;

    HealthScoreCalculator *calculator() { return &mCalculator; }
    void recalculate();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    void updateScoreDisplay();
    void paintBreakdownBars(QPainter &painter);

    HealthScoreCalculator mCalculator;

    QLabel *mLblTitle;
    QLabel *mLblScore;
    QLabel *mLblScoreLabel;
    QToolButton *mGearButton;

    int mCurrentScore;
    QString mCurrentLabel;
};

#endif // HEALTH_SCORE_TILE_H
```

**Step 2: Create `health_score_tile.cpp`**

The implementation should:
- `buildLayout()`: vertical layout with title, large score number, score label. Margins `12, 10, 12, 8`, spacing 2.
- `setValue()`: stores percent and value text, calls `update()`.
- `recalculate()`: reads `mCalculator.compositeScore()` and `scoreLabel()`, resolves color from theme tokens (`@successColor`/`@warningColor`/`@destructiveColor`) based on score range 80+/60+/below, calls `setValue()` and `update()`.
- `paintEvent()`: in `Large`/`Hero` display mode, call `paintBreakdownBars()` which iterates `mCalculator.components()` and draws horizontal bars below the score label. Each bar: 3-letter label, filled rect proportional to score, score number. Use `resolvedColor()` for fill. Bar height ~14px, spacing 2px.
- `refreshThemeColors()`: re-resolve the score-range color and repaint.
- `gearButton()`: return `mGearButton` (hidden by default — no gear menu needed for health).

**Step 3: Add to CMakeLists.txt**

Add to `GUI_SHARED_SRCS`:
```cmake
"${GUI_SHARED_DIR}/Pages/Dashboard/health_score_tile.cpp"
```

Add to `GUI_SHARED_HDRS`:
```cmake
"${GUI_SHARED_DIR}/Pages/Dashboard/health_score_tile.h"
```

**Step 4: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: 100% built, no errors

**Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/health_score_tile.{h,cpp} CMakeLists.txt
git commit -m "feat(dashboard): add HealthScoreTile widget (FR-73)"
```

---

## Task 3: FR-73 — Wire health tile into Dashboard

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` — add member and slot declarations
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — create tile, connect signals, add update slots
- Modify: `shared/nexis/static/themes/default/style/values.ini` — add `@healthScoreColor`

**Step 1: Add `@healthScoreColor` to `values.ini`**

Add after `@diskHealthColor=#FF8C00` (line 42):
```ini
@healthScoreColor=#2ec27e
```

**Step 2: Modify `dashboard_page.h`**

Add include at top (after existing tile includes ~line 27):
```cpp
#include "health_score_tile.h"
```

Add member variable (after `mFanTile` ~line 99):
```cpp
HealthScoreTile *mHealthTile;
```

Add private slots (after `onDiskHealthUpdated` ~line 72):
```cpp
void onHealthCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
void onHealthMemoryUpdated(const MemorySnapshot &snap);
void onHealthDiskUpdated(const QList<Disk> &disks);
void onHealthTempUpdated();
void onHealthBatteryUpdated(const BatteryData &bat);
void onHealthDiskHealthUpdated(const QList<DriveHealth> &drives);
```

**Step 3: Modify `dashboard_page.cpp`**

In `init()`, after `mFanTile` creation (~line 93), add:
```cpp
mHealthTile = new HealthScoreTile("@healthScoreColor", this);
```

After the fan `wrapTile` block (~line 119), add:
```cpp
wrapTile("health", mHealthTile);
```

In `tileTitle()` (~line 1436), add before the closing `}`:
```cpp
else if (id == "health") { title = tr("HEALTH"); colorToken = "@healthScoreColor"; }
```

In `defaultLayout()` (~line 1098), add health tile to row 1 after conditional tiles:
```cpp
addEntry("health", 1, col++, 1, 1);
```

In `availableStyles()` (~line 1468), add before the final return:
```cpp
if (tileId == "health")
    return {};
```

In `defaultStyle()` (~line 1477), add before the final return:
```cpp
if (tileId == "health")
    return "health";
```

In `createTile()` (~line 1447), add before the final return:
```cpp
if (style == "health")
    return new HealthScoreTile(colorToken, this);
```

Add signal connections in `init()` after the disk health connection (~line 287):
```cpp
// Health score tile data feeds
auto *calc = mHealthTile->calculator();
calc->setComponentAvailable("cpu", true);
calc->setComponentAvailable("memory", true);
calc->setComponentAvailable("disk", true);
calc->setComponentAvailable("temp", im->hasThermalSensors());
calc->setComponentAvailable("battery", im->hasBattery());
calc->setComponentAvailable("smart", im->hasDiskHealth());

connect(mRefresh, &DataRefreshService::cpuUpdated,
        this, &DashboardPage::onHealthCpuUpdated);
connect(mRefresh, &DataRefreshService::memoryUpdated,
        this, &DashboardPage::onHealthMemoryUpdated);
connect(mRefresh, &DataRefreshService::diskUsageUpdated,
        this, &DashboardPage::onHealthDiskUpdated);
if (im->hasThermalSensors())
    connect(mRefresh, &DataRefreshService::tempUpdated,
            this, &DashboardPage::onHealthTempUpdated);
if (im->hasBattery())
    connect(mRefresh, &DataRefreshService::batteryUpdated,
            this, &DashboardPage::onHealthBatteryUpdated);
connect(mRefresh, &DataRefreshService::diskHealthUpdated,
        this, &DashboardPage::onHealthDiskHealthUpdated);
```

Implement the health update slots at end of file:
```cpp
void DashboardPage::onHealthCpuUpdated(const QList<int> &percents, double clockGHz,
                                        const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)
    // Score based on 1-minute load average relative to core count
    int coreCount = im->getCpuCoreCount();
    double load1m = loadAvgs.isEmpty() ? 0.0 : loadAvgs.first();
    int score = 100;
    if (coreCount > 0 && load1m > 0) {
        double ratio = load1m / coreCount;
        score = qBound(0, qRound(100.0 * (1.0 - ratio)), 100);
    }
    mHealthTile->calculator()->setCpuScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthMemoryUpdated(const MemorySnapshot &snap)
{
    int score = 100;
    if (snap.total > 0)
        score = qBound(0, 100 - (int)(100.0 * snap.used / snap.total), 100);
    mHealthTile->calculator()->setMemoryScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthDiskUpdated(const QList<Disk> &disks)
{
    int worstScore = 100;
    for (const Disk &d : disks) {
        if (d.size == 0) continue;
        int usedPercent = (int)(100.0 * d.used / d.size);
        int diskScore = qBound(0, 100 - usedPercent, 100);
        worstScore = qMin(worstScore, diskScore);
    }
    mHealthTile->calculator()->setDiskScore(worstScore);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthTempUpdated()
{
    double tempC = im->getThermalTemperature(mSelectedSensorIndex);
    int score = 100;
    if (tempC >= 100.0) score = 0;
    else if (tempC > 60.0) score = qRound(100.0 * (100.0 - tempC) / 40.0);
    mHealthTile->calculator()->setTempScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthBatteryUpdated(const BatteryData &bat)
{
    mHealthTile->calculator()->setBatteryScore(qBound(0, bat.healthPercent, 100));
    mHealthTile->recalculate();
}

void DashboardPage::onHealthDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    int worstScore = 100;
    for (const DriveHealth &d : drives) {
        if (!d.healthy) worstScore = qMin(worstScore, 0);
        else if (d.healthPercent < 50) worstScore = qMin(worstScore, 50);
        else worstScore = qMin(worstScore, d.healthPercent);
    }
    mHealthTile->calculator()->setSmartScore(worstScore);
    mHealthTile->recalculate();
}
```

In the `onTileStyleChangeRequested` tile-pointer reassignment block (~line 1020), add:
```cpp
else if (id == "health") mHealthTile = qobject_cast<HealthScoreTile*>(newTile);
```

**Step 4: Build and verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: 100% built, no errors

**Step 5: Visual verification**

Launch the app and confirm:
- [ ] Health Score tile appears on the Dashboard in default layout
- [ ] Score displays as a number with label (Excellent/Good/Fair/Poor)
- [ ] Tile color changes based on score range
- [ ] In edit mode, tile can be moved, resized, and removed
- [ ] Breakdown bars appear when tile is 2x1 or larger

**Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.{h,cpp} \
        shared/nexis/static/themes/default/style/values.ini
git commit -m "feat(dashboard): wire HealthScoreTile into Dashboard grid (FR-73)"
```

---

## Task 4: FR-71 — LogProvider abstraction and platform implementations

**Files:**
- Create: `shared/nexis/Pages/SystemLogs/log_provider.h`
- Create: `shared/nexis/Pages/SystemLogs/log_provider.cpp`

**Step 1: Create `log_provider.h`**

```cpp
#ifndef LOG_PROVIDER_H
#define LOG_PROVIDER_H

#include <QObject>
#include <QProcess>
#include <QList>
#include <QDateTime>

struct LogEntry {
    QDateTime timestamp;
    int severity;          // 0=Emergency ... 7=Debug (syslog convention)
    QString unit;          // systemd unit or macOS subsystem/process
    QString message;

    static QString severityString(int severity);
};

class LogProvider : public QObject
{
    Q_OBJECT

public:
    explicit LogProvider(QObject *parent = nullptr);
    virtual ~LogProvider() = default;

    virtual void fetchLogs(int maxEntries = 500) = 0;
    virtual void cancel();
    bool isBusy() const { return mBusy; }

    static LogProvider *createForPlatform(QObject *parent = nullptr);

signals:
    void logsReady(const QList<LogEntry> &entries);
    void errorOccurred(const QString &message);

protected:
    QProcess *mProcess;
    bool mBusy;
};

class LogProviderLinux : public LogProvider
{
    Q_OBJECT
public:
    explicit LogProviderLinux(QObject *parent = nullptr);
    void fetchLogs(int maxEntries = 500) override;

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
};

class LogProviderMacOS : public LogProvider
{
    Q_OBJECT
public:
    explicit LogProviderMacOS(QObject *parent = nullptr);
    void fetchLogs(int maxEntries = 500) override;

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
};

#endif // LOG_PROVIDER_H
```

**Step 2: Create `log_provider.cpp`**

Implementation details:
- `LogEntry::severityString()`: returns "EMERG", "ALERT", "CRIT", "ERR", "WARN", "NOTICE", "INFO", "DEBUG" for values 0–7.
- `LogProvider::createForPlatform()`: `#ifdef Q_OS_MACOS` returns `new LogProviderMacOS(parent)`, else `new LogProviderLinux(parent)`.
- `LogProvider::cancel()`: kills process if running, sets `mBusy = false`.
- `LogProviderLinux::fetchLogs()`: starts `QProcess` with `journalctl --output=json --no-pager --lines=<maxEntries> --reverse`. On finished, parse each line as JSON object: `__REALTIME_TIMESTAMP` (microseconds since epoch → QDateTime), `PRIORITY` (int), `_SYSTEMD_UNIT` or `SYSLOG_IDENTIFIER`, `MESSAGE`.
- `LogProviderMacOS::fetchLogs()`: starts `QProcess` with `log show --style ndjson --last 1h --predicate 'eventType == logEvent'`. On finished, parse each line as JSON: `timestamp` (ISO string), `messageType` (map to severity: "Error"→3, "Fault"→2, "Default"→6, "Info"→6, "Debug"→7), `subsystem` or `process`, `eventMessage`.

**Step 3: Add to CMakeLists.txt**

Add to `GUI_SHARED_SRCS` (after `disk_tools_page.cpp` line ~272):
```cmake
"${GUI_SHARED_DIR}/Pages/SystemLogs/log_provider.cpp"
```

Add to `GUI_SHARED_HDRS` (after `disk_tools_page.h`):
```cmake
"${GUI_SHARED_DIR}/Pages/SystemLogs/log_provider.h"
```

**Step 4: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: 100% built, no errors

**Step 5: Commit**

```bash
git add shared/nexis/Pages/SystemLogs/log_provider.{h,cpp} CMakeLists.txt
git commit -m "feat(system-logs): add LogProvider abstraction with Linux/macOS backends (FR-71)"
```

---

## Task 5: FR-71 — SystemLogsPage widget

**Files:**
- Create: `shared/nexis/Pages/SystemLogs/system_logs_page.h`
- Create: `shared/nexis/Pages/SystemLogs/system_logs_page.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create `system_logs_page.h`**

```cpp
#ifndef SYSTEM_LOGS_PAGE_H
#define SYSTEM_LOGS_PAGE_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class LogProvider;

class SystemLogsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SystemLogsPage(QWidget *parent = nullptr);
    ~SystemLogsPage();

private slots:
    void onRefreshClicked();
    void onLogsReady(const QList<struct LogEntry> &entries);
    void onError(const QString &message);
    void onSeverityFilterChanged(int index);
    void onSearchTextChanged(const QString &text);
    void refreshThemeColors();

private:
    void buildLayout();
    void populateModel(const QList<struct LogEntry> &entries);
    QColor severityColor(int severity) const;

    LogProvider *mProvider;
    QStandardItemModel *mModel;
    QSortFilterProxyModel *mProxy;

    QTableView *mTableView;
    QComboBox *mCmbSeverity;
    QLineEdit *mSearchField;
    QPushButton *mBtnRefresh;
    QLabel *mLblStatus;

    int mSeverityFilter;   // minimum severity to show (0=all, 3=Error+, 4=Warning+, 6=Info+)
};

#endif // SYSTEM_LOGS_PAGE_H
```

**Step 2: Create `system_logs_page.cpp`**

Implementation details:
- `buildLayout()`: Programmatic layout (no .ui file). Vertical layout with:
  1. Filter toolbar (horizontal): `mCmbSeverity` (items: "All Severities", "Error & Above", "Warning & Above", "Info & Above"), `mSearchField` (placeholder "Search logs..."), `mBtnRefresh` (QToolButton with refresh icon).
  2. `mTableView` with `mModel` (4 columns: "Timestamp", "Severity", "Unit", "Message") through `mProxy`.
  3. Status bar (horizontal): `mLblStatus` showing entry count.
- `mProxy`: Custom `QSortFilterProxyModel` subclass or use `filterAcceptsRow()` override. Filter on severity column (column 1 stores int as UserRole) and text search across all columns.
- `onRefreshClicked()`: Disables button, calls `mProvider->fetchLogs(500)`.
- `onLogsReady()`: Calls `populateModel()`, updates status label, re-enables button.
- `populateModel()`: Clears model, iterates entries, creates row items. Severity column gets colored text via `setForeground()`. Timestamp formatted as "MMM dd HH:mm:ss". Message column gets `Qt::TextWordWrap` alignment.
- `refreshThemeColors()`: Connected to `SignalMapper::sigChangedAppTheme`. Updates table background from `@color01`, header from `@color02`.
- Severity colors: 0–3 (Emergency–Error) = `@destructiveColor`, 4 (Warning) = `@warningColor`, 5 (Notice) = `@infoColor`, 6–7 (Info/Debug) = `@color05` (default text).
- Table styling: alternating row colors, horizontal headers visible, vertical headers hidden, selection mode = single row, resize mode = stretch for message column.
- Initial load: trigger `onRefreshClicked()` at end of constructor.

**Step 3: Add to CMakeLists.txt**

Add to `GUI_SHARED_SRCS`:
```cmake
"${GUI_SHARED_DIR}/Pages/SystemLogs/system_logs_page.cpp"
```

Add to `GUI_SHARED_HDRS`:
```cmake
"${GUI_SHARED_DIR}/Pages/SystemLogs/system_logs_page.h"
```

**Step 4: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: 100% built, no errors

**Step 5: Commit**

```bash
git add shared/nexis/Pages/SystemLogs/system_logs_page.{h,cpp} CMakeLists.txt
git commit -m "feat(system-logs): add SystemLogsPage widget with filtering (FR-71)"
```

---

## Task 6: FR-71 — Wire SystemLogsPage into sidebar and navigation

**Files:**
- Modify: `shared/nexis/app.h` — add include, member, and button declarations
- Modify: `shared/nexis/app.cpp` — create sidebar button, instantiate page, wire connections

**Step 1: Modify `app.h`**

Add include (after `#include "Pages/HardwareInfo/hardware_info_page.h"` ~line 29):
```cpp
#include "Pages/SystemLogs/system_logs_page.h"
```

Add member (after `HelpersPage *helpersPage;` ~line 98):
```cpp
SystemLogsPage *systemLogsPage;
```

Add button (after `QPushButton *btnHelpers;` ~line 136):
```cpp
QPushButton *btnSystemLogs;
```

**Step 2: Modify `app.cpp` — `buildSidebar()`**

Add after `btnHelpers` creation (~line 186–187):
```cpp
btnSystemLogs = createSidebarButton(tr("System Logs"));
mSidebarLayout->addWidget(btnSystemLogs);
```

**Step 3: Modify `app.cpp` — `init()`**

Add page instantiation (after `helpersPage` ~line 271):
```cpp
systemLogsPage = new SystemLogsPage(mSlidingStacked);
```

Add button text (after `btnHelpers->setText` ~line 294):
```cpp
btnSystemLogs->setText(tr("System Logs"));
```

Add to `mListPages` — insert `systemLogsPage` after `helpersPage` in the initializer list at ~line 304:
```cpp
mListPages = {
    dashboardPage, hardwareInfoPage, resourcesPage, systemCleanerPage, diskToolsPage, searchPage,
    processPage, servicesPage, startupAppsPage, uninstallerPage, helpersPage, systemLogsPage, settingsPage
};
```

Add to `mListSidebarButtons` at ~line 309:
```cpp
mListSidebarButtons = {
    btnDash, btnHardwareInfo, btnResources, btnSystemCleaner, btnDiskTools, btnSearch,
    btnProcesses, btnServices, btnStartupApps, btnUninstaller, btnHelpers, btnSystemLogs, btnSettings
};
```

Add button click connection (after `btnHelpers` connection ~line 394):
```cpp
connect(btnSystemLogs, &QPushButton::clicked, this, [this]() { pageClick(systemLogsPage); });
```

**Step 4: Add sidebar icon**

Create or source a log/terminal SVG icon at `shared/nexis/static/themes/common/img/sidebar/system-logs.svg`. If no icon is available, use a generic document/list icon. The `updateSidebarIcons()` method maps button tooltip text to icon filenames — verify the naming convention and add the mapping.

**Step 5: Build and verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: 100% built, no errors

**Step 6: Visual verification**

Launch the app and confirm:
- [ ] "System Logs" button appears in sidebar under SYSTEM section
- [ ] Clicking it navigates to the log viewer page
- [ ] Logs load on page open (or show an error message if log command not available)
- [ ] Severity filter dropdown works
- [ ] Search field filters entries in real-time
- [ ] Refresh button reloads logs

**Step 7: Commit**

```bash
git add shared/nexis/app.{h,cpp} \
        shared/nexis/static/themes/common/img/sidebar/system-logs.svg
git commit -m "feat(system-logs): wire SystemLogsPage into sidebar navigation (FR-71)"
```

---

## Task 7: Update tracking files and documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md` — mark FR-71 and FR-73 as `[x]` done
- Modify: `docs/APPLICATION_OVERVIEW.md` — add System Logs page and Health Score tile descriptions
- Modify: `docs/ARCHITECTURE_REVIEW.md` — update signal count, note new page/tile patterns

**Step 1: Update `FEATURE_REQUESTS.md`**

Change FR-71 status from `[ ]` to `[x]` and add resolution note.
Change FR-73 status from `[ ]` to `[x]` and add resolution note.

**Step 2: Update `docs/APPLICATION_OVERVIEW.md`**

Add System Logs page description under the appropriate section. Add Health Score tile to the Dashboard section.

**Step 3: Update `docs/ARCHITECTURE_REVIEW.md`**

Note the new `LogProvider` abstraction pattern. Update signal connection count on SignalMapper if any were added (none expected — we use DataRefreshService signals directly).

**Step 4: Move backlog files to Archive**

If any `backlog/FR-71_*.md` or `backlog/FR-73_*.md` files exist, move them to `backlog/Archive/`.

**Step 5: Commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update tracking and docs for FR-71 System Logs and FR-73 Health Score"
```

---

## Task 8: Final build, test, and push

**Step 1: Clean rebuild**

```bash
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && \
  cmake --build build -j$(sysctl -n hw.ncpu)
```

**Step 2: Run tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: All existing tests pass (ScreenshotTests may still fail — pre-existing).

**Step 3: Push**

```bash
git push
```
