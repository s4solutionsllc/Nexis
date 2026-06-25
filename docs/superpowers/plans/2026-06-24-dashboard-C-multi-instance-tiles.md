# Dashboard Plan C — Multi-Instance Input-Bound Tiles Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Depends on Plans A and B being merged.** Consumes `DashboardLayout` (grid constants, schema version, the `{"version":2,"tiles":[...]}` envelope) and the `Compact` tier.

**Goal:** Let a tile type have N instances, each bound to a specific detected input (thermal sensor, fan, disk, GPU, network interface), created via an edit-mode "Add tile" palette and persisted per-tile — so a user can show CPU-fan and pump-fan, or two thermal sensors, simultaneously.

**Architecture:** Today the dashboard is keyed by `tileId` where `tileId == type` (one wrapper per type). This plan separates **type** (e.g. `"temp"`) from a unique **instance id (uid)** (e.g. `"temp#1"`) and adds a per-tile **input** binding. The wrapper carries `{uid, type, input}`. All per-tile state maps and occupancy re-key by uid. Update routines stop reading a single global selection index and instead iterate every wrapper of a given type, resolving each wrapper's bound input. The shared sensor-selection menus are replaced by a per-tile gear menu that re-binds that tile's input. An "Add tile" palette creates new instances bound to inputs. Legacy global selections migrate into the default tiles' bindings.

**Tech Stack:** C++17, Qt6 (QtWidgets, QtCore/QJson), Qt Test.

## Global Constraints

- **License/free:** GPL-3.0-only. No new dependencies.
- **Platforms:** Cross-platform; all files under `shared/`.
- **No hardcoded hex colors in C++** (BUG-47).
- **QToolButton for icon-only transparent buttons on macOS** (BUG-52): the palette's add control and any icon buttons use `QToolButton` + `setAutoRaise(true)`.
- **QScrollArea in programmatic dialogs** (the palette): if the palette uses a scroll area, set `scrollArea->setFrameShape(QFrame::NoFrame); scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");` and the inner widget `background-color:transparent;` (project Qt gotcha).
- **Build/test commands:** same as Plan A.
- **Commit style:** Conventional commits, ≤72 chars, `GH#191`, feature branch only.

## File Structure

- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_layout_util.h` / `.cpp` — add pure uid/binding helpers.
- **Modify:** `tests/managers/test_dashboard_layout_util.cpp` — cover the new helpers.
- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h` / `.cpp` — `tileType()` + `inputKey()`/`setInputKey()`; constructor gains type + input.
- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp` — re-key by uid, per-tile binding, per-tile gear menu, multi-instance update routing, settings migration.
- **Modify:** `shared/nexis/Managers/info_manager.h` / `.cpp` — `getNetworkInterfaceNames()` passthrough (thin wrapper over `getInterfaceStats()` keys / `getAllInterfaces()`).
- **Create:** `shared/nexis/Pages/Dashboard/add_tile_dialog.h` / `.cpp` — the palette.
- **Modify:** `tests/CMakeLists.txt` if the dialog needs its own test (it does not; it's exercised manually).
- **Modify:** `CHANGELOG.md`, `docs/APPLICATION_OVERVIEW.md`, `docs/ARCHITECTURE_REVIEW.md`.

## Data model (persisted, v2 envelope tiles)

Each tile object now carries:
```json
{ "id": "temp", "uid": "temp#1", "input": "k10temp/temp1",
  "row": 2, "col": 0, "rowSpan": 1, "colSpan": 1,
  "style": "sparkline", "visible": true, "color": "#FF6B1A" }
```
- `id` = **type** (unchanged meaning).
- `uid` = **unique instance id**. Absent in pre-Plan-C v2 layouts → defaults to `id` (so singletons keep working).
- `input` = bound input key (thermal sensor id, fan id, disk name, GPU name, interface name). Absent for single-input types (cpu/memory/battery/health).

---

### Task 1: Pure uid + binding helpers

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_layout_util.h` / `.cpp`
- Test: `tests/managers/test_dashboard_layout_util.cpp`

**Interfaces:**
- Produces (in `namespace DashboardLayout`):
  - `bool isMultiInstanceType(const QString &type);` → true for `temp`,`fan`,`disk`,`gpu`,`network`.
  - `QString typeOfUid(const QString &uid);` → substring before `'#'`, or the whole string if no `'#'`.
  - `QString makeUid(const QStringList &existingUids, const QString &type);` → `type` if unused, else `type#N` for the smallest N≥1 not present.
  - `QStringList usedInputsForType(const QJsonArray &tiles, const QString &type);` → the `input` values of all tiles whose `id==type`.

- [ ] **Step 1: Write the failing tests** — append to `test_dashboard_layout_util.cpp`:

```cpp
    void isMultiInstanceType_knownTypes()
    {
        QVERIFY(isMultiInstanceType("temp"));
        QVERIFY(isMultiInstanceType("fan"));
        QVERIFY(isMultiInstanceType("disk"));
        QVERIFY(isMultiInstanceType("gpu"));
        QVERIFY(isMultiInstanceType("network"));
        QVERIFY(!isMultiInstanceType("cpu"));
        QVERIFY(!isMultiInstanceType("memory"));
        QVERIFY(!isMultiInstanceType("health"));
    }

    void typeOfUid_splitsOnHash()
    {
        QCOMPARE(typeOfUid("temp"), QString("temp"));
        QCOMPARE(typeOfUid("temp#2"), QString("temp"));
        QCOMPARE(typeOfUid("fan#10"), QString("fan"));
    }

    void makeUid_firstUnused()
    {
        QStringList used { "temp", "temp#1", "cpu" };
        QCOMPARE(makeUid(used, "fan"), QString("fan"));
        QCOMPARE(makeUid(used, "temp"), QString("temp#2"));
    }

    void usedInputsForType_filtersByType()
    {
        QJsonArray tiles;
        QJsonObject a; a["id"] = "temp"; a["input"] = "s1"; tiles.append(a);
        QJsonObject b; b["id"] = "temp"; b["input"] = "s2"; tiles.append(b);
        QJsonObject c; c["id"] = "fan";  c["input"] = "f1"; tiles.append(c);
        QStringList got = usedInputsForType(tiles, "temp");
        QCOMPARE(got.size(), 2);
        QVERIFY(got.contains("s1"));
        QVERIFY(got.contains("s2"));
    }
```

