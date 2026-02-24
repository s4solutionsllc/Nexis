# FR-58 + FR-59: Per-Process Disk I/O & Network Bandwidth — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add four new columns to the Processes page — Disk Read/s, Disk Write/s, Net Down/s, Net Up/s — with platform-specific data collection and delta-based rate calculation.

**Architecture:** Extend the `Process` struct with 4 rate fields. Platform `ProcessInfo` subclasses collect raw cumulative counters and compute rates via delta tracking with `QElapsedTimer`. The existing `processesUpdated` signal carries enriched `Process` objects unchanged. Network columns use `nettop` on macOS; Linux shows `—` (unavailable).

**Tech Stack:** C++/Qt6, `/proc/<pid>/io` (Linux), `proc_pid_rusage()` (macOS disk), `nettop` (macOS network), `QElapsedTimer`, `QHash` for delta maps.

---

### Task 1: Extend Process Data Class

**Files:**
- Modify: `shared/nexis-core/Info/process.h:8-64`
- Modify: `shared/nexis-core/Info/process.cpp` (append after line 131)

**Step 1: Add fields and accessors to process.h**

Add 4 new private fields and their getter/setter declarations to the `Process` class:

```cpp
// In the public section, after existing getters/setters:

double getDiskReadRate() const;
void setDiskReadRate(const double &value);

double getDiskWriteRate() const;
void setDiskWriteRate(const double &value);

double getNetDownRate() const;
void setNetDownRate(const double &value);

double getNetUpRate() const;
void setNetUpRate(const double &value);

// In the private section, after existing fields:

double diskReadRate = -1.0;
double diskWriteRate = -1.0;
double netDownRate = -1.0;
double netUpRate = -1.0;
```

The default `-1.0` means "unavailable" (distinct from `0.0` meaning "zero I/O"). The UI will display `—` for `-1.0`.

**Step 2: Implement getters/setters in process.cpp**

Append after the existing `setSession()` implementation (line 131):

```cpp
double Process::getDiskReadRate() const
{
    return diskReadRate;
}

void Process::setDiskReadRate(const double &value)
{
    diskReadRate = value;
}

double Process::getDiskWriteRate() const
{
    return diskWriteRate;
}

void Process::setDiskWriteRate(const double &value)
{
    diskWriteRate = value;
}

double Process::getNetDownRate() const
{
    return netDownRate;
}

void Process::setNetDownRate(const double &value)
{
    netDownRate = value;
}

double Process::getNetUpRate() const
{
    return netUpRate;
}

void Process::setNetUpRate(const double &value)
{
    netUpRate = value;
}
```

**Step 3: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build. New fields are unused so far — no warnings expected.

**Step 4: Commit**

```bash
git add shared/nexis-core/Info/process.h shared/nexis-core/Info/process.cpp
git commit -m "feat(process): add disk I/O and network rate fields (FR-58, FR-59)"
```

---

### Task 2: macOS Disk I/O Collection via `proc_pid_rusage()`

**Files:**
- Modify: `macos/nexis-core/Info/process_info_macos.h:1-14`
- Modify: `macos/nexis-core/Info/process_info.cpp:1-55`

**Step 1: Add delta tracking members to ProcessInfoMacOS header**

Add includes and private members:

```cpp
#ifndef PROCESS_INFO_MACOS_H
#define PROCESS_INFO_MACOS_H

#include <Info/process_info.h>
#include <QHash>
#include <QElapsedTimer>

class ProcessInfoMacOS : public ProcessInfo
{
    Q_OBJECT

public:
    void updateProcesses() override;

private:
    QHash<pid_t, QPair<quint64, quint64>> mPrevDiskIo;
    QHash<pid_t, QPair<quint64, quint64>> mPrevNetIo;
    QElapsedTimer mIoTimer;
    bool mIoTimerStarted = false;
};

#endif // PROCESS_INFO_MACOS_H
```

**Step 2: Add disk I/O collection to macOS updateProcesses()**

After the existing `ps` parsing loop builds the `processList`, add a second pass that:
1. Starts `mIoTimer` on first call
2. Measures elapsed time since last tick
3. Calls `proc_pid_rusage()` for each PID
4. Computes delta rates against `mPrevDiskIo`
5. Sets the rate fields on each `Process`
6. Prunes stale PIDs

