# Repo Repair Actions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add automated repair actions to the APT Repository Health Dashboard — convert legacy repos to deb822, remove duplicates, disable/enable/remove sources, and diagnose unreachable repos with inline results.

**Architecture:** Replace the ad-hoc `repairCmd`/`repairLabel` strings with a typed `RepoRepairAction` system. A new `RepoRepairEngine` abstract base class (with platform subclasses mirroring `RepoHealthChecker`) handles all repair logic. File modifications use temp files + `pkexec` for privilege escalation, with a `writeFileElevated()` abstraction that tests can mock.

**Tech Stack:** C++17, Qt6 (Widgets, Network, Concurrent, Dns), QTest/CTest

**Spec:** `docs/superpowers/specs/2026-03-24-repo-repair-actions-design.md`

---

## File Map

### New Files

| File | Purpose |
|------|---------|
| `shared/nexis-core/Tools/repo_repair_engine.h` | Abstract base, `RepairResult`, `DiagnoseStep`, `DiagnoseResult` structs |
| `shared/nexis-core/Tools/repo_repair_engine.cpp` | Shared logic: `runCommand()`, `backupFile()`, `writeFileElevated()` |
| `linux/nexis-core/Tools/repo_repair_engine_linux.h` | `RepoRepairEngineLinux` declaration |
| `linux/nexis-core/Tools/repo_repair_engine.cpp` | Linux: convert, duplicate, disable, enable, remove, diagnose |
| `macos/nexis-core/Tools/repo_repair_engine_macos.h` | `RepoRepairEngineMac` declaration |
| `macos/nexis-core/Tools/repo_repair_engine.cpp` | macOS stubs |
| `tests/core/test_repo_repair_engine.cpp` | Unit tests for repair engine |

### Modified Files

| File | Lines | Changes |
|------|-------|---------|
| `shared/nexis-core/Tools/repo_health_types.h` | 9–17, 29–33 | Add `RepoRepairAction`; replace `repairCmd`/`repairLabel` with `actions`; add `keyUrl` to `RepoKnownInfo` |
| `shared/nexis-core/Tools/repo_knowledge_base.cpp` | 12–125 | Add `keyUrl` to `RepoPattern` struct and populate for known repos |
| `shared/nexis-core/Tools/repo_knowledge_base.h` | 6–11 | Add `keyUrl` to `RepoKnownInfo` (already in types.h, but lookup returns it) |
| `linux/nexis-core/Tools/repo_health_checker.cpp` | 139–330 | Replace `repairCmd`/`repairLabel` with `actions` list for all issue types |
| `macos/nexis-core/Tools/repo_health_checker.cpp` | 42–238 | Replace `repairCmd`/`repairLabel` with `actions` list |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h` | 25–28, 33 | Replace `repairRequested` signal with `repairActionRequested`; add diagnose display method |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp` | 226–280 | Render action button row; add diagnose inline expansion |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h` | 100–105 | Add engine member, new slot |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | 138–151, 648–673 | Replace `onRepairRequested` with typed action handler |
| `shared/nexis/Managers/tool_manager.h` | 11, 60, 68 | Add `#include`, accessor, member for `RepoRepairEngine` |
| `shared/nexis/Managers/tool_manager.cpp` | 5–34 | Include platform header, instantiate engine |
| `CMakeLists.txt` | 41–190 | Add new source/header files to build variables |
| `tests/CMakeLists.txt` | 33 | Register `RepoRepairEngineTests` |
| `shared/nexis/static/themes/default/style/style.qss` | 1803 | Add `#repoRemoveBtn` and `#repoDiagnoseStep` QSS rules |

---

## Task 1: Data Model — `RepoRepairAction` Struct and `actions` Field

**Files:**
- Modify: `shared/nexis-core/Tools/repo_health_types.h:9-17,29-33`
- Test: `tests/core/test_repo_health_checker.cpp` (existing — must still compile)

- [ ] **Step 1: Add `RepoRepairAction` struct and update `RepoHealthIssue`**

In `shared/nexis-core/Tools/repo_health_types.h`, add before `RepoHealthIssue`:

```cpp
#include <QVariantMap>

struct RepoRepairAction {
    enum Type {
        RunCommand,
        ConvertToDeb822,
        RemoveDuplicate,
        DiagnoseConnection,
        DisableSource,
        EnableSource,
        RemoveSource
    };
    Type type;
    QString label;
    QString command;        // RunCommand only
    QVariantMap context;
};
```

Then in `RepoHealthIssue`, replace:
```cpp
    QString repairCmd;
    QString repairLabel;
```
with:
```cpp
    QList<RepoRepairAction> actions;
```

Also add `keyUrl` to `RepoKnownInfo`:
```cpp
struct RepoKnownInfo {
    QString name;
    QString description;
    QString url;
    QString keyUrl;
};
```

- [ ] **Step 2: Fix compilation errors in health checkers**

The existing code in `linux/nexis-core/Tools/repo_health_checker.cpp` (lines 228–242) and `macos/nexis-core/Tools/repo_health_checker.cpp` (lines 72–73, 126–127, 162–163) sets `repairCmd` and `repairLabel`. Update all to use `actions` instead. For now, migrate the existing commands 1:1:

In linux health checker, replace each:
```cpp
issue.repairLabel = QObject::tr("Refresh signing key");
issue.repairCmd = QString("gpg ...");
```
with:
```cpp
RepoRepairAction action;
action.type = RepoRepairAction::RunCommand;
action.label = QObject::tr("Refresh signing key");
action.command = QString("gpg ...");
issue.actions.append(action);
```

Same pattern for macOS `brew upgrade`, `brew uninstall`, `brew uninstall --cask`.

- [ ] **Step 3: Fix compilation errors in detail panel**

In `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp` (lines 268–278), the `addIssueWidget` method checks `issue.repairCmd.isEmpty()`. Update to iterate `issue.actions` instead. For now, keep the same single-button behavior — render only the first action if present:

```cpp
if (!issue.actions.isEmpty()) {
    const RepoRepairAction &action = issue.actions.first();
    QPushButton *btnRepair = new QPushButton(
        action.label.isEmpty() ? tr("Repair") : action.label, issueWidget);
    btnRepair->setAccessibleName("primary");
    btnRepair->setCursor(Qt::PointingHandCursor);
    btnRepair->setFocusPolicy(Qt::NoFocus);
    btnRepair->setFixedHeight(26);
    connect(btnRepair, &QPushButton::clicked, this, [this, action]() {
        emit repairRequested(action.command, action.label);
    });
    issueLayout->addWidget(btnRepair, 0, Qt::AlignLeft);
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: Build succeeds with no errors.

- [ ] **Step 5: Run all tests**

Run: `ctest --test-dir build --output-on-failure 2>&1 | tail -10`
Expected: 25/25 pass (no regressions from data model change).

- [ ] **Step 6: Commit**

```bash
git add shared/nexis-core/Tools/repo_health_types.h \
        linux/nexis-core/Tools/repo_health_checker.cpp \
        macos/nexis-core/Tools/repo_health_checker.cpp \
        shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp
git commit -m "refactor(types): replace repairCmd/repairLabel with typed RepoRepairAction (FR-87)"
```

---

## Task 2: Knowledge Base — Add `keyUrl` Field

**Files:**
- Modify: `shared/nexis-core/Tools/repo_knowledge_base.cpp:12-125`
- Test: `tests/core/test_repo_knowledge_base.cpp` (existing tests + new)

- [ ] **Step 1: Write failing test for keyUrl lookup**

Add to `tests/core/test_repo_knowledge_base.cpp`:

```cpp
void TestRepoKnowledgeBase::lookup_docker_hasKeyUrl()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://download.docker.com/linux/ubuntu");
    QVERIFY(!info.keyUrl.isEmpty());
    QVERIFY(info.keyUrl.contains("docker"));
}