- [ ] **Step 2: Run to verify failure** — `cmake --build build --target test-DashboardLayoutUtilTests` → FAIL (undeclared identifiers).

- [ ] **Step 3: Declare in `dashboard_layout_util.h`** (inside the namespace, after `migrate`):

```cpp
// True for tile types that bind to one of several detected inputs and may
// therefore appear multiple times on the dashboard. (GH#191)
bool isMultiInstanceType(const QString &type);

// The tile type encoded in a uid: the part before '#', or the whole uid.
QString typeOfUid(const QString &uid);

// Generates a unique instance id for a new tile of `type`: returns `type` if
// not present in existingUids, otherwise `type#N` for the smallest free N>=1.
QString makeUid(const QStringList &existingUids, const QString &type);

// The bound input keys of every tile of `type` in a layout-tiles array.
QStringList usedInputsForType(const QJsonArray &tiles, const QString &type);
```

Add `#include <QString>` and `#include <QStringList>` to the header.

- [ ] **Step 4: Implement in `dashboard_layout_util.cpp`**:

```cpp
#include <QJsonObject>
#include <QSet>

// ... inside namespace DashboardLayout ...

bool isMultiInstanceType(const QString &type)
{
    static const QSet<QString> kMulti { "temp", "fan", "disk", "gpu", "network" };
    return kMulti.contains(type);
}

QString typeOfUid(const QString &uid)
{
    int hash = uid.indexOf('#');
    return hash < 0 ? uid : uid.left(hash);
}

QString makeUid(const QStringList &existingUids, const QString &type)
{
    if (!existingUids.contains(type))
        return type;
    for (int n = 1; ; ++n) {
        QString candidate = QString("%1#%2").arg(type).arg(n);
        if (!existingUids.contains(candidate))
            return candidate;
    }
}

QStringList usedInputsForType(const QJsonArray &tiles, const QString &type)
{
    QStringList out;
    for (const QJsonValue &v : tiles) {
        QJsonObject o = v.toObject();
        if (o.value("id").toString() == type) {
            QString input = o.value("input").toString();
            if (!input.isEmpty())
                out.append(input);
        }
    }
    return out;
}
```

- [ ] **Step 5: Run tests** — `ctest --test-dir build -R DashboardLayoutUtilTests --output-on-failure` → PASS.

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_layout_util.h \
        shared/nexis/Pages/Dashboard/dashboard_layout_util.cpp \
        tests/managers/test_dashboard_layout_util.cpp
git commit -m "feat(dashboard): pure uid + input-binding helpers (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Wrapper carries type + input binding

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h` / `.cpp`

**Interfaces:**
- Produces:
  - Constructor: `DashboardTileWrapper(const QString &uid, const QString &type, const QString &input, QWidget *innerWidget, QWidget *parent = nullptr);`
  - `QString tileId() const;` → the uid (unchanged accessor name; now returns the unique instance id).
  - `QString tileType() const;` → the metric type.
  - `QString inputKey() const;` / `void setInputKey(const QString &input);`

- [ ] **Step 1: Header changes** — in `dashboard_tile_wrapper.h`:

Replace the constructor declaration (line 15):

```cpp
    explicit DashboardTileWrapper(const QString &tileId, QWidget *innerWidget, QWidget *parent = nullptr);
```

with:

```cpp
    explicit DashboardTileWrapper(const QString &uid, const QString &type,
                                  const QString &input, QWidget *innerWidget,
                                  QWidget *parent = nullptr);
```

After `QString tileId() const;` (line 17) add:

```cpp
    QString tileType() const;
    QString inputKey() const;
    void setInputKey(const QString &input);
```

In the private members, after `QString mTileId;` (line 60) add:

```cpp
    QString mTileType;
    QString mInputKey;
```

- [ ] **Step 2: Implementation** — in `dashboard_tile_wrapper.cpp`, update the constructor to accept and store `type` + `input` (initialize `mTileId(uid)`, `mTileType(type)`, `mInputKey(input)`), and add:

```cpp
QString DashboardTileWrapper::tileType() const { return mTileType; }
QString DashboardTileWrapper::inputKey() const { return mInputKey; }
void DashboardTileWrapper::setInputKey(const QString &input) { mInputKey = input; }
```

(Keep `tileId()` returning `mTileId`.)

- [ ] **Step 3: Build** — `cmake --build build` will FAIL at the single `new DashboardTileWrapper(id, tile, ...)` call site in `dashboard_page.cpp::wrapTile` (3-arg constructor gone). That call site is fixed in Task 3; this task's deliverable is the wrapper API. To verify the wrapper compiles in isolation is impractical (it's only built via nexis-gui), so proceed to Task 3 and treat Tasks 2+3 as one build/commit unit. **Do not commit Task 2 alone** — commit with Task 3.

---

### Task 3: Re-key DashboardPage by uid; serialize/deserialize type+uid+input

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp`