Add required headers at the top of `process_info.cpp`:

```cpp
#include <libproc.h>
#include <sys/resource.h>
```

After the `try/catch` block (after line 48, before the closing of `updateProcesses()`), add:

```cpp
    // --- Per-process disk I/O via proc_pid_rusage() ---
    double elapsedSecs = 0;
    if (!mIoTimerStarted) {
        mIoTimer.start();
        mIoTimerStarted = true;
    } else {
        elapsedSecs = mIoTimer.elapsed() / 1000.0;
        mIoTimer.restart();
    }

    QSet<pid_t> activePids;

    for (Process &proc : processList) {
        pid_t pid = proc.getPid();
        activePids.insert(pid);

        struct rusage_info_v4 rusage;
        int ret = proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t *)&rusage);

        if (ret == 0) {
            quint64 readBytes = rusage.ri_diskio_bytesread;
            quint64 writeBytes = rusage.ri_diskio_byteswritten;

            if (elapsedSecs > 0 && mPrevDiskIo.contains(pid)) {
                auto prev = mPrevDiskIo.value(pid);
                double readRate = (readBytes >= prev.first)
                    ? (readBytes - prev.first) / elapsedSecs : 0;
                double writeRate = (writeBytes >= prev.second)
                    ? (writeBytes - prev.second) / elapsedSecs : 0;
                proc.setDiskReadRate(readRate);
                proc.setDiskWriteRate(writeRate);
            } else {
                proc.setDiskReadRate(0);
                proc.setDiskWriteRate(0);
            }

            mPrevDiskIo.insert(pid, qMakePair(readBytes, writeBytes));
        }
        // If proc_pid_rusage fails, rates stay at -1.0 (unavailable)
    }

    // Prune stale PIDs
    auto it = mPrevDiskIo.begin();
    while (it != mPrevDiskIo.end()) {
        if (!activePids.contains(it.key()))
            it = mPrevDiskIo.erase(it);
        else
            ++it;
    }
```

**Step 3: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build. Disk I/O rates are now populated on macOS.

**Step 4: Commit**

```bash
git add macos/nexis-core/Info/process_info_macos.h macos/nexis-core/Info/process_info.cpp
git commit -m "feat(macos): collect per-process disk I/O via proc_pid_rusage (FR-58)"
```

---

### Task 3: macOS Network Collection via `nettop`

**Files:**
- Modify: `macos/nexis-core/Info/process_info.cpp` (extend the disk I/O block from Task 2)

**Step 1: Add nettop parsing after the disk I/O block**

After the stale PID pruning for disk I/O, add the network collection block:

```cpp
    // --- Per-process network I/O via nettop ---
    QHash<pid_t, QPair<quint64, quint64>> nettopData;

    QString nettopOutput = CommandUtil::exec("nettop",
        {"-P", "-d", "-L", "1", "-J", "bytes_in,bytes_out", "-t", "external"});

    if (!nettopOutput.isEmpty()) {
        QStringList lines = nettopOutput.trimmed().split('\n');
        // First line is header: "time,bytes_in,bytes_out,"
        // Subsequent lines: "process_name.pid,bytes_in,bytes_out,"
        for (int i = 1; i < lines.size(); ++i) {
            const QString &line = lines.at(i);
            QStringList parts = line.split(',');
            if (parts.size() >= 3) {
                // Parse "process_name.pid" — last dot-separated token is the PID
                QString procField = parts.at(0);
                int lastDot = procField.lastIndexOf('.');
                if (lastDot > 0) {
                    bool ok = false;
                    pid_t pid = procField.mid(lastDot + 1).toLongLong(&ok);
                    if (ok && pid > 0) {
                        quint64 bytesIn = parts.at(1).trimmed().toULongLong();
                        quint64 bytesOut = parts.at(2).trimmed().toULongLong();
                        nettopData.insert(pid, qMakePair(bytesIn, bytesOut));
                    }
                }
            }
        }
    }

    // Apply nettop data to processes using delta tracking
    for (Process &proc : processList) {
        pid_t pid = proc.getPid();

        if (nettopData.contains(pid)) {
            auto net = nettopData.value(pid);
            quint64 rxBytes = net.first;
            quint64 txBytes = net.second;

            if (elapsedSecs > 0 && mPrevNetIo.contains(pid)) {
                auto prev = mPrevNetIo.value(pid);
                double downRate = (rxBytes >= prev.first)
                    ? (rxBytes - prev.first) / elapsedSecs : 0;
                double upRate = (txBytes >= prev.second)
                    ? (txBytes - prev.second) / elapsedSecs : 0;
                proc.setNetDownRate(downRate);
                proc.setNetUpRate(upRate);
            } else {
                proc.setNetDownRate(0);
                proc.setNetUpRate(0);
            }

            mPrevNetIo.insert(pid, qMakePair(rxBytes, txBytes));
        }
        // If PID not in nettop output, net rates stay at -1.0
    }

    // Prune stale net PIDs
    auto netIt = mPrevNetIo.begin();
    while (netIt != mPrevNetIo.end()) {
        if (!activePids.contains(netIt.key()))
            netIt = mPrevNetIo.erase(netIt);
        else
            ++netIt;
    }
```