void TestRepoKnowledgeBase::lookup_ubuntu_noKeyUrl()
{
    // Ubuntu repos don't need external key URLs — they use system keyrings
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://archive.ubuntu.com/ubuntu");
    QVERIFY(info.keyUrl.isEmpty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoKnowledgeBase --output-on-failure`
Expected: FAIL — `lookup_docker_hasKeyUrl` fails (keyUrl is empty).

- [ ] **Step 3: Add `keyUrl` to `RepoPattern` struct and populate**

In `shared/nexis-core/Tools/repo_knowledge_base.cpp`, extend the `RepoPattern` struct:

```cpp
struct RepoPattern {
    const char *pattern;
    const char *name;
    const char *description;
    const char *keyUrl;     // nullptr if no external key needed
};
```

Update the `s_knownRepos` array entries. Ubuntu/Debian entries get `nullptr` (system keyrings). Third-party repos get their GPG key download URLs:

```cpp
{ "download.docker.com",        QT_TR_NOOP("Docker"),               QT_TR_NOOP("Docker Engine packages ..."), "https://download.docker.com/linux/ubuntu/gpg" },
{ "packages.microsoft.com",     QT_TR_NOOP("Microsoft"),             QT_TR_NOOP("Microsoft packages ..."),    "https://packages.microsoft.com/keys/microsoft.asc" },
{ "dl.google.com/linux/chrome",  QT_TR_NOOP("Google Chrome"),        QT_TR_NOOP("Google Chrome ..."),         "https://dl.google.com/linux/linux_signing_key.pub" },
{ "cli.github.com",             QT_TR_NOOP("GitHub CLI"),            QT_TR_NOOP("GitHub CLI ..."),            "https://cli.github.com/packages/githubcli-archive-keyring.gpg" },
{ "deb.nodesource.com",         QT_TR_NOOP("NodeSource"),            QT_TR_NOOP("NodeSource ..."),            "https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key" },
{ "apt.postgresql.org",         QT_TR_NOOP("PostgreSQL"),            QT_TR_NOOP("PostgreSQL ..."),            "https://www.postgresql.org/media/keys/ACCC4CF8.asc" },
{ "apt.grafana.com",            QT_TR_NOOP("Grafana"),               QT_TR_NOOP("Grafana ..."),               "https://apt.grafana.com/gpg.key" },
{ "apt.releases.hashicorp.com", QT_TR_NOOP("HashiCorp"),             QT_TR_NOOP("HashiCorp ..."),             "https://apt.releases.hashicorp.com/gpg" },
{ "brave-browser-apt-release.s3.brave.com", QT_TR_NOOP("Brave"),    QT_TR_NOOP("Brave Browser ..."),         "https://brave-browser-apt-release.s3.brave.com/brave-browser-archive-keyring.gpg" },
```

Update the `lookup()` function to return the `keyUrl` field.

- [ ] **Step 4: Run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoKnowledgeBase --output-on-failure`
Expected: All tests pass including new ones.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis-core/Tools/repo_knowledge_base.cpp \
        tests/core/test_repo_knowledge_base.cpp
git commit -m "feat(knowledge-base): add GPG key URLs for known repositories (FR-87)"
```

---

## Task 3: Repair Engine — Abstract Base and Shared Logic

**Files:**
- Create: `shared/nexis-core/Tools/repo_repair_engine.h`
- Create: `shared/nexis-core/Tools/repo_repair_engine.cpp`
- Modify: `CMakeLists.txt:41-95` (add to `CORE_SHARED_SRCS` and `CORE_SHARED_HDRS`)

- [ ] **Step 1: Create the header file**

Create `shared/nexis-core/Tools/repo_repair_engine.h`:

```cpp
#ifndef REPO_REPAIR_ENGINE_H
#define REPO_REPAIR_ENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <Tools/repo_health_types.h>
#include <Tools/apt_source_tool.h>

struct DiagnoseStep {
    enum Status { Ok, Warning, Failed };
    QString check;
    Status status = Ok;
    QString detail;
};

struct DiagnoseResult {
    QList<DiagnoseStep> steps;
    QString suggestion;
    QList<RepoRepairAction> followUpActions;
};

class RepoRepairEngine : public QObject
{
    Q_OBJECT
public:
    struct RepairResult {
        bool success;
        QString message;
        QString errorDetail;
    };

    virtual ~RepoRepairEngine() = default;

    // Shared (non-virtual)
    RepairResult runCommand(const QString &command);

    // Platform-specific (pure virtual)
    virtual RepairResult convertToDeb822(const APTSourcePtr &source) = 0;
    virtual RepairResult removeDuplicate(const APTSourcePtr &source) = 0;
    virtual RepairResult disableSource(const APTSourcePtr &source) = 0;
    virtual RepairResult enableSource(const APTSourcePtr &source) = 0;
    virtual RepairResult removeSource(const APTSourcePtr &source) = 0;
    virtual void diagnoseConnection(const APTSourcePtr &source) = 0;

    // Configurable output directory — tests override this to use temp dirs
    void setSourcesDir(const QString &dir) { mSourcesDir = dir; }
    QString sourcesDir() const { return mSourcesDir; }

protected:
    // File helpers — virtual so tests can mock pkexec
    virtual bool writeFileElevated(const QString &tempPath, const QString &destPath);
    virtual bool removeFileElevated(const QString &path);
    bool backupFile(const QString &filePath);

    QString mSourcesDir = "/etc/apt/sources.list.d/";

signals:
    void diagnoseFinished(const DiagnoseResult &result);
};

#endif // REPO_REPAIR_ENGINE_H
```

- [ ] **Step 2: Create the shared implementation**

Create `shared/nexis-core/Tools/repo_repair_engine.cpp`:

```cpp
#include "repo_repair_engine.h"
#include "Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>

RepoRepairEngine::RepairResult RepoRepairEngine::runCommand(const QString &command)
{
    QStringList args = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (args.isEmpty())
        return {false, {}, QObject::tr("Empty command")};

    ExecResult r = CommandUtil::execWithStatus("pkexec", args, 60000);

    if (r.exitCode == 0)
        return {true, QObject::tr("Command completed successfully"), {}};
    if (r.exitCode == 126 || r.exitCode == 127)
        return {false, QObject::tr("Authentication cancelled"), {}};

    return {false, QObject::tr("Command failed (exit code %1)").arg(r.exitCode),
            r.error.isEmpty() ? r.output : r.error};
}

bool RepoRepairEngine::writeFileElevated(const QString &tempPath, const QString &destPath)
{
    ExecResult r = CommandUtil::execWithStatus("pkexec", {"cp", tempPath, destPath}, 30000);
    return r.exitCode == 0;
}

bool RepoRepairEngine::removeFileElevated(const QString &path)
{
    ExecResult r = CommandUtil::execWithStatus("pkexec", {"rm", path}, 30000);
    return r.exitCode == 0;
}

bool RepoRepairEngine::backupFile(const QString &filePath)
{
    QString backupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
    QDir().mkpath(backupDir);

    QFileInfo fi(filePath);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QString backupPath = backupDir + "/" + fi.fileName() + "." + timestamp;

    return QFile::copy(filePath, backupPath);
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add `shared/nexis-core/Tools/repo_repair_engine.cpp` to `CORE_SHARED_SRCS` (after line ~56, near `repo_health_checker.cpp`). Add `shared/nexis-core/Tools/repo_repair_engine.h` to `CORE_SHARED_HDRS` (after line ~86, near `repo_health_checker.h`).

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: Build succeeds (abstract class + shared methods compile).

- [ ] **Step 5: Commit**

```bash
git add shared/nexis-core/Tools/repo_repair_engine.h \
        shared/nexis-core/Tools/repo_repair_engine.cpp \
        CMakeLists.txt
git commit -m "feat(repair): add RepoRepairEngine abstract base with shared helpers (FR-87)"
```

---

## Task 4: Linux Repair Engine — Disable, Enable, Remove

**Files:**
- Create: `linux/nexis-core/Tools/repo_repair_engine_linux.h`
- Create: `linux/nexis-core/Tools/repo_repair_engine.cpp`
- Create: `tests/core/test_repo_repair_engine.cpp`
- Modify: `CMakeLists.txt:146-190` (add to Linux `CORE_PLAT_SRCS`/`CORE_PLAT_HDRS`)
- Modify: `tests/CMakeLists.txt:33` (register test)

- [ ] **Step 1: Write failing tests**

Create `tests/core/test_repo_repair_engine.cpp`:

```cpp
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "Tools/repo_health_types.h"
#include "Tools/repo_repair_engine.h"
#include "Tools/apt_source_tool.h"

#ifdef Q_OS_LINUX
#include "Tools/repo_repair_engine_linux.h"
#endif

// Test subclass that bypasses pkexec — writes directly
class TestableRepairEngine : public
#ifdef Q_OS_LINUX
    RepoRepairEngineLinux
#else
    RepoRepairEngine
#endif
{
protected:
    bool writeFileElevated(const QString &tempPath, const QString &destPath) override {
        QFile::remove(destPath);
        return QFile::copy(tempPath, destPath);
    }
    bool removeFileElevated(const QString &path) override {
        return QFile::remove(path);
    }
#ifndef Q_OS_LINUX
    // Stubs for non-Linux
    RepairResult convertToDeb822(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeDuplicate(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult disableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult enableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    void diagnoseConnection(const APTSourcePtr &) override {}
#endif
};

class TestRepoRepairEngine : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir mTempDir;
    TestableRepairEngine mEngine;

    // Helper: write a file with given content, return path
    QString writeTestFile(const QString &name, const QString &content) {
        QString path = mTempDir.path() + "/" + name;
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
        return path;
    }

    QString readFile(const QString &path) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        return QString::fromUtf8(f.readAll());
    }

private slots:
#ifdef Q_OS_LINUX
    void disableSource_legacyList_commentsLine();
    void disableSource_deb822_addsEnabledNo();
    void enableSource_legacyList_uncommentsLine();
    void enableSource_deb822_removesEnabledNo();
    void removeSource_activeSource_refuses();
    void removeSource_disabledLegacy_removesLine();
    void removeSource_onlyEntryInFile_deletesFile();
#endif
};

#ifdef Q_OS_LINUX
void TestRepoRepairEngine::disableSource_legacyList_commentsLine()
{
    QString content = "deb http://archive.ubuntu.com/ubuntu jammy main restricted\n"
                      "deb http://other.example.com/repo stable main\n";
    QString path = writeTestFile("test.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://archive.ubuntu.com/ubuntu";
    src->suites = "jammy";
    src->components = "main restricted";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Legacy;

    auto result = mEngine.disableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(modified.contains("# deb http://archive.ubuntu.com/ubuntu jammy main restricted"));
    QVERIFY(modified.contains("deb http://other.example.com/repo stable main"));
}

void TestRepoRepairEngine::disableSource_deb822_addsEnabledNo()
{
    QString content = "Types: deb\n"
                      "URIs: http://example.com/repo\n"
                      "Suites: stable\n"
                      "Components: main\n";
    QString path = writeTestFile("test.sources", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/repo";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Deb822;

    auto result = mEngine.disableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(modified.contains("Enabled: no"));
}

void TestRepoRepairEngine::enableSource_legacyList_uncommentsLine()
{
    QString content = "# deb http://archive.ubuntu.com/ubuntu jammy main restricted\n";
    QString path = writeTestFile("test.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://archive.ubuntu.com/ubuntu";
    src->suites = "jammy";
    src->components = "main restricted";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Legacy;

    auto result = mEngine.enableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(modified.startsWith("deb http://archive.ubuntu.com/ubuntu"));
    QVERIFY(!modified.contains("# deb"));
}

void TestRepoRepairEngine::enableSource_deb822_removesEnabledNo()
{
    QString content = "Types: deb\n"
                      "URIs: http://example.com/repo\n"
                      "Suites: stable\n"
                      "Components: main\n"
                      "Enabled: no\n";
    QString path = writeTestFile("test.sources", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/repo";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Deb822;

    auto result = mEngine.enableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(!modified.contains("Enabled: no"));
    QVERIFY(modified.contains("URIs: http://example.com/repo"));
}

void TestRepoRepairEngine::removeSource_activeSource_refuses()
{
    APTSourcePtr src(new APTSource);
    src->isActive = true;

    auto result = mEngine.removeSource(src);
    QVERIFY(!result.success);
}

void TestRepoRepairEngine::removeSource_disabledLegacy_removesLine()
{
    QString content = "# deb http://old.repo.com/ubuntu jammy main\n"
                      "deb http://other.repo.com/ubuntu jammy main\n";
    QString path = writeTestFile("test.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://old.repo.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Legacy;

    auto result = mEngine.removeSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(!modified.contains("old.repo.com"));
    QVERIFY(modified.contains("other.repo.com"));
}

void TestRepoRepairEngine::removeSource_onlyEntryInFile_deletesFile()
{
    QString content = "# deb http://dead.repo.com/ubuntu jammy main\n";
    QString path = writeTestFile("single.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://dead.repo.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Legacy;

    auto result = mEngine.removeSource(src);
    QVERIFY(result.success);
    QVERIFY(!QFile::exists(path));
}
#endif

QTEST_MAIN(TestRepoRepairEngine)
#include "test_repo_repair_engine.moc"
```

- [ ] **Step 2: Create Linux engine header**

Create `linux/nexis-core/Tools/repo_repair_engine_linux.h`:

```cpp
#ifndef REPO_REPAIR_ENGINE_LINUX_H
#define REPO_REPAIR_ENGINE_LINUX_H

#include "Tools/repo_repair_engine.h"

class RepoRepairEngineLinux : public RepoRepairEngine
{
public:
    RepairResult convertToDeb822(const APTSourcePtr &source) override;
    RepairResult removeDuplicate(const APTSourcePtr &source) override;
    RepairResult disableSource(const APTSourcePtr &source) override;
    RepairResult enableSource(const APTSourcePtr &source) override;
    RepairResult removeSource(const APTSourcePtr &source) override;
    void diagnoseConnection(const APTSourcePtr &source) override;

private:
    // Helper: read file, modify line matching source, write back
    RepairResult modifySourceFile(const APTSourcePtr &source,
        std::function<QString(const QString &line)> lineTransform,
        const QString &successMsg);

    // Helper: find the line in a .list file that matches this source
    QString buildMatchPattern(const APTSourcePtr &source) const;
};

#endif // REPO_REPAIR_ENGINE_LINUX_H
```

- [ ] **Step 3: Implement disable, enable, remove**

Create `linux/nexis-core/Tools/repo_repair_engine.cpp`:

```cpp
#include "repo_repair_engine_linux.h"
#include "repo_knowledge_base.h"
#include "Utils/command_util.h"
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDnsLookup>
#include <QUrl>
#include <QDesktopServices>
#include <QFileInfo>

QString RepoRepairEngineLinux::buildMatchPattern(const APTSourcePtr &source) const
{
    // Build a pattern that matches the source line in a .list file
    // e.g., "deb http://archive.ubuntu.com/ubuntu jammy main restricted"
    return QString("deb %1 %2 %3").arg(source->uri, source->suites, source->components);
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::modifySourceFile(
    const APTSourcePtr &source,
    std::function<QString(const QString &line)> lineTransform,
    const QString &successMsg)
{
    QFile file(source->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, QObject::tr("Cannot read %1").arg(source->filePath)};

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    backupFile(source->filePath);

    QStringList lines = content.split('\n');
    bool found = false;

    if (source->format == APTSource::Deb822) {
        // For deb822, lineTransform receives the entire stanza
        QString result;
        QString stanza;
        bool inMatchingStanza = false;

        for (const QString &line : lines) {
            if (line.trimmed().isEmpty() && !stanza.isEmpty()) {
                if (inMatchingStanza && !found) {
                    stanza = lineTransform(stanza);
                    found = true;
                }
                result += stanza + "\n";
                stanza.clear();
                inMatchingStanza = false;
                continue;
            }
            stanza += line + "\n";
            if (line.startsWith("URIs:") && line.contains(source->uri))
                inMatchingStanza = true;
        }
        if (!stanza.isEmpty()) {
            if (inMatchingStanza && !found) {
                stanza = lineTransform(stanza);
                found = true;
            }
            result += stanza;
        }
        content = result;
    } else {
        // Legacy .list: match the specific line
        QString pattern = buildMatchPattern(source);
        for (int i = 0; i < lines.size(); ++i) {
            QString stripped = lines[i].trimmed();
            // Match both active and commented versions
            if (stripped == pattern || stripped == "# " + pattern ||
                stripped == "#" + pattern) {
                lines[i] = lineTransform(lines[i]);
                found = true;
                break;
            }
        }
        content = lines.join('\n');
    }

    if (!found)
        return {false, {}, QObject::tr("Could not find matching entry in %1").arg(source->filePath)};

    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return {false, {}, QObject::tr("Cannot create temporary file")};
    tmp.write(content.toUtf8());
    tmp.close();

    if (!writeFileElevated(tmp.fileName(), source->filePath)) {
        QFile::remove(tmp.fileName());
        return {false, {}, QObject::tr("Failed to write %1 (pkexec denied or failed)").arg(source->filePath)};
    }
    QFile::remove(tmp.fileName());

    return {true, successMsg, {}};
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::disableSource(const APTSourcePtr &source)
{
    if (source->format == APTSource::Deb822) {
        return modifySourceFile(source, [](const QString &stanza) {
            if (stanza.contains("Enabled:"))
                return QString(stanza).replace(QRegularExpression("Enabled:\\s*yes", QRegularExpression::CaseInsensitiveOption), "Enabled: no");
            return "Enabled: no\n" + stanza;
        }, QObject::tr("Repository disabled"));
    }

    return modifySourceFile(source, [](const QString &line) {
        QString stripped = line.trimmed();
        if (!stripped.startsWith('#'))
            return "# " + line;
        return line;
    }, QObject::tr("Repository disabled"));
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::enableSource(const APTSourcePtr &source)
{
    if (source->format == APTSource::Deb822) {
        return modifySourceFile(source, [](const QString &stanza) {
            QString result = stanza;
            result.remove(QRegularExpression("Enabled:\\s*no\\n?", QRegularExpression::CaseInsensitiveOption));
            return result;
        }, QObject::tr("Repository enabled"));
    }

    return modifySourceFile(source, [](const QString &line) {
        QString stripped = line.trimmed();
        if (stripped.startsWith("# "))
            return stripped.mid(2);
        if (stripped.startsWith("#"))
            return stripped.mid(1);
        return line;
    }, QObject::tr("Repository enabled"));
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::removeSource(const APTSourcePtr &source)
{
    if (source->isActive)
        return {false, QObject::tr("Cannot remove an active repository. Disable it first."), {}};

    QFile file(source->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, QObject::tr("Cannot read %1").arg(source->filePath)};

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    backupFile(source->filePath);

    // Check if this is the only entry in the file
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    int entryCount = 0;
    for (const QString &line : lines) {
        QString stripped = line.trimmed();
        if (!stripped.isEmpty() && !stripped.startsWith('#'))
            entryCount++;
    }

    // For disabled legacy sources, count commented deb lines
    if (source->format == APTSource::Legacy) {
        int debLineCount = 0;
        for (const QString &line : lines) {
            QString stripped = line.trimmed();
            if (stripped.startsWith("deb ") || stripped.startsWith("# deb ") || stripped.startsWith("#deb "))
                debLineCount++;
        }
        if (debLineCount <= 1) {
            // Only entry — delete the file
            if (!removeFileElevated(source->filePath))
                return {false, {}, QObject::tr("Failed to delete %1").arg(source->filePath)};
            return {true, QObject::tr("Repository file deleted: %1").arg(QFileInfo(source->filePath).fileName()), {}};
        }
    }

    // Multiple entries — remove just this one
    return modifySourceFile(source, [](const QString &) {
        return QString(); // Remove the line entirely
    }, QObject::tr("Repository entry removed"));
}

// Stubs for remaining methods — implemented in subsequent tasks
RepoRepairEngine::RepairResult RepoRepairEngineLinux::convertToDeb822(const APTSourcePtr &)
{
    return {false, QObject::tr("Not yet implemented"), {}};
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::removeDuplicate(const APTSourcePtr &)
{
    return {false, QObject::tr("Not yet implemented"), {}};
}

void RepoRepairEngineLinux::diagnoseConnection(const APTSourcePtr &)
{
    // Stub — implemented in Task 7
}
```

- [ ] **Step 4: Create macOS stubs**

Create `macos/nexis-core/Tools/repo_repair_engine_macos.h`:
```cpp
#ifndef REPO_REPAIR_ENGINE_MACOS_H
#define REPO_REPAIR_ENGINE_MACOS_H

#include "Tools/repo_repair_engine.h"

class RepoRepairEngineMac : public RepoRepairEngine
{
public:
    RepairResult convertToDeb822(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeDuplicate(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult disableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult enableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    void diagnoseConnection(const APTSourcePtr &) override {}
};

#endif // REPO_REPAIR_ENGINE_MACOS_H
```

Create `macos/nexis-core/Tools/repo_repair_engine.cpp`:
```cpp
// macOS Homebrew uses RunCommand type for repairs (brew upgrade, brew uninstall).
// No platform-specific repair engine methods needed.
```

- [ ] **Step 5: Add to CMakeLists.txt and register test**

Add to Linux `CORE_PLAT_SRCS` (~line 162): `linux/nexis-core/Tools/repo_repair_engine.cpp`
Add to Linux `CORE_PLAT_HDRS` (~line 184): `linux/nexis-core/Tools/repo_repair_engine_linux.h`
Add to macOS `CORE_PLAT_SRCS` (~line 117): `macos/nexis-core/Tools/repo_repair_engine.cpp`
Add to macOS `CORE_PLAT_HDRS` (~line 141): `macos/nexis-core/Tools/repo_repair_engine_macos.h`

In `tests/CMakeLists.txt` (~line 33), add:
```cmake
add_nexis_test(NAME RepoRepairEngineTests SOURCES core/test_repo_repair_engine.cpp LIBS Qt6::Network)
```

- [ ] **Step 6: Build and run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoRepairEngine --output-on-failure`
Expected: All 7 tests pass.

- [ ] **Step 7: Run full test suite**

Run: `ctest --test-dir build --output-on-failure 2>&1 | tail -10`
Expected: 26/26 pass (25 existing + 1 new test executable).

- [ ] **Step 8: Commit**

```bash
git add linux/nexis-core/Tools/repo_repair_engine_linux.h \
        linux/nexis-core/Tools/repo_repair_engine.cpp \
        macos/nexis-core/Tools/repo_repair_engine_macos.h \
        macos/nexis-core/Tools/repo_repair_engine.cpp \
        tests/core/test_repo_repair_engine.cpp \
        CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(repair): implement disable/enable/remove source actions (FR-87)"
```

---

## Task 5: Linux Repair Engine — Remove Duplicate

**Files:**
- Modify: `linux/nexis-core/Tools/repo_repair_engine.cpp` (replace stub)
- Modify: `tests/core/test_repo_repair_engine.cpp` (add tests)

- [ ] **Step 1: Write failing tests**

Add to `tests/core/test_repo_repair_engine.cpp` (in the `private slots:` section and implementations):

```cpp
    void removeDuplicate_commentsSecondOccurrence();
    void removeDuplicate_differentFiles();
```

```cpp
void TestRepoRepairEngine::removeDuplicate_commentsSecondOccurrence()
{
    QString content = "deb http://repo.example.com/ubuntu jammy main\n"
                      "deb http://other.example.com/ubuntu jammy main\n"
                      "deb http://repo.example.com/ubuntu jammy main\n";
    QString path = writeTestFile("dupes.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://repo.example.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Legacy;

    auto result = mEngine.removeDuplicate(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    // First occurrence should remain active, second commented
    int activeCount = modified.count("deb http://repo.example.com/ubuntu jammy main");
    int commentedCount = modified.count("# Disabled by Nexis: duplicate entry");
    QCOMPARE(commentedCount, 1);
    // The active deb line should still exist
    QVERIFY(modified.contains("\ndeb http://repo.example.com/ubuntu jammy main\n") ||
            modified.startsWith("deb http://repo.example.com/ubuntu jammy main\n"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoRepairEngine --output-on-failure`
Expected: FAIL — stub returns `{false, "Not yet implemented"}`.

- [ ] **Step 3: Implement `removeDuplicate`**

In `linux/nexis-core/Tools/repo_repair_engine.cpp`, replace the stub:

```cpp
RepoRepairEngine::RepairResult RepoRepairEngineLinux::removeDuplicate(const APTSourcePtr &source)
{
    QFile file(source->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, QObject::tr("Cannot read %1").arg(source->filePath)};

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    backupFile(source->filePath);

    QString pattern = buildMatchPattern(source);
    QStringList lines = content.split('\n');
    bool firstSeen = false;
    bool commented = false;

    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == pattern) {
            if (!firstSeen) {
                firstSeen = true; // Keep first occurrence
            } else {
                // Comment out subsequent occurrences
                lines[i] = "# Disabled by Nexis: duplicate entry\n# " + lines[i];
                commented = true;
                break;
            }
        }
    }

    if (!commented)
        return {false, {}, QObject::tr("No duplicate entry found to remove")};

    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return {false, {}, QObject::tr("Cannot create temporary file")};
    tmp.write(lines.join('\n').toUtf8());
    tmp.close();

    if (!writeFileElevated(tmp.fileName(), source->filePath)) {
        QFile::remove(tmp.fileName());
        return {false, {}, QObject::tr("Failed to write %1").arg(source->filePath)};
    }
    QFile::remove(tmp.fileName());

    return {true, QObject::tr("Duplicate entry commented out"), {}};
}
```

- [ ] **Step 4: Run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoRepairEngine --output-on-failure`
Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add linux/nexis-core/Tools/repo_repair_engine.cpp \
        tests/core/test_repo_repair_engine.cpp
git commit -m "feat(repair): implement duplicate source removal (FR-87)"
```

---

## Task 6: Linux Repair Engine — Convert Legacy to Deb822

**Files:**
- Modify: `linux/nexis-core/Tools/repo_repair_engine.cpp` (replace stub)
- Modify: `tests/core/test_repo_repair_engine.cpp` (add tests)

- [ ] **Step 1: Write failing tests**

Add tests:

```cpp
    void convertToDeb822_generatesCorrectContent();
    void convertToDeb822_withoutSignedBy_warnsInResult();
    void convertToDeb822_commentsOutOldEntry();
```

```cpp
void TestRepoRepairEngine::convertToDeb822_generatesCorrectContent()
{
    // Point engine's sourcesDir to a writable temp directory
    QString testSourcesDir = mTempDir.path() + "/sources.list.d/";
    QDir().mkpath(testSourcesDir);
    mEngine.setSourcesDir(testSourcesDir);

    QString content = "deb http://example.com/repo jammy main contrib\n";
    QString path = writeTestFile("convert.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/repo";
    src->suites = "jammy";
    src->components = "main contrib";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Legacy;
    src->signedByPath = "/usr/share/keyrings/example-keyring.gpg";

    auto result = mEngine.convertToDeb822(src);
    QVERIFY(result.success);

    // Check the .list file was commented out
    QString modified = readFile(path);
    QVERIFY(modified.contains("# Converted to deb822 by Nexis"));

    // Check the .sources file was written with correct content
    QStringList sourcesFiles = QDir(testSourcesDir).entryList({"*.sources"});
    QCOMPARE(sourcesFiles.size(), 1);
    QString sourcesContent = readFile(testSourcesDir + sourcesFiles.first());
    QVERIFY(sourcesContent.contains("Types: deb"));
    QVERIFY(sourcesContent.contains("URIs: http://example.com/repo"));
    QVERIFY(sourcesContent.contains("Suites: jammy"));
    QVERIFY(sourcesContent.contains("Components: main contrib"));
    QVERIFY(sourcesContent.contains("Signed-By: /usr/share/keyrings/example-keyring.gpg"));
}

void TestRepoRepairEngine::convertToDeb822_withoutSignedBy_warnsInResult()
{
    QString testSourcesDir = mTempDir.path() + "/sources2.list.d/";
    QDir().mkpath(testSourcesDir);
    mEngine.setSourcesDir(testSourcesDir);

    QString content = "deb http://unknown-repo.example.com/ubuntu jammy main\n";
    QString path = writeTestFile("nosignedby.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://unknown-repo.example.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Legacy;
    // No signedByPath — key download will fail (unknown repo, no network in tests)

    auto result = mEngine.convertToDeb822(src);
    // Should still succeed (conversion works without key) but warn
    QVERIFY(result.success);
    QVERIFY(result.message.contains("Warning") || result.message.contains("Signed-By not set"));
}
```

- [ ] **Step 2: Implement `convertToDeb822`**

In `linux/nexis-core/Tools/repo_repair_engine.cpp`, replace the stub:

```cpp
static QString generateDeb822(const APTSourcePtr &source, const QString &signedByPath)
{
    QString content;
    content += "Types: deb\n";
    content += "URIs: " + source->uri + "\n";
    content += "Suites: " + source->suites + "\n";
    content += "Components: " + source->components + "\n";
    if (!signedByPath.isEmpty())
        content += "Signed-By: " + signedByPath + "\n";
    return content;
}

static QString domainToFilename(const QString &uri)
{
    QUrl url(uri);
    QString domain = url.host();
    if (domain.isEmpty()) {
        // Fallback regex for non-URL formats
        QRegularExpression re("://([^/]+)");
        auto match = re.match(uri);
        domain = match.hasMatch() ? match.captured(1) : "unknown";
    }
    return domain.replace('.', '-').replace(':', '-');
}

static QString resolveGpgKeyPath(const APTSourcePtr &source)
{
    if (!source->signedByPath.isEmpty())
        return source->signedByPath;

    // Try knowledge base
    RepoKnownInfo info = RepoKnowledgeBase::lookup(source->uri);
    if (!info.keyUrl.isEmpty()) {
        // Key URL known — would download in production. For now, return expected path.
        QString filename = domainToFilename(source->uri) + "-archive-keyring.gpg";
        return "/usr/share/keyrings/" + filename;
    }

    return {};
}

static bool downloadGpgKey(const QString &url, const QString &destPath)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(url));
    req.setTransferTimeout(10000);
    QNetworkReply *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(11000, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return false;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    // Validate: PGP armored key or binary keyring
    if (!data.startsWith("-----BEGIN PGP") && data.size() < 100)
        return false;

    QFile f(destPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    f.close();
    return true;
}

RepoRepairEngine::RepairResult RepoRepairEngineLinux::convertToDeb822(const APTSourcePtr &source)
{
    // Step 1: Resolve GPG key
    QString signedByPath = source->signedByPath;
    QString keyWarning;

    if (signedByPath.isEmpty()) {
        RepoKnownInfo info = RepoKnowledgeBase::lookup(source->uri);
        QString filename = domainToFilename(source->uri) + "-archive-keyring.gpg";
        QString keyDest = "/usr/share/keyrings/" + filename;

        bool keyDownloaded = false;
        if (!info.keyUrl.isEmpty()) {
            // Download from knowledge base URL
            QTemporaryFile tmpKey;
            tmpKey.setAutoRemove(false);
            if (tmpKey.open() && downloadGpgKey(info.keyUrl, tmpKey.fileName())) {
                if (writeFileElevated(tmpKey.fileName(), keyDest))
                    keyDownloaded = true;
                QFile::remove(tmpKey.fileName());
            }
        }

        if (!keyDownloaded) {
            // Auto-discover
            QStringList tryPaths = {"/gpg.key", "/key.gpg", "/signing-key.asc",
                                    "/gpg", "/Release.key", "/apt-key.gpg"};
            QString baseUri = source->uri;
            if (!baseUri.endsWith('/')) baseUri += '/';
            // Also try the base domain
            QUrl url(source->uri);
            QString domainBase = url.scheme() + "://" + url.host();

            for (const QString &path : tryPaths) {
                QTemporaryFile tmpKey;
                tmpKey.setAutoRemove(false);
                if (tmpKey.open()) {
                    if (downloadGpgKey(baseUri + path, tmpKey.fileName()) ||
                        downloadGpgKey(domainBase + path, tmpKey.fileName())) {
                        if (writeFileElevated(tmpKey.fileName(), keyDest)) {
                            keyDownloaded = true;
                            QFile::remove(tmpKey.fileName());
                            break;
                        }
                    }
                    QFile::remove(tmpKey.fileName());
                }
            }
        }

        if (keyDownloaded) {
            signedByPath = keyDest;
        } else {
            keyWarning = QObject::tr(" (Warning: GPG key could not be downloaded — Signed-By not set)");
        }
    }

    // Step 2: Generate deb822 content
    QString deb822Content = generateDeb822(source, signedByPath);

    // Step 3: Write .sources file
    QString sourcesName = domainToFilename(source->uri) + ".sources";
    QString srcDir = sourcesDir();
    QString sourcesPath = srcDir + sourcesName;

    // Handle filename collision
    for (int i = 1; QFile::exists(sourcesPath) && i <= 10; ++i)
        sourcesPath = srcDir + domainToFilename(source->uri) + "-" + QString::number(i) + ".sources";

    if (QFile::exists(sourcesPath))
        return {false, {}, QObject::tr("Could not find available filename in %1").arg(srcDir)};

    QTemporaryFile tmpSources;
    tmpSources.setAutoRemove(false);
    if (!tmpSources.open())
        return {false, {}, QObject::tr("Cannot create temporary file")};
    tmpSources.write(deb822Content.toUtf8());
    tmpSources.close();

    if (!writeFileElevated(tmpSources.fileName(), sourcesPath)) {
        QFile::remove(tmpSources.fileName());
        return {false, {}, QObject::tr("Failed to write %1").arg(sourcesPath)};
    }
    QFile::remove(tmpSources.fileName());

    // Step 4: Comment out old .list entry
    auto commentResult = modifySourceFile(source, [](const QString &line) {
        return "# Converted to deb822 by Nexis\n# " + line;
    }, {});

    if (!commentResult.success)
        return {false, {}, QObject::tr("Wrote %1 but failed to comment out old entry: %2")
            .arg(sourcesPath, commentResult.errorDetail)};

    return {true, QObject::tr("Converted to deb822 format: %1%2")
        .arg(QFileInfo(sourcesPath).fileName(), keyWarning), {}};
}
```

- [ ] **Step 3: Run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoRepairEngine --output-on-failure`
Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add linux/nexis-core/Tools/repo_repair_engine.cpp \
        tests/core/test_repo_repair_engine.cpp
git commit -m "feat(repair): implement legacy-to-deb822 conversion (FR-87)"
```

---

## Task 7: Linux Repair Engine — Diagnose Connection

**Files:**
- Modify: `linux/nexis-core/Tools/repo_repair_engine.cpp` (replace stub)
- Modify: `tests/core/test_repo_repair_engine.cpp` (add test)

- [ ] **Step 1: Write test for suggestion synthesis**

Add test:

```cpp
    void diagnoseConnection_emitsResult();
```

```cpp
void TestRepoRepairEngine::diagnoseConnection_emitsResult()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://nonexistent.invalid.test/repo";
    src->suites = "jammy";
    src->components = "main";

    QSignalSpy spy(&mEngine, &RepoRepairEngine::diagnoseFinished);
    mEngine.diagnoseConnection(src);

    // Wait for async completion (max 30s for DNS + HTTP timeouts)
    QVERIFY(spy.wait(30000));
    QCOMPARE(spy.count(), 1);

    DiagnoseResult result = spy.first().first().value<DiagnoseResult>();
    QVERIFY(!result.steps.isEmpty());
    QVERIFY(!result.suggestion.isEmpty());
}
```

Register metatype at top of test file:
```cpp
Q_DECLARE_METATYPE(DiagnoseResult)
```

And in an `initTestCase`:
```cpp
void TestRepoRepairEngine::initTestCase()
{
    qRegisterMetaType<DiagnoseResult>("DiagnoseResult");
}
```

- [ ] **Step 2: Implement `diagnoseConnection`**

In `linux/nexis-core/Tools/repo_repair_engine.cpp`, replace the stub:

```cpp
static QString httpHeadCheck(const QString &url, int timeoutMs = 5000)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(url));
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = nam.head(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs + 500, &loop, &QEventLoop::quit);
    loop.exec();

    QString error;
    if (reply->error() != QNetworkReply::NoError)
        error = reply->errorString();

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (error.isEmpty() && status >= 400)
        error = QString("HTTP %1").arg(status);

    reply->deleteLater();
    return error; // Empty = success
}

// NOTE: This method is synchronous (blocking network I/O). The caller
// (APTSourceManagerPage) wraps it in QtConcurrent::run since nexis-gui
// links Qt6::Concurrent but nexis-core does not.
void RepoRepairEngineLinux::diagnoseConnection(const APTSourcePtr &source)
{
    DiagnoseResult result;
    QUrl url(source->uri);
    QString domain = url.host();

        // Step 1: DNS
        {
            DiagnoseStep step;
            step.check = QObject::tr("DNS Resolution");
            QDnsLookup dns;
            dns.setType(QDnsLookup::A);
            dns.setName(domain);
            dns.lookup();

            QEventLoop loop;
            QObject::connect(&dns, &QDnsLookup::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            loop.exec();

            if (dns.error() == QDnsLookup::NoError && !dns.hostAddressRecords().isEmpty()) {
                step.status = DiagnoseStep::Ok;
                step.detail = QObject::tr("Resolves to %1").arg(dns.hostAddressRecords().first().value().toString());
            } else if (dns.error() == QDnsLookup::NotFoundError) {
                step.status = DiagnoseStep::Failed;
                step.detail = QObject::tr("Domain does not exist (NXDOMAIN)");
                result.steps.append(step);
                result.suggestion = QObject::tr("This domain no longer exists. The repository may have been discontinued.");
                emit diagnoseFinished(result);
                return;
            } else {
                step.status = DiagnoseStep::Failed;
                step.detail = QObject::tr("DNS lookup failed: %1").arg(dns.errorString());
                result.steps.append(step);
                result.suggestion = QObject::tr("DNS resolution failed. Check your network connection.");
                emit diagnoseFinished(result);
                return;
            }
            result.steps.append(step);
        }

        // Step 2: Base domain
        {
            DiagnoseStep step;
            step.check = QObject::tr("Base Domain");
            QString baseUrl = url.scheme() + "://" + domain;
            QString err = httpHeadCheck(baseUrl);
            if (err.isEmpty()) {
                step.status = DiagnoseStep::Ok;
                step.detail = QObject::tr("Base domain is reachable");

                // Base works but full URI failed — path issue
                result.suggestion = QObject::tr("The server is up but the repository path may have changed. Try opening in a browser to check.");
            } else {
                step.status = DiagnoseStep::Failed;
                step.detail = QObject::tr("Base domain unreachable: %1").arg(err);
                result.suggestion = QObject::tr("The entire server is unreachable. It may be down or blocked.");
            }
            result.steps.append(step);
        }

        // Step 3: Protocol check
        {
            DiagnoseStep step;
            step.check = QObject::tr("Protocol Check");
            QString altScheme = (url.scheme() == "https") ? "http" : "https";
            QString altUrl = altScheme + "://" + domain + url.path();
            QString err = httpHeadCheck(altUrl);
            if (err.isEmpty()) {
                step.status = DiagnoseStep::Warning;
                step.detail = QObject::tr("Available over %1 instead").arg(altScheme.toUpper());
                result.suggestion = QObject::tr("This repository is available over %1. Consider updating the source URL.").arg(altScheme.toUpper());
            } else {
                step.status = DiagnoseStep::Ok;
                step.detail = QObject::tr("Not available over %1 either").arg(altScheme.toUpper());
            }
            result.steps.append(step);
        }

        // Step 4: Knowledge base
        {
            RepoKnownInfo info = RepoKnowledgeBase::lookup(source->uri);
            if (!info.url.isEmpty() && info.url != source->uri) {
                DiagnoseStep step;
                step.check = QObject::tr("Known Repository");
                step.status = DiagnoseStep::Warning;
                step.detail = QObject::tr("Canonical URL: %1").arg(info.url);
                result.suggestion = QObject::tr("The canonical URL for %1 is %2. Your source may be outdated.").arg(info.name, info.url);
                result.steps.append(step);
            }
        }

        // Build follow-up actions
        {
            RepoRepairAction openBrowser;
            openBrowser.type = RepoRepairAction::RunCommand;
            openBrowser.label = QObject::tr("Open in Browser");
#ifdef Q_OS_MACOS
            openBrowser.command = "open " + source->uri;
#else
            openBrowser.command = "xdg-open " + source->uri;
#endif
            result.followUpActions.append(openBrowser);

            RepoRepairAction search;
            search.type = RepoRepairAction::RunCommand;
            search.label = QObject::tr("Search Online");
            QString query = QUrl::toPercentEncoding(domain + " apt repository");
#ifdef Q_OS_MACOS
            search.command = "open https://www.google.com/search?q=" + query;
#else
            search.command = "xdg-open https://www.google.com/search?q=" + query;
#endif
            result.followUpActions.append(search);

            RepoRepairAction disable;
            disable.type = RepoRepairAction::DisableSource;
            disable.label = QObject::tr("Disable Repository");
            result.followUpActions.append(disable);
        }

    emit diagnoseFinished(result);
}
```

- [ ] **Step 3: Run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build -R RepoRepairEngine --output-on-failure`
Expected: All tests pass (diagnose test uses a non-existent domain so DNS should fail quickly).

- [ ] **Step 4: Commit**

```bash
git add linux/nexis-core/Tools/repo_repair_engine.cpp \
        tests/core/test_repo_repair_engine.cpp
git commit -m "feat(repair): implement connection diagnosis with inline results (FR-87)"
```

---

## Task 8: Health Checkers — Populate Actions for All Issue Types

**Files:**
- Modify: `linux/nexis-core/Tools/repo_health_checker.cpp:139-330`
- Modify: `macos/nexis-core/Tools/repo_health_checker.cpp:42-238`

- [ ] **Step 1: Add actions to Linux health checker**

Update each issue creation in `linux/nexis-core/Tools/repo_health_checker.cpp`:

**`checkConnection` (~line 143):** Add `DiagnoseConnection` + `DisableSource` actions.
**`checkReleaseFile` (~line 158):** Add `DiagnoseConnection` + `DisableSource` actions.
**`checkGpgKey` — expired (~line 228):** Fix the GPG refresh command to extract fingerprint first. If fingerprint extraction fails, don't add the action.
**`checkGpgKey` — expiring (~line 240):** Same fix.
**`checkGpgKey` — missing (~line 196):** Add `ConvertToDeb822` action.
**`checkSuiteMismatch` (~line 272):** No actions (informational).
**`checkDeprecatedFormat` — legacy (~line 289):** Add `ConvertToDeb822` action.
**`checkDeprecatedFormat` — no signed-by (~line 297):** Check if `ConvertToDeb822` action already exists in result's issues; if so, suppress to avoid duplicate buttons.
**`checkDuplicates` (~line 318):** Add `RemoveDuplicate` action.

- [ ] **Step 2: Update macOS health checker**

Update `macos/nexis-core/Tools/repo_health_checker.cpp` — the existing `issue.repairCmd`/`issue.repairLabel` assignments should have been migrated in Task 1, but verify they use the `actions` list correctly for `outdated` (brew upgrade), `disabled` (brew uninstall), and `disabled` casks (brew uninstall --cask).

- [ ] **Step 3: Build and run all tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`
Expected: All tests pass. Existing `RepoHealthCheckerTests` still pass with `actions` field.

- [ ] **Step 4: Commit**

```bash
git add linux/nexis-core/Tools/repo_health_checker.cpp \
        macos/nexis-core/Tools/repo_health_checker.cpp
git commit -m "feat(health): populate repair actions for all issue types (FR-87)"
```

---

## Task 9: ToolManager — Wire Up Repair Engine

**Files:**
- Modify: `shared/nexis/Managers/tool_manager.h:11,60,68`
- Modify: `shared/nexis/Managers/tool_manager.cpp:5-34`

- [ ] **Step 1: Add engine to ToolManager**

In `tool_manager.h`, add include:
```cpp
#include <Tools/repo_repair_engine.h>
```

Add accessor (~line 60):
```cpp
RepoRepairEngine *repoRepairEngine() const { return mRepoRepairEngine.get(); }
```

Add member (~line 68):
```cpp
std::unique_ptr<RepoRepairEngine> mRepoRepairEngine;
```

In `tool_manager.cpp`, add platform includes:
```cpp
#ifdef Q_OS_MACOS
#include <Tools/repo_repair_engine_macos.h>
#else
#include <Tools/repo_repair_engine_linux.h>
#endif
```

In constructor, add instantiation:
```cpp
#ifdef Q_OS_MACOS
    mRepoRepairEngine  = std::make_unique<RepoRepairEngineMac>();
#else
    mRepoRepairEngine  = std::make_unique<RepoRepairEngineLinux>();
#endif
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add shared/nexis/Managers/tool_manager.h \
        shared/nexis/Managers/tool_manager.cpp
git commit -m "feat(managers): wire RepoRepairEngine into ToolManager (FR-87)"
```

---

## Task 10: Detail Panel — Action Button Row and Diagnose Expansion

**Files:**
- Modify: `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h:25-28,33`
- Modify: `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp:226-280`
- Modify: `shared/nexis/static/themes/default/style/style.qss:1803`

- [ ] **Step 1: Update signals in header**

In `repo_detail_panel.h`, replace:
```cpp
    void repairRequested(const QString &command, const QString &label);
```
with:
```cpp
    void repairActionRequested(const RepoRepairAction &action, const APTSourcePtr &source);
```

Add method:
```cpp
    void showDiagnoseResult(const DiagnoseResult &result, QVBoxLayout *issueLayout);
```

Add include at top:
```cpp
#include <Tools/repo_repair_engine.h>
```

- [ ] **Step 2: Update `addIssueWidget` to render action button row**

In `repo_detail_panel.cpp`, replace the single-button block (the `if (!issue.actions.isEmpty())` block from Task 1) with:

```cpp
    if (!issue.actions.isEmpty()) {
        QHBoxLayout *actionRow = new QHBoxLayout();
        actionRow->setSpacing(6);

        for (const RepoRepairAction &action : issue.actions) {
            QPushButton *btn = new QPushButton(action.label, issueWidget);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setFixedHeight(26);

            if (action.type == RepoRepairAction::RemoveSource) {
                btn->setObjectName("repoRemoveBtn");
            } else {
                btn->setAccessibleName("primary");
            }

            RepoRepairAction capturedAction = action;
            connect(btn, &QPushButton::clicked, this, [this, capturedAction]() {
                emit repairActionRequested(capturedAction, mCurrentSource);
            });

            actionRow->addWidget(btn);
        }
        actionRow->addStretch();
        issueLayout->addLayout(actionRow);
    }
```

- [ ] **Step 3: Add `showDiagnoseResult` method**

```cpp
void RepoDetailPanel::showDiagnoseResult(const DiagnoseResult &result, QVBoxLayout *issueLayout)
{
    QSettings *sv = AppManager::ins()->getStyleValues();

    for (const DiagnoseStep &step : result.steps) {
        QHBoxLayout *stepRow = new QHBoxLayout();
        stepRow->setSpacing(6);

        QString icon;
        QString iconColor;
        switch (step.status) {
        case DiagnoseStep::Ok:
            icon = QString::fromUtf8("\u2713");
            iconColor = sv ? sv->value("@successColor").toString() : QString();
            break;
        case DiagnoseStep::Warning:
            icon = QString::fromUtf8("\u25B2");
            iconColor = sv ? sv->value("@warningColor").toString() : QString();
            break;
        case DiagnoseStep::Failed:
            icon = QString::fromUtf8("\u2717");
            iconColor = sv ? sv->value("@destructiveColor").toString() : QString();
            break;
        }

        QLabel *lblIcon = new QLabel(icon);
        lblIcon->setStyleSheet(QString("color: %1; font-weight: bold;").arg(iconColor));
        lblIcon->setFixedWidth(16);
        stepRow->addWidget(lblIcon);

        QLabel *lblCheck = new QLabel(QString("<b>%1:</b> %2").arg(step.check, step.detail));
        lblCheck->setObjectName("repoDiagnoseStep");
        lblCheck->setWordWrap(true);
        stepRow->addWidget(lblCheck, 1);

        issueLayout->addLayout(stepRow);
    }

    if (!result.suggestion.isEmpty()) {
        QLabel *lblSuggestion = new QLabel(result.suggestion);
        lblSuggestion->setWordWrap(true);
        QString suggColor = sv ? sv->value("@warningColor").toString() : QString();
        lblSuggestion->setStyleSheet(QString("color: %1; font-style: italic;").arg(suggColor));
        issueLayout->addWidget(lblSuggestion);
    }

    // Follow-up action buttons
    if (!result.followUpActions.isEmpty()) {
        QHBoxLayout *actionRow = new QHBoxLayout();
        actionRow->setSpacing(6);
        for (const RepoRepairAction &action : result.followUpActions) {
            QPushButton *btn = new QPushButton(action.label);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setFixedHeight(26);

            if (action.type == RepoRepairAction::DisableSource)
                btn->setAccessibleName("primary");

            RepoRepairAction capturedAction = action;
            connect(btn, &QPushButton::clicked, this, [this, capturedAction]() {
                emit repairActionRequested(capturedAction, mCurrentSource);
            });
            actionRow->addWidget(btn);
        }
        actionRow->addStretch();
        issueLayout->addLayout(actionRow);
    }
}
```

- [ ] **Step 4: Add QSS rules**

In `shared/nexis/static/themes/default/style/style.qss`, after the `#repoIssueCard` rule, add:

```qss
#repoRemoveBtn {
    color: @destructiveColor;
}

#repoDiagnoseStep {
    color: @color05;
    background-color: transparent;
}
```

- [ ] **Step 5: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/repo_detail_panel.h \
        shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp \
        shared/nexis/static/themes/default/style/style.qss
git commit -m "feat(ui): render action button rows and diagnose results in detail panel (FR-87)"
```

---

## Task 11: Page Integration — Typed Action Dispatch

**Files:**
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h:100-105`
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp:138-151,648-673`

- [ ] **Step 1: Update header**

In `apt_source_manager_page.h`, add forward declarations and slot:

```cpp
class RepoRepairEngine;
struct DiagnoseResult;
```

Add private slot:
```cpp
    void onRepairActionRequested(const RepoRepairAction &action, const APTSourcePtr &source);
    void onDiagnoseFinished(const DiagnoseResult &result);
```

Add member:
```cpp
    bool mDiagnoseRunning = false;
```

- [ ] **Step 2: Replace signal connection and handler**

In `apt_source_manager_page.cpp`, replace the connection (~line 140):
```cpp
connect(mDetailPanel, &RepoDetailPanel::repairRequested,
        this, &APTSourceManagerPage::onRepairRequested);
```
with:
```cpp
connect(mDetailPanel, &RepoDetailPanel::repairActionRequested,
        this, &APTSourceManagerPage::onRepairActionRequested);
```

Replace the `onRepairRequested` method (~lines 648-673) with:

```cpp
void APTSourceManagerPage::onRepairActionRequested(const RepoRepairAction &action, const APTSourcePtr &source)
{
    RepoRepairEngine *engine = mToolManager->repoRepairEngine();

    if (action.type == RepoRepairAction::DiagnoseConnection) {
        if (mDiagnoseRunning) return;
        mDiagnoseRunning = true;
        connect(engine, &RepoRepairEngine::diagnoseFinished,
                this, &APTSourceManagerPage::onDiagnoseFinished, Qt::UniqueConnection);
        // Engine method is synchronous — wrap in QtConcurrent::run
        // (nexis-gui links Qt6::Concurrent, nexis-core does not)
        APTSourcePtr capturedSource = source;
        QtConcurrent::run([engine, capturedSource]() {
            engine->diagnoseConnection(capturedSource);
        });
        return;
    }

    if (action.type == RepoRepairAction::RunCommand) {
        // "Open in Browser" and "Search Online" don't need pkexec
        if (action.command.startsWith("xdg-open") || action.command.startsWith("open")) {
            QStringList args = action.command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (args.size() >= 2)
                QDesktopServices::openUrl(QUrl(args.mid(1).join(' ')));
            return;
        }
    }

    // Confirmation dialog
    QString message;
    if (action.type == RepoRepairAction::RemoveSource) {
        message = tr("This will permanently delete this repository entry.\n\n"
                     "This action cannot be undone.\n\nProceed?");
    } else {
        message = tr("This will modify your system's repository configuration.\n\n"
                     "Action: %1\n\nThis requires administrator privileges. Proceed?")
            .arg(action.label);
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm: %1").arg(action.label),
        message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    RepoRepairEngine::RepairResult result;
    switch (action.type) {
    case RepoRepairAction::RunCommand:
        result = engine->runCommand(action.command);
        break;
    case RepoRepairAction::ConvertToDeb822:
        result = engine->convertToDeb822(source);
        break;
    case RepoRepairAction::RemoveDuplicate:
        result = engine->removeDuplicate(source);
        break;
    case RepoRepairAction::DisableSource:
        result = engine->disableSource(source);
        break;
    case RepoRepairAction::EnableSource:
        result = engine->enableSource(source);
        break;
    case RepoRepairAction::RemoveSource:
        result = engine->removeSource(source);
        break;
    default:
        return;
    }

    if (result.success) {
        loadAptSources();
        mRefresh->triggerRepoHealthCheck();
    } else if (!result.errorDetail.isEmpty()) {
        QMessageBox::warning(this, tr("Action Failed"),
            tr("%1\n\n%2").arg(result.message, result.errorDetail));
    }
}

void APTSourceManagerPage::onDiagnoseFinished(const DiagnoseResult &result)
{
    mDiagnoseRunning = false;
    // Store the diagnose result and re-render the detail panel with it
    mLastDiagnoseResult = result;
    mHasDiagnoseResult = true;
    if (mDetailPanel->isVisible() && selectedAptSource) {
        QString key = RepoHealthChecker::cacheKey(selectedAptSource);
        if (mHealthCache.contains(key))
            mDetailPanel->showRepo(selectedAptSource, mHealthCache[key], &result);
    }
}
```

This requires `showRepo` to accept an optional `DiagnoseResult*` parameter (nullptr by default). When non-null, the detail panel calls `showDiagnoseResult` after the connection_error issue card. Update the `showRepo` signature in `repo_detail_panel.h`:

```cpp
void showRepo(const APTSourcePtr &source, const RepoHealthResult &result,
              const DiagnoseResult *diagnoseResult = nullptr);
```

And in `showRepo`, after the `addIssueWidget` loop:

```cpp
if (diagnoseResult) {
    // Find the connection_error issue card and append results
    // Or add as a separate section below the issues
    showDiagnoseResult(*diagnoseResult, mIssuesLayout);
}
```

- [ ] **Step 3: Remove old `onRepairRequested` method**

Delete the old `onRepairRequested(const QString &command, const QString &label)` declaration from the header and implementation.

- [ ] **Step 4: Build and run all tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`
Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h \
        shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp
git commit -m "feat(page): implement typed repair action dispatch handler (FR-87)"
```

---

## Task 12: Final Integration — Clean Build, Full Tests, Manual Verification

**Files:**
- All files from previous tasks

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
Expected: Clean build succeeds with no warnings related to repair engine.

- [ ] **Step 2: Run all tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass (26+).

- [ ] **Step 3: Manual verification**

Launch the app: `./build/output/nexis`
1. Navigate to APT Repository Manager
2. Click "Refresh Health" to trigger health checks
3. Click a repo with issues — verify action buttons appear in the detail panel
4. Test "Diagnose" on an unreachable repo — verify inline results expand
5. Test "Disable" on a repo — verify it gets commented out and re-check shows it disabled
6. Test "Enable" on a disabled repo — verify it gets re-enabled
7. Test "Remove" on a disabled repo — verify confirmation dialog with warning, then deletion
8. Test "Convert to deb822" on a legacy source — verify new .sources file created and old .list commented

- [ ] **Step 4: Update tracking files**

In `FEATURE_REQUESTS.md`, update FR-87 entry to note the repair actions extension is complete.

- [ ] **Step 5: Commit**

```bash
git add FEATURE_REQUESTS.md
git commit -m "feat(repair): complete repo repair actions implementation (FR-87)"
```