**Interfaces:**
- Consumes: Task 1 helpers, Task 2 wrapper API.
- Produces:
  - `DashboardTileWrapper *wrapTile(const QString &uid, const QString &type, const QString &input, QWidget *tile);`
  - `QList<DashboardTileWrapper*> wrappersOfType(const QString &type) const;`
  - `findWrapper(const QString &uid)` (unchanged signature; now matches uid).

This is the mechanical core. The rule: **anywhere the code currently passes `w->tileId()` (or a literal type string) to a function that wants the metric TYPE, switch to `w->tileType()` / the type.** Per-instance state maps key by uid (`w->tileId()`).

- [ ] **Step 1: Update `wrapTile` signature + body** — in `dashboard_page.h` change the declaration (line 173):

```cpp
    DashboardTileWrapper *wrapTile(const QString &uid, const QString &type,
                                   const QString &input, QWidget *tile);
```

In `dashboard_page.cpp`, update `wrapTile` to construct `new DashboardTileWrapper(uid, type, input, tile, this)` and to look up styles/customization by **uid** but build the customization defaults by **type**:

```cpp
    setupCustomizationMenu(wrapper, mTileStyles.value(uid, defaultStyle(type)));
```

- [ ] **Step 2: Add `wrappersOfType`** — in `dashboard_page.cpp` (near `findWrapper`):

```cpp
QList<DashboardTileWrapper*> DashboardPage::wrappersOfType(const QString &type) const
{
    QList<DashboardTileWrapper*> out;
    for (DashboardTileWrapper *w : mTileWrappers)
        if (w->tileType() == type)
            out.append(w);
    return out;
}
```

Declare it in the header near `findWrapper`.

- [ ] **Step 3: Rework `init()` tile creation to be layout-driven** — the current `init()` hard-creates one tile per type then wraps each. Replace the block that creates `mCpuTile … mFanTile`/`wrapTile(...)` (lines 84-121) with a two-phase approach: (a) keep direct creation+wrap for the **singletons** (cpu, memory, battery, health, and the single default for each multi type), (b) create **additional** multi-instance wrappers from the saved layout. Concretely, drive wrapping from the parsed layout tiles:

```cpp
    // Parse layout tiles up front (envelope-aware) so we can create one wrapper
    // per persisted tile, including multiple instances of a type. (GH#191)
    QJsonArray layoutTiles;
    {
        QString src = savedLayout.isEmpty()
            ? QString(QJsonDocument(defaultLayout()).toJson())
            : savedLayout;
        QJsonDocument d = QJsonDocument::fromJson(src.toUtf8());
        QJsonArray raw = d.isObject() ? d.object().value("tiles").toArray() : d.array();
        int ver = d.isObject() ? d.object().value("version").toInt(1) : 1;
        layoutTiles = DashboardLayout::migrate(raw, ver);
    }

    for (const QJsonValue &v : layoutTiles) {
        QJsonObject o = v.toObject();
        QString type = o.value("id").toString();
        QString uid  = o.contains("uid") ? o.value("uid").toString() : type;
        QString input = o.value("input").toString();
        QString style = o.contains("style") ? o.value("style").toString()
                                             : defaultStyle(type);

        // Skip unavailable optional types (no sensor present).
        if (type == "gpu" && !im->hasGpu()) continue;
        if (type == "temp" && !im->hasThermalSensors()) continue;
        if (type == "battery" && !im->hasBattery()) continue;
        if (type == "fan" && !im->hasFanSensors()) continue;

        QWidget *tile = nullptr;
        if (type == "network")      tile = new NetworkTile("@networkColor", this);
        else                        tile = createTile(type, style);

        mTileStyles[uid] = style;
        wrapTile(uid, type, input, tile);

        // Keep singleton convenience pointers for the health tile's quick action
        // and any singleton-specific wiring.
        if (type == "health") mHealthTile = qobject_cast<HealthScoreTile*>(tile);
        if (type == "network") mNetworkTile = qobject_cast<NetworkTile*>(tile);
    }
```

Remove the now-obsolete single member pointers for multi types and the standalone `mCpuTile = createTile(...)` block. **Retain** `mCpuTile`, `mMemTile`, `mBatteryTile`, `mHealthTile`, `mNetworkTile` as convenience pointers for the singletons; set them inside the loop when their type is created (e.g. `if (type=="cpu") mCpuTile = qobject_cast<MetricTileBase*>(tile);`). Remove `mDiskTile`, `mTempTile`, `mGpuTile`, `mFanTile` members entirely (they become per-instance; updates iterate `wrappersOfType`).

> NOTE: this is the largest single edit. After it, the `deserializeLayout(...)` call still runs to apply positions/spans/colors to the now-created wrappers, matching by uid (Step 5).

- [ ] **Step 4: Update `serializeLayout` to emit uid + type + input** — replace its body (lines 1255-1275):

```cpp
QJsonArray DashboardPage::serializeLayout() const
{
    QJsonArray arr;
    for (const DashboardTileWrapper *w : mTileWrappers) {
        QJsonObject obj;
        obj["id"] = w->tileType();
        obj["uid"] = w->tileId();
        if (!w->inputKey().isEmpty())
            obj["input"] = w->inputKey();
        obj["row"] = w->gridRow();
        obj["col"] = w->gridCol();
        obj["rowSpan"] = w->gridRowSpan();
        obj["colSpan"] = w->gridColSpan();
        obj["style"] = w->currentStyle();
        if (mHiddenTiles.contains(w->tileId()))
            obj["visible"] = false;
        if (mTileRanges.contains(w->tileId()))
            obj["color"] = QString("range::%1").arg(mTileRanges.value(w->tileId()));
        else if (mTileColors.contains(w->tileId()))
            obj["color"] = mTileColors.value(w->tileId());
        arr.append(obj);
    }
    return arr;
}
```