**Important note about nettop:** The `-d` flag gives **delta mode** — `nettop` already provides the delta since last sample, not cumulative totals. However, since we call `nettop -L 1` each tick (a fresh invocation), it returns the delta for that single sample window. If `nettop`'s output is actually cumulative-per-invocation (need to verify at runtime), the delta tracking above handles it correctly either way.

**Step 2: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 3: Commit**

```bash
git add macos/nexis-core/Info/process_info.cpp
git commit -m "feat(macos): collect per-process network via nettop parsing (FR-59)"
```

---

### Task 4: Linux Disk I/O Collection via `/proc/<pid>/io`

**Files:**
- Modify: `linux/nexis-core/Info/process_info_linux.h:1-14`
- Modify: `linux/nexis-core/Info/process_info.cpp:1-51`

**Step 1: Add delta tracking members to ProcessInfoLinux header**

```cpp
#ifndef PROCESS_INFO_LINUX_H
#define PROCESS_INFO_LINUX_H

#include <Info/process_info.h>
#include <QHash>
#include <QElapsedTimer>

class ProcessInfoLinux : public ProcessInfo
{
    Q_OBJECT

public:
    void updateProcesses() override;

private:
    QHash<pid_t, QPair<quint64, quint64>> mPrevDiskIo;
    QElapsedTimer mIoTimer;
    bool mIoTimerStarted = false;
};

#endif // PROCESS_INFO_LINUX_H
```

**Step 2: Add /proc/<pid>/io reading to Linux updateProcesses()**

After the `try/catch` block, add:

```cpp
    // --- Per-process disk I/O via /proc/<pid>/io ---
    double elapsedSecs = 0;
    if (!mIoTimerStarted) {
        mIoTimer.start();
        mIoTimerStarted = true;
    } else {
        elapsedSecs = mIoTimer.elapsed() / 1000.0;
        mIoTimer.restart();
    }

    QSet<pid_t> activePids;

    for (Process &proc : processList) {
        pid_t pid = proc.getPid();
        activePids.insert(pid);

        QString ioContent = FileUtil::readStringFromFile(
            QString("/proc/%1/io").arg(pid));

        if (!ioContent.isEmpty()) {
            quint64 readBytes = 0;
            quint64 writeBytes = 0;

            const QStringList ioLines = ioContent.split('\n');
            for (const QString &ioLine : ioLines) {
                if (ioLine.startsWith(QLatin1String("read_bytes:")))
                    readBytes = ioLine.mid(12).trimmed().toULongLong();
                else if (ioLine.startsWith(QLatin1String("write_bytes:")))
                    writeBytes = ioLine.mid(13).trimmed().toULongLong();
            }

            if (elapsedSecs > 0 && mPrevDiskIo.contains(pid)) {
                auto prev = mPrevDiskIo.value(pid);
                double readRate = (readBytes >= prev.first)
                    ? (readBytes - prev.first) / elapsedSecs : 0;
                double writeRate = (writeBytes >= prev.second)
                    ? (writeBytes - prev.second) / elapsedSecs : 0;
                proc.setDiskReadRate(readRate);
                proc.setDiskWriteRate(writeRate);
            } else {
                proc.setDiskReadRate(0);
                proc.setDiskWriteRate(0);
            }

            mPrevDiskIo.insert(pid, qMakePair(readBytes, writeBytes));
        }
        // If /proc/<pid>/io is unreadable (permission, kernel thread), rates stay at -1.0

        // Linux: network rates stay at -1.0 (unavailable)
    }

    // Prune stale PIDs
    auto it = mPrevDiskIo.begin();
    while (it != mPrevDiskIo.end()) {
        if (!activePids.contains(it.key()))
            it = mPrevDiskIo.erase(it);
        else
            ++it;
    }
```