- [ ] **Step 5: Update `deserializeLayout` to match by uid + restore input** — in the per-tile loop, change `QString id = obj["id"].toString();` to read both:

```cpp
        QString type = obj["id"].toString();
        QString uid = obj.contains("uid") ? obj["uid"].toString() : type;
        QString input = obj["input"].toString();
```

Re-key the `mTileStyles/mHiddenTiles/mTileColors/mTileRanges` writes from `id` to `uid`. Change the wrapper-matching loop from `if (w->tileId() == id)` to `if (w->tileId() == uid)`, and inside it also restore the binding: `if (!input.isEmpty()) const_cast<DashboardTileWrapper*>(w)->setInputKey(input);` (the loop variable is non-const here — confirm and drop the cast).

- [ ] **Step 6: Sweep `tileId()`→`tileType()` where the TYPE is meant** — update these call sites (each currently assumes `tileId()==type`):
  - `onTileStyleChangeRequested` (line 1600): `QString id = wrapper->tileId();` → use `QString type = wrapper->tileType();` for `createTile(type, style)`, `setupTileGearMenu`, `tileUsesRangeMenu`, and key `mTileStyles` by `wrapper->tileId()` (uid). **Remove** the `if (id == "cpu") mCpuTile = newTile; …` member-pointer block for multi types; keep only the singleton assignments (cpu/memory/battery), guarded by `wrapper->tileType()`.
  - `init()` color/range restore loops (lines 133-155): key by uid (`it.key()` is already the map key = uid); the `findWrapper(it.key())` call now matches uid — correct.
  - `applyDisplayModeForSpan`: unchanged (operates on the wrapper widget).
  - `tileTitle`, `createTile`, `availableStyles`, `defaultStyle`, `setupTileGearMenu`: continue to take a **type** string — callers pass `wrapper->tileType()`.

- [ ] **Step 7: Build + run the full suite** — `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: clean build, all tests pass. Launch the app: existing single-instance layouts (default or saved) render exactly as after Plan B. (Multi-instance creation arrives in Tasks 5/7.)

- [ ] **Step 8: Commit (Tasks 2 + 3 together)**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp \
        shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "refactor(dashboard): key tiles by unique uid + type + input binding (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Multi-instance update routing (temp, fan, gpu, disk)

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp`

**Interfaces:**
- Consumes: `wrappersOfType`, `wrapper->inputKey()`, the index-based `InfoManager` getters.
- Produces: helper `int indexOfSensorId(const QList<ThermalSensor>&, const QString&)` etc. inline; update routines that iterate wrappers.

- [ ] **Step 1: Rewrite `updateTempTile()`** to iterate temp wrappers, each resolving its bound sensor id to an index:

```cpp
void DashboardPage::updateTempTile()
{
    if (!mActive) return;
    QList<ThermalSensor> sensors = im->getThermalSensors();
    for (DashboardTileWrapper *w : wrappersOfType("temp")) {
        int idx = 0;
        for (int i = 0; i < sensors.size(); ++i)
            if (sensors.at(i).id == w->inputKey()) { idx = i; break; }
        double temp = im->getThermalTemperature(idx);
        int percent = qBound(0, static_cast<int>(temp), 100);
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        tile->setValue(percent, QString("%1°C").arg(temp, 0, 'f', 1));
        tile->addDataPoint(temp);
    }
}
```