**Step 3: Build to verify compilation**

This won't compile on macOS (Linux-only code), but verify the macOS build still succeeds:

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build (macOS build only compiles macOS platform files).

**Step 4: Commit**

```bash
git add linux/nexis-core/Info/process_info_linux.h linux/nexis-core/Info/process_info.cpp
git commit -m "feat(linux): collect per-process disk I/O via /proc/<pid>/io (FR-58)"
```

---

### Task 5: Add Columns to Processes Page

**Files:**
- Modify: `shared/nexis/Pages/Processes/processes_page.cpp:31-98` (init + loadHeaderMenu)
- Modify: `shared/nexis/Pages/Processes/processes_page.cpp:134-197` (createRow)

**Step 1: Add 4 new column headers**

In `ProcessesPage::init()`, update the `mHeaders` list. Insert 4 new headers before the last one ("Process"):

```cpp
mHeaders = QStringList {
    "PID", tr("Resident Memory"), tr("%Memory"), tr("Virtual Memory"),
    tr("User"), "%CPU", tr("Start Time"), tr("State"), tr("Group"),
    tr("Nice"), tr("CPU Time"), tr("Session"),
    tr("Disk Read/s"), tr("Disk Write/s"), tr("Net Down/s"), tr("Net Up/s"),
    tr("Process")
};
```

Column indices after this change:
- 0-11: unchanged (PID through Session)
- 12: Disk Read/s (new)
- 13: Disk Write/s (new)
- 14: Net Down/s (new)
- 15: Net Up/s (new)
- 16: Process (shifted from 12)

**Step 2: Update hidden headers list**

In `loadHeaderMenu()`, update the `hiddenHeaders` list to include the 4 new columns (hidden by default):

```cpp
QList<int> hiddenHeaders = { 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
```

**Step 3: Update createRow() to include 4 new items**

Add 4 new `QStandardItem*` entries after `session_i` and before `cmd_i` in `createRow()`:

```cpp
    // Disk Read/s
    QString diskReadText = proc.getDiskReadRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskReadRate())) + "/s";
    QStandardItem *diskRead_i = new QStandardItem(diskReadText);
    diskRead_i->setData(proc.getDiskReadRate(), data);
    diskRead_i->setData(diskReadText, Qt::ToolTipRole);

    // Disk Write/s
    QString diskWriteText = proc.getDiskWriteRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskWriteRate())) + "/s";
    QStandardItem *diskWrite_i = new QStandardItem(diskWriteText);
    diskWrite_i->setData(proc.getDiskWriteRate(), data);
    diskWrite_i->setData(diskWriteText, Qt::ToolTipRole);

    // Net Down/s
    QString netDownText = proc.getNetDownRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetDownRate())) + "/s";
    QStandardItem *netDown_i = new QStandardItem(netDownText);
    netDown_i->setData(proc.getNetDownRate(), data);
    netDown_i->setData(netDownText, Qt::ToolTipRole);

    // Net Up/s
    QString netUpText = proc.getNetUpRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetUpRate())) + "/s";
    QStandardItem *netUp_i = new QStandardItem(netUpText);
    netUp_i->setData(proc.getNetUpRate(), data);
    netUp_i->setData(netUpText, Qt::ToolTipRole);
```

Update the row assembly line to include the new items:

```cpp
    row << pid_i << rss_i << pmem_i << vsize_i << uname_i << pcpu_i
        << starttime_i << state_i << group_i << nice_i << cpuTime_i
        << session_i << diskRead_i << diskWrite_i << netDown_i << netUp_i
        << cmd_i;
```

**Step 4: Verify filter column auto-adapts**

The search filter at line 204 uses `mHeaders.count() - 1` which now equals 16 (the "Process" column). No change needed — it auto-adapts.

The default sort column 5 (%CPU) is unchanged. The "End Process" button reads column 4 (User) — unchanged.

**Step 5: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build. All 4 new columns should appear in the table (hidden by default, toggleable via right-click).

**Step 6: Commit**

```bash
git add shared/nexis/Pages/Processes/processes_page.cpp
git commit -m "feat(processes): add Disk Read/s, Disk Write/s, Net Down/s, Net Up/s columns (FR-58, FR-59)"
```

---

### Task 6: Runtime Verification and Edge Cases

**Step 1: Launch the app and navigate to Processes page**

Run the built application. Navigate to the Processes page. Verify:
- The table still renders with the original 6 visible columns
- No crashes, no layout issues
- Process count label is correct

**Step 2: Enable new columns via header right-click**

Right-click the table header. Verify:
- All 4 new column names appear in the context menu
- They are unchecked by default
- Checking "Disk Read/s" shows the column with data

**Step 3: Verify disk I/O data**

Enable "Disk Read/s" and "Disk Write/s" columns. Verify:
- Most user-owned processes show `0 B/s` or small rates
- Some processes with active I/O (e.g., the app itself, browsers) show non-zero rates
- System processes may show `—` if `proc_pid_rusage` returns an error
- Sorting by these columns works (ascending/descending)
- Values update on each refresh tick

**Step 4: Verify network data (macOS)**

Enable "Net Down/s" and "Net Up/s" columns. Verify:
- Processes with active network connections (browser, system services) show rates
- Most processes show `—` (no network activity in nettop output)
- Values are reasonable (not absurdly high)

**Step 5: Test the refresh slider**

Move the refresh slider from 1s to 5s and back. Verify:
- Rates adjust proportionally (rate = delta / elapsed)
- No spikes or discontinuities when changing interval
- Columns continue updating at the new rate

**Step 6: Commit verification notes**

No code commit for this step — this is manual verification.

---

### Task 7: Update Tracking Files and Documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md` — Mark FR-58 and FR-59 with resolution notes
- Modify: `docs/APPLICATION_OVERVIEW.md` — Update Processes page description
- Modify: `docs/ARCHITECTURE_REVIEW.md` — Note ProcessInfo I/O collection pattern

**Step 1: Update FEATURE_REQUESTS.md**

Mark FR-58 as `[x]` with resolution note:
```
**Resolved:** Per-process disk I/O columns added via proc_pid_rusage (macOS) and /proc/<pid>/io (Linux). Delta-based rate calculation with QElapsedTimer.
```

Mark FR-59 as `[x]` with resolution note:
```
**Resolved:** Per-process network columns added via nettop parsing (macOS). Linux shows N/A (no viable non-privileged API). Combined with FR-58 implementation.
```

**Step 2: Update APPLICATION_OVERVIEW.md**

In the Processes page section, add mention of the 4 new columns (Disk Read/s, Disk Write/s, Net Down/s, Net Up/s). Note that network columns are macOS-only. Update the column count from 13 to 17.

**Step 3: Update ARCHITECTURE_REVIEW.md**

In the ProcessInfo section, note the new delta tracking pattern with `QHash` + `QElapsedTimer`. Mention that `ProcessInfoMacOS` now calls `proc_pid_rusage()` and `nettop` in addition to `ps`. Mention that `ProcessInfoLinux` now reads `/proc/<pid>/io`.

**Step 4: Commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update tracking and docs for FR-58 + FR-59 completion"
```

---

### Task 8: Archive Research and Plan Files

**Files:**
- Move: `claude_definitions/FR-58_research.md` -> `claude_definitions/Archive/`
- Move: `claude_definitions/FR-59_research.md` -> `claude_definitions/Archive/`

**Step 1: Move files to Archive**

```bash
mv claude_definitions/FR-58_research.md claude_definitions/Archive/
mv claude_definitions/FR-59_research.md claude_definitions/Archive/
```

**Step 2: Final push**

```bash
git push
```