- [ ] **Step 2: Rewrite `updateFanTile()`** analogously (resolve fan id→index, use that fan's `maxRpm`):

```cpp
void DashboardPage::updateFanTile()
{
    if (!mActive) return;
    QList<FanSensor> fans = im->getFanSensors();
    for (DashboardTileWrapper *w : wrappersOfType("fan")) {
        int idx = 0;
        for (int i = 0; i < fans.size(); ++i)
            if (fans.at(i).id == w->inputKey()) { idx = i; break; }
        int rpm = im->getFanSpeed(idx);
        int maxRpm = (idx >= 0 && idx < fans.size() && fans.at(idx).maxRpm > 0)
                       ? fans.at(idx).maxRpm : 6000;
        int percent = qBound(0, static_cast<int>(rpm * 100.0 / maxRpm), 100);
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        tile->setValue(percent, QString("%1 RPM").arg(rpm));
        tile->addDataPoint(rpm);
    }
}
```

- [ ] **Step 3: Rewrite `onGpuUpdated`** to iterate gpu wrappers, binding by GPU name:

```cpp
void DashboardPage::onGpuUpdated(const QList<GpuDevice> &gpus)
{
    if (!mActive) return;
    for (DashboardTileWrapper *w : wrappersOfType("gpu")) {
        int idx = 0;
        for (int i = 0; i < gpus.size(); ++i)
            if (gpus.at(i).name == w->inputKey()) { idx = i; break; }
        if (idx < 0 || idx >= gpus.size()) continue;
        const GpuDevice &gpu = gpus.at(idx);
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        if (gpu.utilization < 0) { tile->setValue(0, tr("N/A")); }
        else {
            int util = qBound(0, gpu.utilization, 100);
            tile->setValue(util, QString("%1%").arg(util));
            tile->addDataPoint(util);
        }
    }
}
```

- [ ] **Step 4: Rewrite `onDiskUsageUpdated`** to iterate disk wrappers, binding by disk name (cache `disks` to `mCachedDisks` as today, then per-wrapper resolve). Keep `updateDiskHealthBadge()` driven per disk wrapper. Replace the single-`mDiskTile` body with a loop over `wrappersOfType("disk")`, each finding its `Disk` by `w->inputKey()` (fallback to the first disk if unbound) and calling `setDiskInfo(...)` on its `qobject_cast<MetricTileBase*>(w->innerWidget())`.

- [ ] **Step 5: Remove `mSelectedSensorIndex`, `mSelectedGpuIndex`, `mSelectedFanIndex`** from the header and constructor initializer list, plus the now-unused `onTempSensorSelected`/`onFanSensorSelected`/`onGpuDeviceSelected`/`onDiskSelected` single-selection slots (their per-tile replacements come in Task 5). Remove the shared menu members `mTempSensorMenu`/`mFanSensorMenu`/`mGpuDeviceMenu`/`mDiskMenu` and their `init()` setup blocks (lines 162-283, 341-343).

> The connections `connect(mRefresh, &DataRefreshService::tempUpdated, this, &DashboardPage::updateTempTile)` etc. stay — but move them out of the now-removed `if (im->hasThermalSensors()) { …menu… }` blocks into unconditional wiring guarded only by availability (keep the `if (im->hasThermalSensors())` guard around the connect).

- [ ] **Step 6: Build + run** — `cmake --build build && ctest --test-dir build --output-on-failure`. Launch: each existing temp/fan/gpu/disk tile shows its bound input's live value (default binding = the value migrated in Task 5; until then unbound tiles fall back to index 0 / first input). Clean build, tests pass.

- [ ] **Step 7: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): per-instance update routing for temp/fan/gpu/disk (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: Per-tile gear menu (re-bind input) + legacy settings migration

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp`

**Interfaces:**
- Produces:
  - `void DashboardPage::setupTileGearMenu(DashboardTileWrapper *wrapper);` (replaces the `(id, tile)` shared-menu version) — builds a per-tile input-selection menu for multi-instance types, with the wrapper's current `inputKey()` checked.
  - `void DashboardPage::onTileInputSelected(DashboardTileWrapper *wrapper, const QString &input);`
  - `void DashboardPage::migrateLegacyBindings(const QJsonArray &defaultTiles);` — one-time: seed default multi-instance tiles' bindings from `TempSensorId`/`FanSensorId`/`GpuDeviceId`/`DiskName`, or from the first detected input.

- [ ] **Step 1: Per-tile gear menu** — implement `setupTileGearMenu(wrapper)` that, for a multi-instance type, builds a fresh `QMenu` owned by the wrapper's gear button listing that type's detected inputs (label → input key), checks the current binding, and on trigger calls `onTileInputSelected`. For single-input types it hides the gear. Use `InfoManager` enumerations per type:
  - temp: `im->getThermalSensors()` → `{label, id}`
  - fan: `im->getFanSensors()` → `{label, id}`
  - gpu: `im->getGpuDevices()` → `{name, name}`
  - disk: `mCachedDisks` → `{name, name}`
  - network: `im->getNetworkInterfaceNames()` (Task 6) → `{name, name}`

```cpp
void DashboardPage::setupTileGearMenu(DashboardTileWrapper *wrapper)
{
    auto *tile = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    QToolButton *gear = tile ? tile->gearButton() : nullptr;
    QString type = wrapper->tileType();

    if (!gear || !DashboardLayout::isMultiInstanceType(type)) {
        if (tile) tile->setGearVisible(false);
        return;
    }

    QList<QPair<QString,QString>> inputs; // label, key
    if (type == "temp")
        for (const ThermalSensor &s : im->getThermalSensors()) inputs.append({s.label, s.id});
    else if (type == "fan")
        for (const FanSensor &f : im->getFanSensors()) inputs.append({f.label, f.id});
    else if (type == "gpu")
        for (const GpuDevice &g : im->getGpuDevices()) inputs.append({g.name, g.name});
    else if (type == "disk")
        for (const Disk &d : mCachedDisks) inputs.append({d.name, d.name});
    else if (type == "network")
        for (const QString &n : im->getNetworkInterfaceNames()) inputs.append({n, n});

    auto *menu = new QMenu(wrapper);
    for (const auto &p : inputs) {
        QAction *a = menu->addAction(p.first);
        a->setCheckable(true);
        a->setData(p.second);
        a->setChecked(p.second == wrapper->inputKey());
    }
    connect(menu, &QMenu::triggered, this, [this, wrapper](QAction *a) {
        onTileInputSelected(wrapper, a->data().toString());
    });
    gear->setMenu(menu);
    gear->setPopupMode(QToolButton::InstantPopup);
    tile->setGearVisible(inputs.size() >= 2);
}
```

Update the header declaration and every caller (the old `setupTileGearMenu("disk", mDiskTile)` etc. become `setupTileGearMenu(wrapper)` inside `wrapTile`, after the wrapper is built). Call `setupTileGearMenu(wrapper)` at the end of `wrapTile`.

- [ ] **Step 2: `onTileInputSelected`** — re-bind, refresh subtitle, persist, re-check menu, clear the tile's history, and refresh the value:

```cpp
void DashboardPage::onTileInputSelected(DashboardTileWrapper *wrapper, const QString &input)
{
    wrapper->setInputKey(input);
    auto *tile = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (tile) {
        // Subtitle = the human label for this input.
        QString label = input;
        if (wrapper->tileType() == "temp")
            for (const ThermalSensor &s : im->getThermalSensors())
                if (s.id == input) { label = s.label; break; }
        else if (wrapper->tileType() == "fan")
            for (const FanSensor &f : im->getFanSensors())
                if (f.id == input) { label = f.label; break; }
        tile->setSubtitle(label);
        tile->clearDataPoints();
        if (QMenu *m = tile->gearButton()->menu())
            for (QAction *a : m->actions())
                a->setChecked(a->data().toString() == input);
    }
    // Push a fresh value immediately for the changed type.
    if (wrapper->tileType() == "temp") updateTempTile();
    else if (wrapper->tileType() == "fan") updateFanTile();
    persistLayout();
}
```

(disk/gpu/network refresh on their next signal tick; that is acceptable — or call the matching update routine if cheap. For network, see Task 6.)

- [ ] **Step 3: Legacy binding migration** — when there is **no saved layout** (first run on Plan C, or migrating a pre-Plan-C layout that lacks `input`), seed the default multi-instance tiles' bindings. Implement and call once during `init()` right after wrappers are created and before `deserializeLayout`:

```cpp
void DashboardPage::migrateLegacyBindings()
{
    auto bindFirst = [&](const QString &type, const QString &savedId,
                         std::function<QStringList()> allIds) {
        for (DashboardTileWrapper *w : wrappersOfType(type)) {
            if (!w->inputKey().isEmpty()) continue;
            QStringList ids = allIds();
            QString chosen = (!savedId.isEmpty() && ids.contains(savedId))
                                ? savedId
                                : (ids.isEmpty() ? QString() : ids.first());
            w->setInputKey(chosen);
        }
    };
    bindFirst("temp", mSettingManager->getTempSensorId(), [&]{
        QStringList v; for (const ThermalSensor &s : im->getThermalSensors()) v << s.id; return v; });
    bindFirst("fan", mSettingManager->getFanSensorId(), [&]{
        QStringList v; for (const FanSensor &f : im->getFanSensors()) v << f.id; return v; });
    bindFirst("gpu", mSettingManager->getGpuDeviceId(), [&]{
        QStringList v; for (const GpuDevice &g : im->getGpuDevices()) v << g.name; return v; });
    bindFirst("disk", mSettingManager->getDiskName(), [&]{
        QStringList v; for (const Disk &d : im->getDisks()) v << d.name; return v; });
    bindFirst("network", im->getDefaultNetworkInterface(), [&]{
        return im->getNetworkInterfaceNames(); });
}
```

The legacy global setters (`setTempSensorId` etc.) are no longer called anywhere after this task — leave the getters in `SettingManager` for migration; mark the setters as deprecated with a `// GH#191: superseded by per-tile layout binding` comment. Do not delete them (avoids a settings-schema change).

- [ ] **Step 4: Build + run** — `cmake --build build && ctest --test-dir build --output-on-failure`. Launch: a machine with ≥2 thermal sensors shows the temp tile's gear menu; selecting a different sensor re-binds and the value updates; the choice persists across restart. Clean build, tests pass.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): per-tile input gear menu + legacy binding migration (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: Network per-interface tiles

**Files:**
- Modify: `shared/nexis/Managers/info_manager.h` / `.cpp`
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp`

**Interfaces:**
- Produces:
  - `QStringList InfoManager::getNetworkInterfaceNames() const;` — sorted keys of `getInterfaceStats()` (up+running, non-loopback).
  - `onNetworkUpdated` iterates network wrappers, each computing rx/tx deltas for its bound interface using a per-uid last-bytes cache.

- [ ] **Step 1: `InfoManager::getNetworkInterfaceNames()`** — in `info_manager.h` declare it (near line 65) and implement in `info_manager.cpp`:

```cpp
QStringList InfoManager::getNetworkInterfaceNames() const
{
    QStringList names = getInterfaceStats().keys();
    names.sort();
    return names;
}
```

- [ ] **Step 2: Per-uid last-bytes cache** — in `dashboard_page.h` add `QHash<QString, QPair<quint64,quint64>> mNetLastBytes;` (uid → {rx,tx}).

- [ ] **Step 3: Rewrite `onNetworkUpdated`** to drive every network wrapper from the per-interface stats map (ignore the aggregate `rxBytes/txBytes` args for multi-iface; keep them for the default-bound tile or drop the signal args usage):

```cpp
void DashboardPage::onNetworkUpdated(quint64 rxBytes, quint64 txBytes)
{
    Q_UNUSED(rxBytes) Q_UNUSED(txBytes)
    if (!mActive) return;

    NetInterfaceStatsMap stats = im->getInterfaceStats();
    for (DashboardTileWrapper *w : wrappersOfType("network")) {
        auto *tile = qobject_cast<NetworkTile*>(w->innerWidget());
        if (!tile) continue;
        QString iface = w->inputKey().isEmpty() ? im->getDefaultNetworkInterface()
                                                : w->inputKey();
        if (!stats.contains(iface)) continue;
        quint64 rx = stats.value(iface).rx;
        quint64 tx = stats.value(iface).tx;
        auto last = mNetLastBytes.value(w->tileId(), {rx, tx});
        tile->setInterfaceName(iface);
        tile->setValues(rx - last.first, tx - last.second, rx, tx);
        mNetLastBytes[w->tileId()] = {rx, tx};
    }
}
```

- [ ] **Step 4: Build + run** — `cmake --build build && ctest --test-dir build --output-on-failure`. Launch on a multi-NIC machine; bind two network tiles to different interfaces and confirm independent rates. Clean build, tests pass.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Managers/info_manager.h shared/nexis/Managers/info_manager.cpp \
        shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): per-interface network tiles (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 7: "Add tile" palette — choose type, then input

**Files:**
- Create: `shared/nexis/Pages/Dashboard/add_tile_dialog.h` / `.cpp`
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — `onAddTileClicked`
- Modify: the GUI target's source list (CMake) to compile the new dialog — find with `grep -rn "dashboard_tile_wrapper.cpp" --include=CMakeLists.txt .` and add `add_tile_dialog.cpp` beside it.

**Interfaces:**
- Produces: `AddTileDialog` returning a chosen `{type, input}` pair. `type` is any tile type; `input` is empty for single-input types, else the chosen input key.

- [ ] **Step 1: Dialog class** — `AddTileDialog` is a `QDialog` with a type list (display names from the existing `kTileDisplayNames` map) and, when a multi-instance type is selected, a second list of that type's **available** inputs (all detected inputs minus those already placed — passed in by the caller). Returns the selection via accessors `chosenType()` / `chosenInput()`. Build it programmatically; honor the QScrollArea-transparency and QToolButton gotchas from Global Constraints. Keep it focused (~120 lines).

```cpp
// add_tile_dialog.h
#ifndef ADD_TILE_DIALOG_H
#define ADD_TILE_DIALOG_H
#include <QDialog>
#include <QString>
#include <QList>
#include <QPair>
class QListWidget;
class AddTileDialog : public QDialog {
    Q_OBJECT
public:
    // typeOptions: (typeKey, displayName). inputsByType: typeKey -> list of
    // (inputKey, label) still AVAILABLE (not already on the dashboard).
    AddTileDialog(const QList<QPair<QString,QString>> &typeOptions,
                  const QHash<QString, QList<QPair<QString,QString>>> &inputsByType,
                  QWidget *parent = nullptr);
    QString chosenType() const;
    QString chosenInput() const;
private slots:
    void onTypeChanged();
private:
    QListWidget *mTypeList;
    QListWidget *mInputList;
    QHash<QString, QList<QPair<QString,QString>>> mInputsByType;
};
#endif
```

Implement `.cpp` with two `QListWidget`s side by side, an OK/Cancel `QDialogButtonBox`, `onTypeChanged()` repopulating `mInputList` (hidden/disabled when the selected type isn't multi-instance), and the accessors reading the current selections (`chosenInput()` returns empty when the input list is hidden).

- [ ] **Step 2: Rewrite `onAddTileClicked`** to gather available inputs, show the dialog, and create a new wrapper bound to the chosen input at the first free cell:

```cpp
void DashboardPage::onAddTileClicked()
{
    rebuildOccupancy();

    // Build the available-inputs map (detected minus already-placed).
    QJsonArray tiles = serializeLayout();
    QHash<QString, QList<QPair<QString,QString>>> avail;
    auto addAvail = [&](const QString &type, const QList<QPair<QString,QString>> &all) {
        QStringList used = DashboardLayout::usedInputsForType(tiles, type);
        QList<QPair<QString,QString>> free;
        for (const auto &p : all) if (!used.contains(p.first)) free.append(p);
        avail[type] = free;
    };
    { QList<QPair<QString,QString>> v; for (const ThermalSensor &s : im->getThermalSensors()) v.append({s.id, s.label}); addAvail("temp", v); }
    { QList<QPair<QString,QString>> v; for (const FanSensor &f : im->getFanSensors()) v.append({f.id, f.label}); addAvail("fan", v); }
    { QList<QPair<QString,QString>> v; for (const GpuDevice &g : im->getGpuDevices()) v.append({g.name, g.name}); addAvail("gpu", v); }
    { QList<QPair<QString,QString>> v; for (const Disk &d : mCachedDisks) v.append({d.name, d.name}); addAvail("disk", v); }
    { QList<QPair<QString,QString>> v; for (const QString &n : im->getNetworkInterfaceNames()) v.append({n, n}); addAvail("network", v); }

    // Type options: multi types always offered; singletons only if hidden.
    static const QList<QPair<QString,QString>> kTypes = {
        {"cpu", tr("CPU Usage")}, {"memory", tr("Memory Usage")}, {"disk", tr("Disk Usage")},
        {"network", tr("Network Speed")}, {"gpu", tr("GPU Usage")}, {"temp", tr("Temperature")},
        {"battery", tr("Battery")}, {"fan", tr("Fan Speed")}, {"health", tr("Health Score")},
    };
    QList<QPair<QString,QString>> typeOptions;
    for (const auto &t : kTypes) {
        if (DashboardLayout::isMultiInstanceType(t.first)) {
            if (!avail.value(t.first).isEmpty()) typeOptions.append(t);
        } else if (mHiddenTiles.contains(t.first) || wrappersOfType(t.first).isEmpty()) {
            typeOptions.append(t); // singleton not currently shown
        }
    }
    if (typeOptions.isEmpty()) return;

    AddTileDialog dlg(typeOptions, avail, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString type = dlg.chosenType();
    QString input = dlg.chosenInput();
    if (type.isEmpty()) return;

    // Find first free 2x2 region (fallback 1x1) for the new tile.
    int row = -1, col = -1, rs = 2, cs = 2;
    for (int r = 0; r <= GRID_ROWS - 2 && row == -1; ++r)
        for (int c = 0; c <= GRID_COLS - 2 && row == -1; ++c)
            if (regionIsFree(r, c, 2, 2)) { row = r; col = c; }
    if (row == -1) { // fall back to any free cell
        rs = cs = 1;
        for (int r = 0; r < GRID_ROWS && row == -1; ++r)
            for (int c = 0; c < GRID_COLS && row == -1; ++c)
                if (regionIsFree(r, c, 1, 1)) { row = r; col = c; }
    }
    if (row == -1) return; // grid full

    // Singleton re-show vs. new instance.
    if (!DashboardLayout::isMultiInstanceType(type)) {
        mHiddenTiles.remove(type);
        DashboardTileWrapper *w = findWrapper(type);
        if (w) w->setGridPosition(row, col, rs, cs);
    } else {
        QStringList uids;
        for (DashboardTileWrapper *w : mTileWrappers) uids << w->tileId();
        QString uid = DashboardLayout::makeUid(uids, type);
        QString style = defaultStyle(type);
        QWidget *tile = (type == "network") ? static_cast<QWidget*>(new NetworkTile("@networkColor", this))
                                            : static_cast<QWidget*>(createTile(type, style));
        mTileStyles[uid] = style;
        DashboardTileWrapper *w = wrapTile(uid, type, input, tile);
        w->setGridPosition(row, col, rs, cs);
        w->setEditMode(mEditMode);
        if (auto *m = qobject_cast<MetricTileBase*>(tile)) onTileInputSelected(w, input);
    }

    buildGrid();
    updateAddTileButton();
    persistLayout();
}
```

- [ ] **Step 3: `updateAddTileButton`** — the add button should now be available whenever there is at least one addable type, not only when a tile is hidden. Update its visibility predicate:

```cpp
void DashboardPage::updateAddTileButton()
{
    if (mAddTileButton)
        mAddTileButton->setVisible(mEditMode);
}
```

- [ ] **Step 4: Build + run** — `cmake --build build && ctest --test-dir build --output-on-failure`. Launch, enter edit mode, click "Add tile", add a second fan bound to a different fan, place it, exit edit mode, restart → both fans persist and show independent RPMs. Clean build, tests pass.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/add_tile_dialog.h shared/nexis/Pages/Dashboard/add_tile_dialog.cpp \
        shared/nexis/Pages/Dashboard/dashboard_page.cpp <the CMakeLists with the GUI source list>
git commit -m "feat(dashboard): add-tile palette for N input-bound tiles (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 8: Documentation

**Files:**
- Modify: `CHANGELOG.md`, `docs/APPLICATION_OVERVIEW.md`, `docs/ARCHITECTURE_REVIEW.md`

- [ ] **Step 1: CHANGELOG** — under a new `## [Unreleased]` (or the current dev version) section, add:

```markdown
### Added
- Dashboard: add multiple tiles of a type, each bound to a specific detected
  input — e.g. CPU fan and pump fan, or two thermal sensors — via the edit-mode
  "Add tile" palette (GH#191).
- Dashboard: per-interface network tiles.
- Dashboard: compact tile rendering keeps small tiles readable.

### Changed
- Dashboard: bento grid is now 8×8 for finer tile placement; existing layouts
  migrate automatically. Per-tile sensor selection replaces the single global
  temperature/fan/GPU/disk selection.
```

- [ ] **Step 2: APPLICATION_OVERVIEW.md** — update the dashboard section: denser grid, multiple temp/fan/disk/GPU/network tiles, the add-tile palette, compact tiles. Update any tile-count / feature-count stats and the "Last updated" date/version.

- [ ] **Step 3: ARCHITECTURE_REVIEW.md** — document: tiles keyed by unique uid + type + per-tile `input` binding (persisted in the layout JSON envelope), replacing the global sensor-selection settings; update routines iterate `wrappersOfType`; new `Compact` display tier and the `DashboardLayout` pure helper. Update the "Last updated" date/version and any signal/page/test-count stats.

- [ ] **Step 4: Commit**

```bash
git add CHANGELOG.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs(dashboard): multi-instance tiles, 8x8 grid, compact tier (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (completed during planning)

- **Spec coverage:** Plan C implements spec §3 (multi-instance input-bound tiles: Tasks 2-5, 7), §4 (scope of types incl. network-per-interface: Tasks 4, 6), §5 (settings migration: Task 5 Step 3), and §"Documentation updates" (Task 8). Combined with Plan A (§1) and Plan B (§2), all spec sections are covered.
- **Placeholder scan:** the only non-literal steps are Task 7 Step 1 (dialog `.cpp` body described structurally with a complete header) and the CMake source-list location (Task 7 — a `grep` locates it). These are discovery/standard-widget steps, not logic placeholders; all dashboard logic is shown as code.
- **Type consistency:** `tileType()`/`inputKey()`/`setInputKey()` (Task 2) are used consistently in Tasks 3-7. `wrappersOfType` (Task 3) is consumed by Tasks 4-7. `getNetworkInterfaceNames()` declared (Task 6) and used in Tasks 5, 7. `DashboardLayout::isMultiInstanceType`/`makeUid`/`usedInputsForType`/`typeOfUid` (Task 1) used in Tasks 5, 7. Wrapper constructor arg order `(uid, type, input, inner, parent)` is consistent between Task 2 and all `wrapTile`/`new DashboardTileWrapper` sites.
- **Migration safety:** pre-Plan-C v2 layouts have no `uid`/`input`; Task 3 Step 5 defaults `uid = type` and Task 5 Step 3 seeds bindings — so an upgrading user's single-instance dashboard is preserved exactly.
- **Risk note:** Task 3 Step 3 is the largest single edit (rebuilding `init()` tile creation). It is committed together with Task 2 and gated by a full build + test + manual single-instance smoke test before the multi-instance features layer on. The `init()` rework must run `migrateLegacyBindings()` (Task 5) and `deserializeLayout()` in the right order: create wrappers → migrate bindings (only fills empty) → deserialize (restores saved bindings/positions) → buildGrid.
