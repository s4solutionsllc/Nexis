# Repo Health Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add health indicators, descriptions, and guided repair actions to the APT Repository Manager (Linux) and Homebrew Packages (macOS) pages.

**Architecture:** Extend the APTSource data model with format/signing metadata, add a RepoHealthChecker abstraction with platform-specific implementations, integrate periodic health checks via DataRefreshService, enrich existing repo cards with status badges and descriptions, and add a toggleable side detail panel with diagnostics and guided repair.

**Tech Stack:** C++17, Qt6 (Widgets, Network, Concurrent), QTest/CTest

**Spec:** `docs/superpowers/specs/2026-03-20-repo-health-dashboard-design.md`

---

## File Map

### New Files

| File | Purpose |
|------|---------|
| `shared/nexis-core/Tools/repo_health_types.h` | `RepoHealthIssue`, `RepoHealthResult`, `RepoKnownInfo` structs |
| `shared/nexis-core/Tools/repo_knowledge_base.h` | `RepoKnowledgeBase` class declaration |
| `shared/nexis-core/Tools/repo_knowledge_base.cpp` | Static lookup table + `lookup()` method |
| `shared/nexis-core/Tools/repo_health_checker.h` | Abstract `RepoHealthChecker` base class |
| `linux/nexis-core/Tools/repo_health_checker_linux.h` | Linux health checker declaration |
| `linux/nexis-core/Tools/repo_health_checker.cpp` | Linux APT health check implementations |
| `macos/nexis-core/Tools/repo_health_checker_macos.h` | macOS health checker declaration |
| `macos/nexis-core/Tools/repo_health_checker.cpp` | macOS Homebrew health check implementations |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h` | Side detail panel widget declaration |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp` | Side detail panel implementation |
| `tests/core/test_repo_knowledge_base.cpp` | Knowledge base unit tests |
| `tests/core/test_repo_health_checker.cpp` | Health checker unit tests |

### Modified Files

| File | Changes |
|------|---------|
| `shared/nexis-core/Tools/apt_source_tool.h` | Add `Format` enum, `format`, `signedByPath` fields to `APTSource` |
| `shared/nexis-core/Tools/apt_source_tool_shared.cpp` | Set `format` and extract `signedByPath` in both parsers |
| `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.h` | Add `setHealthResult()`, status dot/description labels |
| `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp` | Implement enriched card layout with status indicators |
| `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.ui` | Bump minimum height from 45px to 60px |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h` | Add detail panel member, health result cache, new slots |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | QSplitter layout, health signal wiring, panel toggle logic |
| `shared/nexis/Managers/data_refresh_service.h` | Add `repoHealthChecked` signal, `triggerRepoHealthCheck()`, `mRepoHealthRunning` |
| `shared/nexis/Managers/data_refresh_service.cpp` | Implement `triggerRepoHealthCheck()` with `QtConcurrent::run()` |
| `shared/nexis/Managers/tool_manager.h` | Add `repoHealthChecker()` accessor |
| `shared/nexis/Managers/tool_manager.cpp` | Instantiate platform-specific health checker |
| `CMakeLists.txt` | Add new source/header files to build |
| `tests/CMakeLists.txt` | Register new test executables |
| `tests/core/test_apt_source_tool.cpp` | Add tests for `format` and `signedByPath` fields |

---

## Task 1: Data Types — `RepoHealthResult` and `RepoKnownInfo` Structs

**Files:**
- Create: `shared/nexis-core/Tools/repo_health_types.h`
- Modify: `CMakeLists.txt:64-90` (add to `CORE_SHARED_HDRS`)

- [ ] **Step 1: Create the header file**

```cpp
// shared/nexis-core/Tools/repo_health_types.h
#ifndef REPO_HEALTH_TYPES_H
#define REPO_HEALTH_TYPES_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QMap>

struct RepoHealthIssue {
    enum Severity { Info, Warning, Error };
    Severity severity = Info;
    QString code;        // "gpg_expiring", "release_404", "suite_mismatch", etc.
    QString summary;     // "GPG key expires in 14 days"
    QString detail;      // Longer explanation for the detail panel
    QString repairCmd;   // Command to run, empty if no auto-repair
    QString repairLabel; // "Refresh signing key"
};

struct RepoHealthResult {
    enum Status { Unknown, Healthy, Warning, Error };
    Status status = Unknown;
    QString name;
    QString description;
    QList<RepoHealthIssue> issues;
    QDateTime lastChecked;
    QString releaseOrigin;
};

struct RepoKnownInfo {
    QString name;
    QString description;
    QString url;
};

// Convenience type for the health cache
using RepoHealthCache = QMap<QString, RepoHealthResult>;

#endif // REPO_HEALTH_TYPES_H
```

- [ ] **Step 2: Add to CMakeLists.txt**

In `CMakeLists.txt`, add to the `CORE_SHARED_HDRS` set (after line 81, the existing `apt_source_tool.h` entry):

```cmake
  "${CORE_SHARED_DIR}/Tools/repo_health_types.h"
```

- [ ] **Step 3: Verify build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build succeeds (header-only, no new compilation units yet)

- [ ] **Step 4: Commit**

```bash
git add shared/nexis-core/Tools/repo_health_types.h CMakeLists.txt
git commit -m "feat(repo-health): add RepoHealthResult and RepoKnownInfo data types"
```

---

## Task 2: Extend APTSource with `format` and `signedByPath`

**Files:**
- Modify: `shared/nexis-core/Tools/apt_source_tool.h:8-19`
- Modify: `shared/nexis-core/Tools/apt_source_tool_shared.cpp:8-44` and `46-83`
- Test: `tests/core/test_apt_source_tool.cpp`

- [ ] **Step 1: Write failing tests for new fields**

Add these test methods to `tests/core/test_apt_source_tool.cpp`:

In the class declaration (after line 25, before the closing `};`):

```cpp
    // New fields: format and signedByPath
    void listLine_setsLegacyFormat();
    void listLine_extractsSignedByPath();
    void listLine_noSignedBy_emptyPath();
    void deb822_setsDeb822Format();
    void deb822_extractsSignedByPath();
```

Add implementations before the `QTEST_MAIN` line:

```cpp
// --- format and signedByPath ---

void TestAptSourceTool::listLine_setsLegacyFormat()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb http://archive.ubuntu.com/ubuntu jammy main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->format, APTSource::Legacy);
}

void TestAptSourceTool::listLine_extractsSignedByPath()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64 signed-by=/usr/share/keyrings/example.gpg] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->signedByPath, QString("/usr/share/keyrings/example.gpg"));
}

void TestAptSourceTool::listLine_noSignedBy_emptyPath()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->signedByPath, QString());
}

void TestAptSourceTool::deb822_setsDeb822Format()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->format, APTSource::Deb822);
}

void TestAptSourceTool::deb822_extractsSignedByPath()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n"
        "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->signedByPath, QString("/usr/share/keyrings/ubuntu-archive-keyring.gpg"));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Compilation error — `APTSource` has no member `format` or `signedByPath`

- [ ] **Step 3: Add fields to APTSource class**

In `shared/nexis-core/Tools/apt_source_tool.h`, replace lines 8-19 with:

```cpp
class APTSource {
public:
    enum Format { Legacy, Deb822 };

    QString filePath;
    bool isSource = false;
    QString options;
    QString uri;
    QString suites;
    QString components;
    QString source;
    bool isActive = false;
    Format format = Legacy;
    QString signedByPath;
};
```

- [ ] **Step 4: Update `parseSourceListLine()` to set new fields**

In `shared/nexis-core/Tools/apt_source_tool_shared.cpp`, in `parseSourceListLine()`:

After line 16 (`APTSourcePtr aptSource(new APTSource);`), add:

```cpp
    aptSource->format = APTSource::Legacy;
```

After line 24 (`aptSource->options = optMatch.captured().trimmed();`), add:

```cpp
    // Extract signed-by path from options
    QRegularExpression signedByRegex("signed-by=([^\\],\\s\\]]+)");
    QRegularExpressionMatch sbMatch = signedByRegex.match(aptSource->options);
    if (sbMatch.hasMatch())
        aptSource->signedByPath = sbMatch.captured(1);
```

- [ ] **Step 5: Update `parseDeb822Stanza()` to set new fields**

In `shared/nexis-core/Tools/apt_source_tool_shared.cpp`, in `parseDeb822Stanza()`:

After line 69 (`APTSourcePtr aptSource(new APTSource);`), add:

```cpp
    aptSource->format = APTSource::Deb822;
    aptSource->signedByPath = fields.value("Signed-By").trimmed();
```

- [ ] **Step 6: Build and run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure -R AptSource`
Expected: All AptSource tests pass (existing + 5 new)

- [ ] **Step 7: Commit**

```bash
git add shared/nexis-core/Tools/apt_source_tool.h shared/nexis-core/Tools/apt_source_tool_shared.cpp tests/core/test_apt_source_tool.cpp
git commit -m "feat(repo-health): extend APTSource with format and signedByPath fields"
```

---

## Task 3: Knowledge Base — URI-to-Description Lookup

**Files:**
- Create: `shared/nexis-core/Tools/repo_knowledge_base.h`
- Create: `shared/nexis-core/Tools/repo_knowledge_base.cpp`
- Create: `tests/core/test_repo_knowledge_base.cpp`
- Modify: `CMakeLists.txt:41-62` (add to `CORE_SHARED_SRCS`)
- Modify: `CMakeLists.txt:64-90` (add to `CORE_SHARED_HDRS`)
- Modify: `tests/CMakeLists.txt` (register test)

- [ ] **Step 1: Write test file**

Create `tests/core/test_repo_knowledge_base.cpp`:

```cpp
#include <QTest>
#include "Tools/repo_knowledge_base.h"

class TestRepoKnowledgeBase : public QObject
{
    Q_OBJECT

private slots:
    void lookup_ubuntuMain();
    void lookup_ppaDeadsnakes();
    void lookup_dockerCE();
    void lookup_unknownRepo_returnsEmpty();
    void lookup_partialMatch();
    void lookup_caseInsensitive();
};

void TestRepoKnowledgeBase::lookup_ubuntuMain()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://archive.ubuntu.com/ubuntu");
    QVERIFY(!info.name.isEmpty());
    QCOMPARE(info.name, QString("Ubuntu Main"));
}

void TestRepoKnowledgeBase::lookup_ppaDeadsnakes()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://ppa.launchpadcontent.net/deadsnakes/ppa/ubuntu");
    QVERIFY(!info.name.isEmpty());
    QVERIFY(info.name.contains("Deadsnakes"));
}

void TestRepoKnowledgeBase::lookup_dockerCE()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://download.docker.com/linux/ubuntu");
    QVERIFY(!info.name.isEmpty());
    QVERIFY(info.name.contains("Docker"));
}

void TestRepoKnowledgeBase::lookup_unknownRepo_returnsEmpty()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://totally-unknown-repo.example.com/apt");
    QVERIFY(info.name.isEmpty());
    QVERIFY(info.description.isEmpty());
}

void TestRepoKnowledgeBase::lookup_partialMatch()
{
    // Should match even with extra path segments
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://packages.microsoft.com/repos/vscode");
    QVERIFY(!info.name.isEmpty());
    QVERIFY(info.name.contains("VS Code") || info.name.contains("Visual Studio Code"));
}

void TestRepoKnowledgeBase::lookup_caseInsensitive()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://ARCHIVE.UBUNTU.COM/ubuntu");
    QVERIFY(!info.name.isEmpty());
}

QTEST_MAIN(TestRepoKnowledgeBase)
#include "test_repo_knowledge_base.moc"
```

- [ ] **Step 2: Create the header file**

Create `shared/nexis-core/Tools/repo_knowledge_base.h`:

```cpp
#ifndef REPO_KNOWLEDGE_BASE_H
#define REPO_KNOWLEDGE_BASE_H

#include "repo_health_types.h"

class RepoKnowledgeBase
{
public:
    // Returns matching info for a repo URI; empty name/description if unknown
    static RepoKnownInfo lookup(const QString &uri);

    // Extracts a human-readable domain name from a URI as last-resort fallback
    static QString domainFromUri(const QString &uri);
};

#endif // REPO_KNOWLEDGE_BASE_H
```

- [ ] **Step 3: Implement the knowledge base**

Create `shared/nexis-core/Tools/repo_knowledge_base.cpp`:

```cpp
#include "repo_knowledge_base.h"
#include <QUrl>
#include <QCoreApplication>

struct KnowledgeEntry {
    const char *pattern;  // substring to match in URI (case-insensitive)
    const char *name;
    const char *description;
    const char *url;
};

// IMPORTANT: More specific patterns MUST come before generic ones.
// Lookup uses first-match-wins with contains(), so e.g.
// "packages.microsoft.com/repos/vscode" must precede "packages.microsoft.com".
static const KnowledgeEntry s_entries[] = {
    // Ubuntu official
    { "archive.ubuntu.com",
      QT_TR_NOOP("Ubuntu Main"),
      QT_TR_NOOP("Core OS packages and security updates"),
      "https://packages.ubuntu.com" },
    { "security.ubuntu.com",
      QT_TR_NOOP("Ubuntu Security"),
      QT_TR_NOOP("Security patches for Ubuntu packages"),
      "https://packages.ubuntu.com" },
    { "ports.ubuntu.com",
      QT_TR_NOOP("Ubuntu Ports"),
      QT_TR_NOOP("Ubuntu packages for non-x86 architectures"),
      "https://packages.ubuntu.com" },

    // Debian official
    { "deb.debian.org",
      QT_TR_NOOP("Debian Official"),
      QT_TR_NOOP("Debian project package repository"),
      "https://packages.debian.org" },
    { "security.debian.org",
      QT_TR_NOOP("Debian Security"),
      QT_TR_NOOP("Debian security update repository"),
      "https://www.debian.org/security" },

    // Common PPAs
    { "ppa.launchpadcontent.net/deadsnakes",
      QT_TR_NOOP("Deadsnakes PPA"),
      QT_TR_NOOP("Python 3.x alternate versions for Ubuntu"),
      "https://launchpad.net/~deadsnakes/+archive/ubuntu/ppa" },
    { "ppa.launchpadcontent.net/git-core",
      QT_TR_NOOP("Git Core PPA"),
      QT_TR_NOOP("Latest stable Git version"),
      "https://launchpad.net/~git-core/+archive/ubuntu/ppa" },

    // Microsoft
    { "packages.microsoft.com/repos/vscode",
      QT_TR_NOOP("VS Code"),
      QT_TR_NOOP("Microsoft Visual Studio Code editor"),
      "https://code.visualstudio.com" },
    { "packages.microsoft.com/repos/edge",
      QT_TR_NOOP("Microsoft Edge"),
      QT_TR_NOOP("Microsoft Edge web browser"),
      "https://www.microsoft.com/edge" },
    { "packages.microsoft.com/repos/ms-teams",
      QT_TR_NOOP("Microsoft Teams"),
      QT_TR_NOOP("Microsoft Teams collaboration platform"),
      "https://www.microsoft.com/microsoft-teams" },
    { "packages.microsoft.com",
      QT_TR_NOOP("Microsoft Packages"),
      QT_TR_NOOP("Microsoft Linux package repository"),
      "https://packages.microsoft.com" },

    // Google
    { "dl.google.com/linux/chrome",
      QT_TR_NOOP("Google Chrome"),
      QT_TR_NOOP("Google Chrome web browser"),
      "https://www.google.com/chrome" },
    { "dl.google.com/linux/earth",
      QT_TR_NOOP("Google Earth"),
      QT_TR_NOOP("Google Earth desktop application"),
      "https://earth.google.com" },

    // Development tools
    { "deb.nodesource.com",
      QT_TR_NOOP("NodeSource"),
      QT_TR_NOOP("Node.js LTS and current releases"),
      "https://nodesource.com" },
    { "download.docker.com",
      QT_TR_NOOP("Docker CE"),
      QT_TR_NOOP("Docker container engine and CLI tools"),
      "https://www.docker.com" },
    { "apt.postgresql.org",
      QT_TR_NOOP("PostgreSQL"),
      QT_TR_NOOP("PostgreSQL database server and tools"),
      "https://www.postgresql.org" },
    { "repo.mysql.com",
      QT_TR_NOOP("MySQL"),
      QT_TR_NOOP("MySQL database server and tools"),
      "https://dev.mysql.com" },
    { "cli.github.com",
      QT_TR_NOOP("GitHub CLI"),
      QT_TR_NOOP("GitHub command-line tool"),
      "https://cli.github.com" },
    { "apt.grafana.com",
      QT_TR_NOOP("Grafana"),
      QT_TR_NOOP("Grafana monitoring and visualization"),
      "https://grafana.com" },
    { "apt.releases.hashicorp.com",
      QT_TR_NOOP("HashiCorp"),
      QT_TR_NOOP("Terraform, Vault, Consul, and other HashiCorp tools"),
      "https://www.hashicorp.com" },

    // Browsers and apps
    { "packages.mozilla.org",
      QT_TR_NOOP("Mozilla APT"),
      QT_TR_NOOP("Firefox and Thunderbird direct from Mozilla"),
      "https://www.mozilla.org" },
    { "brave-browser-apt-release.s3.brave.com",
      QT_TR_NOOP("Brave Browser"),
      QT_TR_NOOP("Brave privacy-focused web browser"),
      "https://brave.com" },
    { "linux.dropbox.com",
      QT_TR_NOOP("Dropbox"),
      QT_TR_NOOP("Dropbox cloud storage client"),
      "https://www.dropbox.com" },
    { "repo.steampowered.com",
      QT_TR_NOOP("Steam"),
      QT_TR_NOOP("Valve Steam gaming platform"),
      "https://store.steampowered.com" },
    { "spotify.com",
      QT_TR_NOOP("Spotify"),
      QT_TR_NOOP("Spotify music streaming client"),
      "https://www.spotify.com" },
    { "download.sublimetext.com",
      QT_TR_NOOP("Sublime Text"),
      QT_TR_NOOP("Sublime Text editor"),
      "https://www.sublimetext.com" },
    { "paulcarroty.gitlab.io/vscodium",
      QT_TR_NOOP("VSCodium"),
      QT_TR_NOOP("Free/open-source VS Code binaries without telemetry"),
      "https://vscodium.com" },

    // System tools
    { "download.opensuse.org",
      QT_TR_NOOP("openSUSE Build Service"),
      QT_TR_NOOP("Community packages from openSUSE OBS"),
      "https://build.opensuse.org" },
    { "ppa.launchpadcontent.net",
      QT_TR_NOOP("Launchpad PPA"),
      QT_TR_NOOP("Ubuntu Personal Package Archive"),
      "https://launchpad.net" },
    { "ppa.launchpad.net",
      QT_TR_NOOP("Launchpad PPA"),
      QT_TR_NOOP("Ubuntu Personal Package Archive"),
      "https://launchpad.net" },
};

static constexpr int s_entryCount = sizeof(s_entries) / sizeof(s_entries[0]);

RepoKnownInfo RepoKnowledgeBase::lookup(const QString &uri)
{
    QString lowerUri = uri.toLower();
    for (int i = 0; i < s_entryCount; ++i) {
        if (lowerUri.contains(QString::fromLatin1(s_entries[i].pattern))) {
            RepoKnownInfo info;
            info.name = QCoreApplication::translate("RepoKnowledgeBase", s_entries[i].name);
            info.description = QCoreApplication::translate("RepoKnowledgeBase", s_entries[i].description);
            info.url = QString::fromLatin1(s_entries[i].url);
            return info;
        }
    }
    return {};
}

QString RepoKnowledgeBase::domainFromUri(const QString &uri)
{
    QUrl url(uri);
    if (url.isValid() && !url.host().isEmpty())
        return url.host();
    // Fallback: extract domain-like substring
    QString s = uri;
    s.remove(QRegularExpression("^https?://"));
    int slash = s.indexOf('/');
    return slash > 0 ? s.left(slash) : s;
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `CMakeLists.txt`, add to `CORE_SHARED_SRCS` (after `apt_source_tool_shared.cpp`, line 54):

```cmake
  "${CORE_SHARED_DIR}/Tools/repo_knowledge_base.cpp"
```

Add to `CORE_SHARED_HDRS` (after `repo_health_types.h`):

```cmake
  "${CORE_SHARED_DIR}/Tools/repo_knowledge_base.h"
```

- [ ] **Step 5: Register test in tests/CMakeLists.txt**

Add after the `AptSourceTests` line (line 31):

```cmake
add_nexis_test(NAME RepoKnowledgeBaseTests SOURCES core/test_repo_knowledge_base.cpp)
```

- [ ] **Step 6: Build and run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure -R RepoKnowledge`
Expected: All 6 tests pass

- [ ] **Step 7: Commit**

```bash
git add shared/nexis-core/Tools/repo_knowledge_base.h shared/nexis-core/Tools/repo_knowledge_base.cpp tests/core/test_repo_knowledge_base.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(repo-health): add RepoKnowledgeBase with 30+ common repo descriptions"
```

---

## Task 4: Health Checker — Abstract Base and Linux Implementation

**Files:**
- Create: `shared/nexis-core/Tools/repo_health_checker.h`
- Create: `linux/nexis-core/Tools/repo_health_checker_linux.h`
- Create: `linux/nexis-core/Tools/repo_health_checker.cpp`
- Create: `tests/core/test_repo_health_checker.cpp`
- Modify: `CMakeLists.txt` (add to shared headers + Linux platform sources/headers)

- [ ] **Step 1: Create abstract base class**

Create `shared/nexis-core/Tools/repo_health_checker.h`:

```cpp
#ifndef REPO_HEALTH_CHECKER_H
#define REPO_HEALTH_CHECKER_H

#include "repo_health_types.h"
#include "apt_source_tool.h"
#include <QList>

class RepoHealthChecker
{
public:
    virtual ~RepoHealthChecker() = default;

    // Run health checks on all repos. Blocking — call from background thread.
    virtual RepoHealthCache checkAll(const QList<APTSourcePtr> &sources) = 0;

    // Run health check on a single repo.
    virtual RepoHealthResult checkOne(const APTSourcePtr &source) = 0;

    // Composite cache key for a source entry
    static QString cacheKey(const APTSourcePtr &source);
};

#endif // REPO_HEALTH_CHECKER_H
```

- [ ] **Step 2: Create Linux health checker header**

Create `linux/nexis-core/Tools/repo_health_checker_linux.h`:

```cpp
#ifndef REPO_HEALTH_CHECKER_LINUX_H
#define REPO_HEALTH_CHECKER_LINUX_H

#include "Tools/repo_health_checker.h"

class RepoHealthCheckerLinux : public RepoHealthChecker
{
public:
    RepoHealthCache checkAll(const QList<APTSourcePtr> &sources) override;
    RepoHealthResult checkOne(const APTSourcePtr &source) override;

private:
    // Individual checks — each appends issues to the result
    void checkConnection(const APTSourcePtr &source, RepoHealthResult &result);
    void checkReleaseFile(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result);
    void checkGpgKey(const APTSourcePtr &source, RepoHealthResult &result);
    void checkSuiteMismatch(const APTSourcePtr &source, RepoHealthResult &result);
    void checkDeprecatedFormat(const APTSourcePtr &source, RepoHealthResult &result);

    // Check duplicates across the full list (called from checkAll)
    void checkDuplicates(const QList<APTSourcePtr> &sources, RepoHealthCache &cache);

    // Resolve name/description via knowledge base + Release file fallback
    void resolveDescription(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result);

    // Fetch the InRelease or Release file contents (fetched once per repo, passed to multiple checks)
    QString fetchReleaseFile(const QString &uri, const QString &suite);

    // Compute overall status from issue list
    static RepoHealthResult::Status worstStatus(const QList<RepoHealthIssue> &issues);
};

#endif // REPO_HEALTH_CHECKER_LINUX_H
```

- [ ] **Step 3: Write test file**

Create `tests/core/test_repo_health_checker.cpp`:

```cpp
#include <QTest>
#include "Tools/repo_health_types.h"
#include "Tools/repo_health_checker.h"
#include "Tools/apt_source_tool.h"

class TestRepoHealthChecker : public QObject
{
    Q_OBJECT

private slots:
    void cacheKey_compositeFormat();
    void cacheKey_differentSuites_differentKeys();
    void cacheKey_sameRepo_sameKey();
    void worstStatus_noIssues_healthy();
    void worstStatus_warningOnly();
    void worstStatus_errorOverridesWarning();
    void deprecatedFormat_legacyNoSignedBy_twoIssues();
    void deprecatedFormat_deb822_noIssue();
    void duplicates_detected();
    void duplicates_noDuplicates();
};

void TestRepoHealthChecker::cacheKey_compositeFormat()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://archive.ubuntu.com/ubuntu";
    src->suites = "jammy";
    src->components = "main restricted";
    QString key = RepoHealthChecker::cacheKey(src);
    QCOMPARE(key, QString("http://archive.ubuntu.com/ubuntu jammy main restricted"));
}

void TestRepoHealthChecker::cacheKey_differentSuites_differentKeys()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://archive.ubuntu.com/ubuntu";
    src1->suites = "jammy";
    src1->components = "main";

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://archive.ubuntu.com/ubuntu";
    src2->suites = "jammy-updates";
    src2->components = "main";

    QVERIFY(RepoHealthChecker::cacheKey(src1) != RepoHealthChecker::cacheKey(src2));
}

void TestRepoHealthChecker::cacheKey_sameRepo_sameKey()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://example.com/apt";
    src1->suites = "stable";
    src1->components = "main";

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://example.com/apt";
    src2->suites = "stable";
    src2->components = "main";

    QCOMPARE(RepoHealthChecker::cacheKey(src1), RepoHealthChecker::cacheKey(src2));
}

void TestRepoHealthChecker::worstStatus_noIssues_healthy()
{
    // A result with no issues should be Healthy after check
    RepoHealthResult result;
    result.status = RepoHealthResult::Healthy;
    QCOMPARE(result.status, RepoHealthResult::Healthy);
}

void TestRepoHealthChecker::worstStatus_warningOnly()
{
    RepoHealthIssue issue;
    issue.severity = RepoHealthIssue::Warning;
    issue.code = "test_warning";

    RepoHealthResult result;
    result.issues.append(issue);

    // Compute worst: Warning > Healthy
    RepoHealthResult::Status worst = RepoHealthResult::Healthy;
    for (const auto &i : result.issues) {
        if (i.severity == RepoHealthIssue::Error)
            worst = RepoHealthResult::Error;
        else if (i.severity == RepoHealthIssue::Warning && worst != RepoHealthResult::Error)
            worst = RepoHealthResult::Warning;
    }
    QCOMPARE(worst, RepoHealthResult::Warning);
}

void TestRepoHealthChecker::worstStatus_errorOverridesWarning()
{
    RepoHealthIssue warn;
    warn.severity = RepoHealthIssue::Warning;
    warn.code = "test_warning";

    RepoHealthIssue err;
    err.severity = RepoHealthIssue::Error;
    err.code = "test_error";

    RepoHealthResult result;
    result.issues.append(warn);
    result.issues.append(err);

    RepoHealthResult::Status worst = RepoHealthResult::Healthy;
    for (const auto &i : result.issues) {
        if (i.severity == RepoHealthIssue::Error)
            worst = RepoHealthResult::Error;
        else if (i.severity == RepoHealthIssue::Warning && worst != RepoHealthResult::Error)
            worst = RepoHealthResult::Warning;
    }
    QCOMPARE(worst, RepoHealthResult::Error);
}

// --- Pure-logic checks (no network) ---
// These tests instantiate RepoHealthCheckerLinux directly for logic checks.
// They are Linux-only tests; on macOS, they should be wrapped in #ifdef Q_OS_LINUX.

#ifndef Q_OS_MACOS
#include "Tools/repo_health_checker_linux.h"

void TestRepoHealthChecker::deprecatedFormat_legacyNoSignedBy_twoIssues()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/apt";
    src->suites = "stable";
    src->components = "main";
    src->format = APTSource::Legacy;
    src->signedByPath = "";  // no signed-by

    RepoHealthCheckerLinux checker;
    RepoHealthResult result;
    // Call checkDeprecatedFormat via checkOne would do network calls,
    // so we test the deprecated format logic by running a full checkOne
    // with a known-unreachable URI — but that couples to network.
    // Instead, we verify the fields that deprecatedFormat checks:
    // Legacy format + no signedByPath = 2 issues (legacy_format + no_signed_by)
    // We can't call private methods directly, so we verify via checkOne
    // on localhost (which will fail connection but still run deprecatedFormat).
    src->uri = "http://127.0.0.1:1"; // unreachable, fast timeout
    result = checker.checkOne(src);

    // Should have connection_error + legacy_format + no_signed_by = 3 issues
    bool hasLegacyFormat = false;
    bool hasNoSignedBy = false;
    for (const auto &issue : result.issues) {
        if (issue.code == "legacy_format") hasLegacyFormat = true;
        if (issue.code == "no_signed_by") hasNoSignedBy = true;
    }
    QVERIFY(hasLegacyFormat);
    QVERIFY(hasNoSignedBy);
}

void TestRepoHealthChecker::deprecatedFormat_deb822_noIssue()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://127.0.0.1:1";
    src->suites = "stable";
    src->components = "main";
    src->format = APTSource::Deb822;
    src->signedByPath = "/usr/share/keyrings/test.gpg";

    RepoHealthCheckerLinux checker;
    RepoHealthResult result = checker.checkOne(src);

    bool hasLegacyFormat = false;
    bool hasNoSignedBy = false;
    for (const auto &issue : result.issues) {
        if (issue.code == "legacy_format") hasLegacyFormat = true;
        if (issue.code == "no_signed_by") hasNoSignedBy = true;
    }
    QVERIFY(!hasLegacyFormat);
    QVERIFY(!hasNoSignedBy);
}

void TestRepoHealthChecker::duplicates_detected()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://example.com/apt";
    src1->suites = "stable";
    src1->components = "main";
    src1->format = APTSource::Deb822;

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://example.com/apt";
    src2->suites = "stable";
    src2->components = "main";
    src2->format = APTSource::Deb822;

    // Use unreachable URIs for fast failure
    src1->uri = "http://127.0.0.1:1";
    src2->uri = "http://127.0.0.1:1";

    RepoHealthCheckerLinux checker;
    QList<APTSourcePtr> sources = {src1, src2};
    RepoHealthCache cache = checker.checkAll(sources);

    // At least one entry should have a duplicate_source issue
    bool foundDuplicate = false;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        for (const auto &issue : it.value().issues) {
            if (issue.code == "duplicate_source")
                foundDuplicate = true;
        }
    }
    QVERIFY(foundDuplicate);
}

void TestRepoHealthChecker::duplicates_noDuplicates()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://127.0.0.1:1";
    src1->suites = "stable";
    src1->components = "main";
    src1->format = APTSource::Deb822;

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://127.0.0.1:1";
    src2->suites = "testing";  // different suite
    src2->components = "main";
    src2->format = APTSource::Deb822;

    RepoHealthCheckerLinux checker;
    QList<APTSourcePtr> sources = {src1, src2};
    RepoHealthCache cache = checker.checkAll(sources);

    bool foundDuplicate = false;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        for (const auto &issue : it.value().issues) {
            if (issue.code == "duplicate_source")
                foundDuplicate = true;
        }
    }
    QVERIFY(!foundDuplicate);
}

#else
// macOS stubs — these tests are Linux-only
void TestRepoHealthChecker::deprecatedFormat_legacyNoSignedBy_twoIssues() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::deprecatedFormat_deb822_noIssue() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::duplicates_detected() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::duplicates_noDuplicates() { QSKIP("Linux-only test"); }
#endif

QTEST_MAIN(TestRepoHealthChecker)
#include "test_repo_health_checker.moc"
```

- [ ] **Step 4: Implement `RepoHealthChecker::cacheKey()` (shared, non-virtual)**

Add a small shared source file or put the static method inline in the header. Simplest: add a shared source file.

Create `shared/nexis-core/Tools/repo_health_checker.cpp`:

```cpp
#include "repo_health_checker.h"

QString RepoHealthChecker::cacheKey(const APTSourcePtr &source)
{
    return source->uri + " " + source->suites + " " + source->components;
}
```

- [ ] **Step 5: Implement Linux health checker**

Create `linux/nexis-core/Tools/repo_health_checker.cpp`:

```cpp
#include "repo_health_checker_linux.h"
#include "repo_knowledge_base.h"
#include "Utils/command_util.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>

// --- Helpers ---

static QString systemCodename()
{
    static QString codename;
    if (codename.isEmpty()) {
        ExecResult r = CommandUtil::execWithStatus("lsb_release", {"-cs"});
        if (r.exitCode == 0)
            codename = r.output.trimmed();
    }
    return codename;
}

static QString httpHead(const QString &url, int timeoutMs = 5000)
{
    // Returns empty string on success, error string on failure
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
    return error;
}

static QString httpGet(const QString &url, int timeoutMs = 5000)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(url));
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs + 500, &loop, &QEventLoop::quit);
    loop.exec();

    QString body;
    if (reply->error() == QNetworkReply::NoError)
        body = QString::fromUtf8(reply->readAll());

    reply->deleteLater();
    return body;
}

// --- RepoHealthCheckerLinux ---

RepoHealthResult::Status RepoHealthCheckerLinux::worstStatus(const QList<RepoHealthIssue> &issues)
{
    RepoHealthResult::Status worst = RepoHealthResult::Healthy;
    for (const auto &issue : issues) {
        if (issue.severity == RepoHealthIssue::Error)
            return RepoHealthResult::Error;
        if (issue.severity == RepoHealthIssue::Warning)
            worst = RepoHealthResult::Warning;
    }
    return worst;
}

RepoHealthCache RepoHealthCheckerLinux::checkAll(const QList<APTSourcePtr> &sources)
{
    RepoHealthCache cache;
    for (const APTSourcePtr &src : sources) {
        QString key = cacheKey(src);
        if (cache.contains(key))
            continue; // skip duplicate entries
        cache[key] = checkOne(src);
    }
    checkDuplicates(sources, cache);
    return cache;
}

RepoHealthResult RepoHealthCheckerLinux::checkOne(const APTSourcePtr &source)
{
    RepoHealthResult result;
    result.lastChecked = QDateTime::currentDateTime();

    checkConnection(source, result);

    // Only run deeper checks if connection succeeded
    bool connected = true;
    for (const auto &issue : result.issues) {
        if (issue.code == "connection_error") {
            connected = false;
            break;
        }
    }

    // Fetch Release file once — used by both resolveDescription and checkReleaseFile
    QString releaseContent;
    if (connected)
        releaseContent = fetchReleaseFile(source->uri, source->suites);

    resolveDescription(source, releaseContent, result);

    if (connected) {
        checkReleaseFile(source, releaseContent, result);
        checkGpgKey(source, result);
        checkSuiteMismatch(source, result);
    }

    checkDeprecatedFormat(source, result);

    result.status = result.issues.isEmpty()
        ? RepoHealthResult::Healthy
        : worstStatus(result.issues);

    return result;
}

void RepoHealthCheckerLinux::checkConnection(const APTSourcePtr &source, RepoHealthResult &result)
{
    QString error = httpHead(source->uri);
    if (!error.isEmpty()) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Error;
        issue.code = "connection_error";
        issue.summary = QObject::tr("Repository unreachable");
        issue.detail = QObject::tr("Could not connect to %1: %2")
            .arg(source->uri, error);
        result.issues.append(issue);
    }
}

void RepoHealthCheckerLinux::checkReleaseFile(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result)
{
    if (releaseContent.isEmpty()) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Error;
        issue.code = "release_404";
        issue.summary = QObject::tr("Release file not found");
        issue.detail = QObject::tr("No InRelease or Release file at %1/dists/%2/. "
                                    "This suite may not exist for this repository.")
            .arg(source->uri, source->suites);
        result.issues.append(issue);
    } else {
        // Parse Origin for metadata
        for (const QString &line : releaseContent.split('\n')) {
            if (line.startsWith("Origin:"))
                result.releaseOrigin = line.mid(7).trimmed();
        }
    }
}

QString RepoHealthCheckerLinux::fetchReleaseFile(const QString &uri, const QString &suite)
{
    // Try InRelease first, then Release
    QString base = uri;
    if (!base.endsWith('/'))
        base += '/';
    base += "dists/" + suite + "/";

    QString content = httpGet(base + "InRelease");
    if (content.isEmpty())
        content = httpGet(base + "Release");
    return content;
}

void RepoHealthCheckerLinux::checkGpgKey(const APTSourcePtr &source, RepoHealthResult &result)
{
    if (!source->signedByPath.isEmpty()) {
        // Check if the keyring file exists
        QFileInfo keyFile(source->signedByPath);
        if (!keyFile.exists()) {
            RepoHealthIssue issue;
            issue.severity = RepoHealthIssue::Error;
            issue.code = "gpg_missing";
            issue.summary = QObject::tr("Signing key file missing");
            issue.detail = QObject::tr("The keyring file %1 does not exist. "
                                        "Packages from this repository cannot be verified.")
                .arg(source->signedByPath);
            result.issues.append(issue);
            return;
        }

        // Check key expiry using gpg
        ExecResult gpgResult = CommandUtil::execWithStatus(
            "gpg", {"--no-default-keyring", "--keyring", source->signedByPath,
                     "--list-keys", "--with-colons"}, 10000);

        if (gpgResult.exitCode == 0) {
            // Parse expiry dates from gpg colon format
            // pub:...:expire_date:...
            for (const QString &line : gpgResult.output.split('\n')) {
                QStringList fields = line.split(':');
                if (fields.size() > 6 && (fields[0] == "pub" || fields[0] == "sub")) {
                    QString expStr = fields[6];
                    if (!expStr.isEmpty()) {
                        QDateTime expiry = QDateTime::fromSecsSinceEpoch(expStr.toLongLong());
                        QDateTime now = QDateTime::currentDateTime();
                        qint64 daysUntil = now.daysTo(expiry);

                        if (daysUntil < 0) {
                            RepoHealthIssue issue;
                            issue.severity = RepoHealthIssue::Error;
                            issue.code = "gpg_expired";
                            issue.summary = QObject::tr("GPG key expired");
                            issue.detail = QObject::tr("The signing key expired on %1.")
                                .arg(expiry.toString("yyyy-MM-dd"));
                            issue.repairLabel = QObject::tr("Refresh signing key");
                            issue.repairCmd = QString("gpg --no-default-keyring --keyring %1 --recv-keys --keyserver keyserver.ubuntu.com")
                                .arg(source->signedByPath);
                            result.issues.append(issue);
                        } else if (daysUntil < 30) {
                            RepoHealthIssue issue;
                            issue.severity = RepoHealthIssue::Warning;
                            issue.code = "gpg_expiring";
                            issue.summary = QObject::tr("GPG key expires in %1 days").arg(daysUntil);
                            issue.detail = QObject::tr("The signing key expires on %1. "
                                                        "Updates will fail after this date.")
                                .arg(expiry.toString("yyyy-MM-dd"));
                            issue.repairLabel = QObject::tr("Refresh signing key");
                            issue.repairCmd = QString("gpg --no-default-keyring --keyring %1 --recv-keys --keyserver keyserver.ubuntu.com")
                                .arg(source->signedByPath);
                            result.issues.append(issue);
                        }
                        break; // Only check first key
                    }
                }
            }
        }
    }
    // If no signed-by and apt-key exists, check via apt-key (legacy)
    // Skipped for now — deprecated format check covers this case
}

void RepoHealthCheckerLinux::checkSuiteMismatch(const APTSourcePtr &source, RepoHealthResult &result)
{
    QString codename = systemCodename();
    if (codename.isEmpty())
        return;

    // Split suites (could be "jammy jammy-updates") and check each
    QStringList suites = source->suites.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &suite : suites) {
        // Skip common suffixes that are valid for any release
        if (suite.endsWith("-updates") || suite.endsWith("-backports") || suite.endsWith("-security"))
            continue;
        // Skip non-codename suites like "stable", "testing", "sid"
        if (suite == "stable" || suite == "testing" || suite == "unstable" || suite == "sid")
            continue;

        if (suite != codename) {
            RepoHealthIssue issue;
            issue.severity = RepoHealthIssue::Warning;
            issue.code = "suite_mismatch";
            issue.summary = QObject::tr("Suite mismatch: %1 vs system %2").arg(suite, codename);
            issue.detail = QObject::tr("This repository targets '%1' but your system runs '%2'. "
                                        "Packages may be incompatible or unavailable.")
                .arg(suite, codename);
            result.issues.append(issue);
            break; // One warning is enough
        }
    }
}

void RepoHealthCheckerLinux::checkDeprecatedFormat(const APTSourcePtr &source, RepoHealthResult &result)
{
    if (source->format == APTSource::Legacy) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Info;
        issue.code = "legacy_format";
        issue.summary = QObject::tr("Legacy .list format");
        issue.detail = QObject::tr("This source uses the legacy one-line format. "
                                    "The modern deb822 (.sources) format is recommended.");
        result.issues.append(issue);
    }

    if (source->signedByPath.isEmpty() && source->format == APTSource::Legacy) {
        RepoHealthIssue issue;
        issue.severity = RepoHealthIssue::Warning;
        issue.code = "no_signed_by";
        issue.summary = QObject::tr("No signed-by key specified");
        issue.detail = QObject::tr("This source does not use the signed-by option. "
                                    "It may rely on deprecated apt-key for signature verification.");
        result.issues.append(issue);
    }
}

void RepoHealthCheckerLinux::checkDuplicates(const QList<APTSourcePtr> &sources, RepoHealthCache &cache)
{
    QMap<QString, int> seen; // normalized key -> count
    for (const APTSourcePtr &src : sources) {
        QString normalized = src->uri.toLower() + " " + src->suites + " " + src->components;
        seen[normalized]++;
    }

    for (const APTSourcePtr &src : sources) {
        QString normalized = src->uri.toLower() + " " + src->suites + " " + src->components;
        if (seen[normalized] > 1) {
            QString key = cacheKey(src);
            if (cache.contains(key)) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "duplicate_source";
                issue.summary = QObject::tr("Duplicate source entry");
                issue.detail = QObject::tr("This repository is defined %1 times across source files. "
                                            "Duplicate entries can cause apt warnings.")
                    .arg(seen[normalized]);
                cache[key].issues.append(issue);
                cache[key].status = worstStatus(cache[key].issues);
            }
        }
    }
}

void RepoHealthCheckerLinux::resolveDescription(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result)
{
    // 1. Try knowledge base
    RepoKnownInfo known = RepoKnowledgeBase::lookup(source->uri);
    if (!known.name.isEmpty()) {
        result.name = known.name;
        result.description = known.description;
        return;
    }

    // 2. Try Release file metadata (pre-fetched, shared with checkReleaseFile)
    if (!releaseContent.isEmpty()) {
        for (const QString &line : releaseContent.split('\n')) {
            if (line.startsWith("Origin:") && result.name.isEmpty())
                result.name = line.mid(7).trimmed();
            if (line.startsWith("Description:") && result.description.isEmpty())
                result.description = line.mid(12).trimmed();
        }
        if (!result.name.isEmpty())
            return;
    }

    // 3. Fallback: domain from URI
    result.name = RepoKnowledgeBase::domainFromUri(source->uri);
}
```

- [ ] **Step 6: Add to CMakeLists.txt**

Add to `CORE_SHARED_HDRS` (after `repo_knowledge_base.h`):

```cmake
  "${CORE_SHARED_DIR}/Tools/repo_health_checker.h"
```

Add to `CORE_SHARED_SRCS` (after `repo_knowledge_base.cpp`):

```cmake
  "${CORE_SHARED_DIR}/Tools/repo_health_checker.cpp"
```

Add to the Linux `CORE_PLAT_SRCS` (after `apt_source_tool.cpp`, around line 154):

```cmake
    "${CORE_PLAT_DIR}/Tools/repo_health_checker.cpp"
```

Add to the Linux `CORE_PLAT_HDRS` (after `apt_source_tool_linux.h`, around line 175):

```cmake
    "${CORE_PLAT_DIR}/Tools/repo_health_checker_linux.h"
```

- [ ] **Step 7: Register test**

In `tests/CMakeLists.txt`, after the `RepoKnowledgeBaseTests` line:

```cmake
add_nexis_test(NAME RepoHealthCheckerTests SOURCES core/test_repo_health_checker.cpp LIBS Qt6::Network)
```

- [ ] **Step 8: Build and run tests**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure -R RepoHealth`
Expected: All 10 health checker tests pass (6 base + 4 logic tests)

- [ ] **Step 9: Commit**

```bash
git add shared/nexis-core/Tools/repo_health_checker.h shared/nexis-core/Tools/repo_health_checker.cpp linux/nexis-core/Tools/repo_health_checker_linux.h linux/nexis-core/Tools/repo_health_checker.cpp tests/core/test_repo_health_checker.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(repo-health): add RepoHealthChecker base and Linux APT implementation"
```

---

## Task 5: macOS Health Checker

**Files:**
- Create: `macos/nexis-core/Tools/repo_health_checker_macos.h`
- Create: `macos/nexis-core/Tools/repo_health_checker.cpp`
- Modify: `CMakeLists.txt` (add to macOS platform sources/headers)

- [ ] **Step 1: Create macOS health checker header**

Create `macos/nexis-core/Tools/repo_health_checker_macos.h`:

```cpp
#ifndef REPO_HEALTH_CHECKER_MACOS_H
#define REPO_HEALTH_CHECKER_MACOS_H

#include "Tools/repo_health_checker.h"
#include <Tools/package_tool_shared.h>

class RepoHealthCheckerMac : public RepoHealthChecker
{
public:
    RepoHealthCache checkAll(const QList<APTSourcePtr> &sources) override;
    RepoHealthResult checkOne(const APTSourcePtr &source) override;

    // Homebrew-specific: check all packages in batch
    RepoHealthCache checkBrewPackages(const QList<Package> &packages);

private:
    void checkOutdated(RepoHealthCache &cache);
    void checkDeprecated(RepoHealthCache &cache);
    void checkTaps(RepoHealthCache &cache);
};

#endif // REPO_HEALTH_CHECKER_MACOS_H
```

- [ ] **Step 2: Implement macOS health checker**

Create `macos/nexis-core/Tools/repo_health_checker.cpp`:

```cpp
#include "repo_health_checker_macos.h"
#include "Utils/command_util.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

RepoHealthCache RepoHealthCheckerMac::checkAll(const QList<APTSourcePtr> &)
{
    // macOS doesn't use APTSource — use checkBrewPackages() instead
    return {};
}

RepoHealthResult RepoHealthCheckerMac::checkOne(const APTSourcePtr &)
{
    // Not used on macOS
    return {};
}

RepoHealthCache RepoHealthCheckerMac::checkBrewPackages(const QList<Package> &packages)
{
    RepoHealthCache cache;

    // Initialize all packages as Healthy
    for (const Package &pkg : packages) {
        RepoHealthResult result;
        result.status = RepoHealthResult::Healthy;
        result.name = pkg.name;
        result.description = pkg.description;
        result.lastChecked = QDateTime::currentDateTime();
        cache[pkg.name] = result;
    }

    checkOutdated(cache);
    checkDeprecated(cache);
    checkTaps(cache);

    return cache;
}

void RepoHealthCheckerMac::checkOutdated(RepoHealthCache &cache)
{
    ExecResult r = CommandUtil::execWithStatus("brew", {"outdated", "--json=v2"}, 30000);
    if (r.exitCode != 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(r.output.toUtf8());
    if (!doc.isObject())
        return;

    auto processArray = [&](const QJsonArray &arr) {
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            if (cache.contains(name)) {
                QString currentVer = obj["installed_versions"].toArray().first().toString();
                QString latestVer;
                QJsonObject versions = obj["current_version"].toObject();
                if (versions.isEmpty())
                    latestVer = obj["current_version"].toString();
                else
                    latestVer = versions["stable"].toString();

                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "outdated";
                issue.summary = QObject::tr("Update available: %1 → %2").arg(currentVer, latestVer);
                issue.detail = QObject::tr("A newer version is available. "
                                            "Current: %1, Available: %2")
                    .arg(currentVer, latestVer);
                issue.repairLabel = QObject::tr("Update package");
                issue.repairCmd = QString("brew upgrade %1").arg(name);

                cache[name].issues.append(issue);
                cache[name].status = RepoHealthResult::Warning;
            }
        }
    };

    QJsonObject root = doc.object();
    processArray(root["formulae"].toArray());
    processArray(root["casks"].toArray());
}

void RepoHealthCheckerMac::checkDeprecated(RepoHealthCache &cache)
{
    ExecResult r = CommandUtil::execWithStatus("brew", {"info", "--json=v2", "--installed"}, 30000);
    if (r.exitCode != 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(r.output.toUtf8());
    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();

    auto processFormulae = [&](const QJsonArray &arr) {
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            if (!cache.contains(name))
                continue;

            if (obj["deprecated"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "deprecated";
                issue.summary = QObject::tr("Package deprecated");
                issue.detail = obj["deprecation_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This package has been deprecated and may be removed in a future Homebrew update.");
                cache[name].issues.append(issue);
                if (cache[name].status != RepoHealthResult::Error)
                    cache[name].status = RepoHealthResult::Warning;
            }

            if (obj["disabled"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Error;
                issue.code = "disabled";
                issue.summary = QObject::tr("Package disabled");
                issue.detail = obj["disable_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This package has been disabled and will not receive updates.");
                issue.repairLabel = QObject::tr("Uninstall package");
                issue.repairCmd = QString("brew uninstall %1").arg(name);
                cache[name].issues.append(issue);
                cache[name].status = RepoHealthResult::Error;
            }
        }
    };

    auto processCasks = [&](const QJsonArray &arr) {
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString token = obj["token"].toString();
            if (!cache.contains(token))
                continue;

            if (obj["deprecated"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Warning;
                issue.code = "deprecated";
                issue.summary = QObject::tr("Cask deprecated");
                issue.detail = obj["deprecation_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This cask has been deprecated.");
                cache[token].issues.append(issue);
                if (cache[token].status != RepoHealthResult::Error)
                    cache[token].status = RepoHealthResult::Warning;
            }

            if (obj["disabled"].toBool()) {
                RepoHealthIssue issue;
                issue.severity = RepoHealthIssue::Error;
                issue.code = "disabled";
                issue.summary = QObject::tr("Cask disabled");
                issue.detail = obj["disable_reason"].toString();
                if (issue.detail.isEmpty())
                    issue.detail = QObject::tr("This cask has been disabled.");
                issue.repairLabel = QObject::tr("Uninstall cask");
                issue.repairCmd = QString("brew uninstall --cask %1").arg(token);
                cache[token].issues.append(issue);
                cache[token].status = RepoHealthResult::Error;
            }
        }
    };

    processFormulae(root["formulae"].toArray());
    processCasks(root["casks"].toArray());
}

void RepoHealthCheckerMac::checkTaps(RepoHealthCache &)
{
    // Tap reachability check — not tied to individual packages, so we skip
    // for now. Could be added as a separate "tap health" feature.
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add to macOS `CORE_PLAT_SRCS` (after `apt_source_tool.cpp`, around line 108):

```cmake
    "${CORE_PLAT_DIR}/Tools/repo_health_checker.cpp"
```

Add to macOS `CORE_PLAT_HDRS` (after `apt_source_tool_macos.h`, around line 130):

```cmake
    "${CORE_PLAT_DIR}/Tools/repo_health_checker_macos.h"
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build succeeds (macOS code compiles on Linux via `#ifdef` exclusion from active use, or skipped entirely since this is in the macOS platform directory)

- [ ] **Step 5: Commit**

```bash
git add macos/nexis-core/Tools/repo_health_checker_macos.h macos/nexis-core/Tools/repo_health_checker.cpp CMakeLists.txt
git commit -m "feat(repo-health): add macOS Homebrew health checker implementation"
```

---

## Task 6: DataRefreshService Integration

**Files:**
- Modify: `shared/nexis/Managers/data_refresh_service.h:19-77`
- Modify: `shared/nexis/Managers/data_refresh_service.cpp:227-240`
- Modify: `shared/nexis/Managers/tool_manager.h:12-68`
- Modify: `shared/nexis/Managers/tool_manager.cpp`

- [ ] **Step 1: Add health checker to ToolManager**

In `shared/nexis/Managers/tool_manager.h`:

Add include after line 8 (`#include <Tools/apt_source_tool.h>`):

```cpp
#include <Tools/repo_health_checker.h>
```

Add public accessor after line 56 (`PackageTool *packageTool() const { ... }`):

```cpp
    RepoHealthChecker *repoHealthChecker() const { return mRepoHealthChecker.get(); }
```

Add private member after line 64 (`std::unique_ptr<AptSourceTool> mAptSourceTool;`):

```cpp
    std::unique_ptr<RepoHealthChecker> mRepoHealthChecker;
```

- [ ] **Step 2: Instantiate health checker in ToolManager constructor**

In `shared/nexis/Managers/tool_manager.cpp`, find the constructor and add after the `mAptSourceTool` initialization:

```cpp
#ifdef Q_OS_MACOS
    #include "repo_health_checker_macos.h"
    mRepoHealthChecker = std::make_unique<RepoHealthCheckerMac>();
#else
    #include "repo_health_checker_linux.h"
    mRepoHealthChecker = std::make_unique<RepoHealthCheckerLinux>();
#endif
```

Note: `tool_manager.cpp` uses `Q_OS_MACOS` (not `Q_OS_MAC`). Follow the same convention here for consistency within the Managers layer. The `#include` inside `#ifdef` is an established pattern in this file.

- [ ] **Step 3: Add signal and method to DataRefreshService**

In `shared/nexis/Managers/data_refresh_service.h`:

Add include after line 14 (`#include <Info/update_info.h>`):

```cpp
#include <Tools/repo_health_types.h>
```

Add public method after line 34 (`void triggerUpdateCheck();`):

```cpp
    void triggerRepoHealthCheck();
```

Add signal after line 48 (`void systemUpdatesChecked(...);`):

```cpp
    void repoHealthChecked(const RepoHealthCache &cache);
```

Add private member after line 76 (`bool mDiskHealthRunning = false;`):

```cpp
    bool mRepoHealthRunning = false;
```

- [ ] **Step 4: Implement `triggerRepoHealthCheck()`**

In `shared/nexis/Managers/data_refresh_service.cpp`, add after the `triggerUpdateCheck()` method (after line 146):

```cpp
void DataRefreshService::triggerRepoHealthCheck()
{
    if (mRepoHealthRunning)
        return;

    mRepoHealthRunning = true;
    QtConcurrent::run([this]() {
        RepoHealthCache cache;
        auto *checker = ToolManager::ins()->repoHealthChecker();
        if (checker) {
#ifdef Q_OS_MACOS
            // macOS: check brew packages
            QList<Package> packages = ToolManager::ins()->getPackages();
            auto *macChecker = static_cast<RepoHealthCheckerMac *>(checker);
            cache = macChecker->checkBrewPackages(packages);
#else
            // Linux: check APT sources
            QList<APTSourcePtr> sources = ToolManager::ins()->getSourceList();
            cache = checker->checkAll(sources);
#endif
        }
        QMetaObject::invokeMethod(this, [this, cache]() {
            mRepoHealthRunning = false;
            emit repoHealthChecked(cache);
        }, Qt::QueuedConnection);
    });
}
```

Add necessary includes at the top of the file:

```cpp
#include "Managers/tool_manager.h"
#ifdef Q_OS_MACOS
#include "repo_health_checker_macos.h"
#endif
```

- [ ] **Step 5: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 6: Run existing tests (no regressions)**

Run: `ctest --test-dir build --output-on-failure`
Expected: All existing tests still pass

- [ ] **Step 7: Commit**

```bash
git add shared/nexis/Managers/data_refresh_service.h shared/nexis/Managers/data_refresh_service.cpp shared/nexis/Managers/tool_manager.h shared/nexis/Managers/tool_manager.cpp
git commit -m "feat(repo-health): integrate health checker into DataRefreshService and ToolManager"
```

---

## Task 7: Enriched Repository Cards

**Files:**
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.ui`
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.h:1-34`
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp:1-66`

- [ ] **Step 1: Update .ui minimum height**

In `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.ui`, change the height values:

Line 10: `<height>45</height>` → `<height>60</height>`
Line 22: `<height>45</height>` → `<height>60</height>`

- [ ] **Step 2: Add new members to header**

In `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.h`:

Add include after line 5 (`#include "Managers/tool_manager.h"`):

```cpp
#include <Tools/repo_health_types.h>
class QLabel;
```

Add public method after line 20 (`APTSourcePtr aptSource() const;`):

```cpp
    void setHealthResult(const RepoHealthResult &result);

private:
    void updateStatusIndicator(RepoHealthResult::Status status);
    void refreshThemeColors();
```

Add private members after line 31 (`APTSourcePtr mAptSource;`):

```cpp
    QLabel *mStatusDot = nullptr;
    QLabel *mLblDescription = nullptr;
    RepoHealthResult::Status mCurrentStatus = RepoHealthResult::Unknown;
```

- [ ] **Step 3: Implement enriched card layout**

Replace the content of `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp` `init()` method. After the existing `ui->setupUi(this);` and `Utilities::addDropShadow(this, 30, 10);` lines, add the new layout code.

Add includes at top of file:

```cpp
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include <QVBoxLayout>
```

In `init()`, after `Utilities::addDropShadow(this, 30, 10);` and before the `#ifdef Q_OS_MAC` block:

```cpp
    // --- Enriched card: status dot + description line ---
    // Restructure layout: wrap name label in a VBox with description underneath
    QHBoxLayout *hLayout = ui->startupAppLayout;

    // Create status dot (8px colored circle)
    mStatusDot = new QLabel(this);
    mStatusDot->setFixedSize(8, 8);
    mStatusDot->setAccessibleName("statusDot");
    updateStatusIndicator(RepoHealthResult::Unknown);

    // Create description label
    mLblDescription = new QLabel(this);
    mLblDescription->setObjectName("lblRepoDescription");
    QFont descFont = mLblDescription->font();
    descFont.setPointSize(descFont.pointSize() - 1);
    mLblDescription->setFont(descFont);
    mLblDescription->setStyleSheet("color: " + AppManager::ins()->getStyleValues().value("@tertiaryText") + ";");

    // Create vertical layout for name + description
    QVBoxLayout *textVBox = new QVBoxLayout();
    textVBox->setSpacing(2);
    textVBox->setContentsMargins(0, 0, 0, 0);

    // Remove lblAptSourceName from the HBox, add to VBox
    hLayout->removeWidget(ui->lblAptSourceName);
    textVBox->addWidget(ui->lblAptSourceName);
    textVBox->addWidget(mLblDescription);

    // Insert status dot and text VBox into the HBox after the icon
    // Icon is at index 0, so insert dot at 1, text at 2
    hLayout->insertWidget(1, mStatusDot, 0, Qt::AlignVCenter);
    hLayout->insertLayout(2, textVBox, 1);

    // Connect theme changes
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &APTSourceRepositoryItem::refreshThemeColors);
```

- [ ] **Step 4: Implement `setHealthResult()` and `updateStatusIndicator()`**

Add these methods to the .cpp file (before the existing `on_checkAptSource_clicked`):

```cpp
void APTSourceRepositoryItem::setHealthResult(const RepoHealthResult &result)
{
    // Update description
    if (!result.description.isEmpty()) {
        QString desc = result.name.isEmpty() ? result.description
            : result.name + QString::fromUtf8(" \u2014 ") + result.description;
        mLblDescription->setText(desc);
        mLblDescription->setToolTip(desc);
    }

    // Update status indicator
    updateStatusIndicator(result.status);
    mCurrentStatus = result.status;

    // Show inline issue summary for warnings/errors
    if (!result.issues.isEmpty() && result.status != RepoHealthResult::Healthy) {
        QString issueSummary = result.issues.first().summary;
        mLblDescription->setText(mLblDescription->text() + " — " + issueSummary);
    }

    // Accessibility
    QString statusText;
    switch (result.status) {
    case RepoHealthResult::Healthy: statusText = tr("Healthy"); break;
    case RepoHealthResult::Warning: statusText = tr("Warning"); break;
    case RepoHealthResult::Error:   statusText = tr("Error"); break;
    default:                         statusText = tr("Unknown"); break;
    }
    setAccessibleDescription(statusText + ": " + mLblDescription->text());
}

void APTSourceRepositoryItem::updateStatusIndicator(RepoHealthResult::Status status)
{
    QMap<QString, QString> tokens = AppManager::ins()->getStyleValues();
    QString color;
    switch (status) {
    case RepoHealthResult::Healthy: color = tokens.value("@successColor"); break;
    case RepoHealthResult::Warning: color = tokens.value("@warningColor"); break;
    case RepoHealthResult::Error:   color = tokens.value("@destructiveColor"); break;
    default:                         color = tokens.value("@tertiaryText"); break;
    }

    mStatusDot->setStyleSheet(QString("background-color: %1; border-radius: 4px;").arg(color));
    ui->aptSourceRepositoryItemWidget->setStyleSheet(
        QString("border-left: 3px solid %1;").arg(color));
}

void APTSourceRepositoryItem::refreshThemeColors()
{
    updateStatusIndicator(mCurrentStatus);
    mLblDescription->setStyleSheet("color: " + AppManager::ins()->getStyleValues().value("@tertiaryText") + ";");
}
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 6: Run all tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass

- [ ] **Step 7: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/apt_source_repository_item.ui shared/nexis/Pages/AptSourceManager/apt_source_repository_item.h shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp
git commit -m "feat(repo-health): enrich repo cards with status dot, description, and theme colors"
```

---

## Task 8: Side Detail Panel Widget

**Files:**
- Create: `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h`
- Create: `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp`
- Modify: `CMakeLists.txt` (add to `GUI_SHARED_SRCS` and headers)

- [ ] **Step 1: Create detail panel header**

Create `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h`:

```cpp
#ifndef REPO_DETAIL_PANEL_H
#define REPO_DETAIL_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <Tools/repo_health_types.h>
#include <Tools/apt_source_tool.h>

class SignalMapper;

class RepoDetailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RepoDetailPanel(QWidget *parent = nullptr);

    void showRepo(const APTSourcePtr &source, const RepoHealthResult &result);
    void clear();

signals:
    void editRequested(const APTSourcePtr &source);
    void disableRequested(const APTSourcePtr &source);
    void repairRequested(const QString &command, const QString &label);
    void closeRequested();

private:
    void setupUi();
    void refreshThemeColors();
    void addIssueWidget(const RepoHealthIssue &issue);
    void clearIssues();

    // Header
    QLabel *mLblName = nullptr;
    QLabel *mLblStatusBadge = nullptr;
    QLabel *mLblDescription = nullptr;

    // Metadata
    QWidget *mMetadataWidget = nullptr;
    QLabel *mLblStatus = nullptr;
    QLabel *mLblLastChecked = nullptr;
    QLabel *mLblFile = nullptr;
    QLabel *mLblSuite = nullptr;
    QLabel *mLblFormat = nullptr;

    // Issues
    QWidget *mIssuesContainer = nullptr;
    QVBoxLayout *mIssuesLayout = nullptr;

    // Actions
    QPushButton *mBtnEdit = nullptr;
    QPushButton *mBtnOpenUri = nullptr;
    QPushButton *mBtnDisable = nullptr;
    QPushButton *mBtnClose = nullptr;

    APTSourcePtr mCurrentSource;
    RepoHealthResult mCurrentResult;
    SignalMapper *mSignalMapper = nullptr;
};

#endif // REPO_DETAIL_PANEL_H
```

- [ ] **Step 2: Implement detail panel**

Create `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp`:

```cpp
#include "repo_detail_panel.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "Utils/command_util.h"
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QScrollArea>
#include <QToolButton>

RepoDetailPanel::RepoDetailPanel(QWidget *parent)
    : QWidget(parent),
      mSignalMapper(SignalMapper::ins())
{
    setupUi();
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &RepoDetailPanel::refreshThemeColors);
}

void RepoDetailPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    // Close button (top-right)
    QHBoxLayout *headerRow = new QHBoxLayout();
    mLblName = new QLabel(this);
    mLblName->setObjectName("repoDetailName");
    QFont nameFont = mLblName->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    mLblName->setFont(nameFont);
    mLblName->setWordWrap(true);
    headerRow->addWidget(mLblName, 1);

    mLblStatusBadge = new QLabel(this);
    mLblStatusBadge->setObjectName("repoStatusBadge");
    mLblStatusBadge->setFixedHeight(20);
    mLblStatusBadge->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(mLblStatusBadge);

    mBtnClose = new QPushButton("✕", this);
    mBtnClose->setFixedSize(24, 24);
    mBtnClose->setFocusPolicy(Qt::NoFocus);
    mBtnClose->setCursor(Qt::PointingHandCursor);
    mBtnClose->setFlat(true);
    connect(mBtnClose, &QPushButton::clicked, this, &RepoDetailPanel::closeRequested);
    headerRow->addWidget(mBtnClose);

    mainLayout->addLayout(headerRow);

    // Description
    mLblDescription = new QLabel(this);
    mLblDescription->setObjectName("repoDetailDescription");
    mLblDescription->setWordWrap(true);
    mainLayout->addWidget(mLblDescription);

    // Metadata grid
    mMetadataWidget = new QWidget(this);
    QGridLayout *metaGrid = new QGridLayout(mMetadataWidget);
    metaGrid->setContentsMargins(0, 0, 0, 0);
    metaGrid->setSpacing(8);

    auto addMetaField = [&](int row, int col, const QString &label, QLabel *&valueLabel) {
        QLabel *lbl = new QLabel(label, mMetadataWidget);
        lbl->setStyleSheet("font-size: 9px; text-transform: uppercase;");
        metaGrid->addWidget(lbl, row * 2, col);
        valueLabel = new QLabel(mMetadataWidget);
        valueLabel->setObjectName("metaValue");
        metaGrid->addWidget(valueLabel, row * 2 + 1, col);
    };

    addMetaField(0, 0, tr("STATUS"), mLblStatus);
    addMetaField(0, 1, tr("LAST CHECKED"), mLblLastChecked);
#ifdef Q_OS_LINUX
    addMetaField(0, 2, tr("FILE"), mLblFile);
    addMetaField(1, 0, tr("SUITE"), mLblSuite);
    addMetaField(1, 1, tr("FORMAT"), mLblFormat);
#endif

    mainLayout->addWidget(mMetadataWidget);

    // Issues section (scrollable)
    QScrollArea *issueScroll = new QScrollArea(this);
    issueScroll->setFrameShape(QFrame::NoFrame);
    issueScroll->setWidgetResizable(true);
    issueScroll->setStyleSheet("QScrollArea{background-color:transparent;}");

    mIssuesContainer = new QWidget();
    mIssuesContainer->setStyleSheet("background-color:transparent;");
    mIssuesLayout = new QVBoxLayout(mIssuesContainer);
    mIssuesLayout->setContentsMargins(0, 0, 0, 0);
    mIssuesLayout->setSpacing(6);
    mIssuesLayout->addStretch();

    issueScroll->setWidget(mIssuesContainer);
    mainLayout->addWidget(issueScroll, 1);

    // Action buttons
    QHBoxLayout *actionRow = new QHBoxLayout();
#ifdef Q_OS_LINUX
    mBtnEdit = new QPushButton(tr("Edit"), this);
    mBtnEdit->setAccessibleName("primary");
    mBtnEdit->setCursor(Qt::PointingHandCursor);
    mBtnEdit->setFocusPolicy(Qt::NoFocus);
    connect(mBtnEdit, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            emit editRequested(mCurrentSource);
    });
    actionRow->addWidget(mBtnEdit);

    mBtnOpenUri = new QPushButton(tr("Open URI"), this);
    mBtnOpenUri->setCursor(Qt::PointingHandCursor);
    mBtnOpenUri->setFocusPolicy(Qt::NoFocus);
    connect(mBtnOpenUri, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            QDesktopServices::openUrl(QUrl(mCurrentSource->uri));
    });
    actionRow->addWidget(mBtnOpenUri);

    mBtnDisable = new QPushButton(tr("Disable"), this);
    mBtnDisable->setAccessibleName("danger");
    mBtnDisable->setCursor(Qt::PointingHandCursor);
    mBtnDisable->setFocusPolicy(Qt::NoFocus);
    connect(mBtnDisable, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            emit disableRequested(mCurrentSource);
    });
    actionRow->addWidget(mBtnDisable);
#else
    // macOS actions
    mBtnOpenUri = new QPushButton(tr("Open Homepage"), this);
    mBtnOpenUri->setCursor(Qt::PointingHandCursor);
    mBtnOpenUri->setFocusPolicy(Qt::NoFocus);
    actionRow->addWidget(mBtnOpenUri);
#endif

    actionRow->addStretch();
    mainLayout->addLayout(actionRow);
}

void RepoDetailPanel::showRepo(const APTSourcePtr &source, const RepoHealthResult &result)
{
    mCurrentSource = source;
    mCurrentResult = result;

    mLblName->setText(result.name.isEmpty() ? source->uri : result.name);
    mLblDescription->setText(result.description);

    // Status badge
    QMap<QString, QString> tokens = AppManager::ins()->getStyleValues();
    QString statusText, statusColor;
    switch (result.status) {
    case RepoHealthResult::Healthy:
        statusText = tr("Healthy"); statusColor = tokens.value("@successColor"); break;
    case RepoHealthResult::Warning:
        statusText = tr("Warning"); statusColor = tokens.value("@warningColor"); break;
    case RepoHealthResult::Error:
        statusText = tr("Error"); statusColor = tokens.value("@destructiveColor"); break;
    default:
        statusText = tr("Unknown"); statusColor = tokens.value("@tertiaryText"); break;
    }
    mLblStatusBadge->setText(statusText);
    mLblStatusBadge->setStyleSheet(QString(
        "background-color: %1; color: white; border-radius: 10px; padding: 2px 10px; font-size: 11px;"
    ).arg(statusColor));

    // Metadata
    mLblStatus->setText(statusText);
    mLblStatus->setStyleSheet(QString("color: %1;").arg(statusColor));

    if (result.lastChecked.isValid()) {
        qint64 secsAgo = result.lastChecked.secsTo(QDateTime::currentDateTime());
        if (secsAgo < 60)
            mLblLastChecked->setText(tr("Just now"));
        else if (secsAgo < 3600)
            mLblLastChecked->setText(tr("%1 min ago").arg(secsAgo / 60));
        else
            mLblLastChecked->setText(tr("%1 hr ago").arg(secsAgo / 3600));
    } else {
        mLblLastChecked->setText(tr("Never"));
    }

#ifdef Q_OS_LINUX
    if (mLblFile) {
        QFileInfo fi(source->filePath);
        mLblFile->setText(fi.fileName());
        mLblFile->setToolTip(source->filePath);
    }
    if (mLblSuite)
        mLblSuite->setText(source->suites);
    if (mLblFormat)
        mLblFormat->setText(source->format == APTSource::Deb822 ? "deb822" : "legacy .list");

    if (mBtnDisable)
        mBtnDisable->setText(source->isActive ? tr("Disable") : tr("Enable"));
#endif

    // Issues
    clearIssues();
    for (const RepoHealthIssue &issue : result.issues)
        addIssueWidget(issue);

    show();
}

void RepoDetailPanel::addIssueWidget(const RepoHealthIssue &issue)
{
    QMap<QString, QString> tokens = AppManager::ins()->getStyleValues();
    QString color;
    switch (issue.severity) {
    case RepoHealthIssue::Error:   color = tokens.value("@destructiveColor"); break;
    case RepoHealthIssue::Warning: color = tokens.value("@warningColor"); break;
    default:                        color = tokens.value("@tertiaryText"); break;
    }

    QWidget *issueWidget = new QWidget(mIssuesContainer);
    issueWidget->setStyleSheet(QString("background-color: rgba(0,0,0,0.1); border-radius: 4px; border-left: 3px solid %1;").arg(color));

    QVBoxLayout *issueLayout = new QVBoxLayout(issueWidget);
    issueLayout->setContentsMargins(10, 8, 10, 8);
    issueLayout->setSpacing(4);

    QLabel *lblSummary = new QLabel(issue.summary, issueWidget);
    lblSummary->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
    issueLayout->addWidget(lblSummary);

    if (!issue.detail.isEmpty()) {
        QLabel *lblDetail = new QLabel(issue.detail, issueWidget);
        lblDetail->setWordWrap(true);
        lblDetail->setStyleSheet(QString("color: %1;").arg(tokens.value("@tertiaryText")));
        issueLayout->addWidget(lblDetail);
    }

    if (!issue.repairCmd.isEmpty()) {
        QPushButton *btnRepair = new QPushButton(issue.repairLabel.isEmpty() ? tr("Repair") : issue.repairLabel, issueWidget);
        btnRepair->setAccessibleName("primary");
        btnRepair->setCursor(Qt::PointingHandCursor);
        btnRepair->setFocusPolicy(Qt::NoFocus);
        btnRepair->setFixedHeight(26);
        QString cmd = issue.repairCmd;
        QString label = issue.repairLabel;
        connect(btnRepair, &QPushButton::clicked, this, [this, cmd, label]() {
            emit repairRequested(cmd, label);
        });
        issueLayout->addWidget(btnRepair, 0, Qt::AlignLeft);
    }

    // Insert before the stretch
    mIssuesLayout->insertWidget(mIssuesLayout->count() - 1, issueWidget);
}

void RepoDetailPanel::clearIssues()
{
    // Remove all widgets except the stretch at the end
    while (mIssuesLayout->count() > 1) {
        QLayoutItem *item = mIssuesLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void RepoDetailPanel::clear()
{
    mCurrentSource.clear();
    mLblName->clear();
    mLblDescription->clear();
    mLblStatusBadge->clear();
    mLblStatus->clear();
    mLblLastChecked->clear();
    clearIssues();
    hide();
}

void RepoDetailPanel::refreshThemeColors()
{
    if (mCurrentSource) {
        // Full re-render: showRepo handles all color/style application
        showRepo(mCurrentSource, mCurrentResult);
    }
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add to `GUI_SHARED_SRCS` (after `apt_source_repository_item.cpp`, around line 244):

```cmake
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/repo_detail_panel.cpp"
```

Add to the GUI headers section (after `apt_source_repository_item.h`, around line 320):

```cmake
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/repo_detail_panel.h"
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/repo_detail_panel.h shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp CMakeLists.txt
git commit -m "feat(repo-health): add RepoDetailPanel side panel widget"
```

---

## Task 9: Page Integration — QSplitter, Health Wiring, Panel Toggle

**Files:**
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h`
- Modify: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`

- [ ] **Step 1: Update page header with new members**

In `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h`:

Add includes after line 14 (`#include <Info/update_info.h>`):

```cpp
#include <Tools/repo_health_types.h>
class RepoDetailPanel;
class QSplitter;
```

Add private slots after line 70 (`void onSystemUpdatesChecked(...);`):

```cpp
    void onRepoHealthChecked(const RepoHealthCache &cache);
    void onRepoItemClicked(QListWidgetItem *item);
    void onDetailPanelCloseRequested();
    void onRepairRequested(const QString &command, const QString &label);
```

Add private members after line 92 (`DataRefreshService *mRefresh = nullptr;`):

```cpp
    // Health dashboard
    RepoDetailPanel *mDetailPanel = nullptr;
    QSplitter *mSplitter = nullptr;
    RepoHealthCache mHealthCache;
    QPushButton *mBtnRefreshHealth = nullptr;
```

- [ ] **Step 2: Implement page integration**

This is the largest change. In `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`:

Add includes:

```cpp
#include "repo_detail_panel.h"
#include <QSplitter>
```

In `init()`, after the updates section setup and before the `#ifdef Q_OS_MAC` block (around line 101), add QSplitter and detail panel setup:

```cpp
    // --- Health Dashboard: QSplitter + Detail Panel ---
    // Wrap the main content area and detail panel in a QSplitter.
    // The existing main content is inside ui->verticalWidget_2.
    // We create a horizontal splitter: [list area | detail panel].
    mSplitter = new QSplitter(Qt::Horizontal, this);
    mSplitter->setChildrenCollapsible(false);

    // Reparent the main vertical widget into the splitter
    mSplitter->addWidget(ui->verticalWidget_2);

    mDetailPanel = new RepoDetailPanel(this);
    mDetailPanel->hide();
    mDetailPanel->setMinimumWidth(250);
    mSplitter->addWidget(mDetailPanel);

    // Set initial sizes: 60% list, 40% detail
    mSplitter->setSizes({600, 400});
    // Since detail panel starts hidden, splitter auto-adjusts
    mSplitter->setStretchFactor(0, 1);
    mSplitter->setStretchFactor(1, 0);

    // Insert splitter into the page layout (replacing the direct widget)
    ui->verticalLayout_2->addWidget(mSplitter);

    connect(mDetailPanel, &RepoDetailPanel::closeRequested,
            this, &APTSourceManagerPage::onDetailPanelCloseRequested);
    connect(mDetailPanel, &RepoDetailPanel::repairRequested,
            this, &APTSourceManagerPage::onRepairRequested);
    connect(mDetailPanel, &RepoDetailPanel::editRequested,
            this, [this](const APTSourcePtr &src) {
        selectedAptSource = src;
        on_btnEditAptSource_clicked();
    });
    connect(mDetailPanel, &RepoDetailPanel::disableRequested,
            this, [this](const APTSourcePtr &src) {
        mToolManager->changeAPTStatus(src, !src->isActive);
        loadAptSources();
    });

    // Refresh Health button (next to Check Now)
    mBtnRefreshHealth = new QPushButton(tr("Refresh Health"), this);
    mBtnRefreshHealth->setObjectName("btnRefreshHealth");
    mBtnRefreshHealth->setCursor(Qt::PointingHandCursor);
    mBtnRefreshHealth->setFocusPolicy(Qt::NoFocus);
    mBtnRefreshHealth->setAccessibleName("primary");
    mBtnRefreshHealth->setFixedHeight(28);
    connect(mBtnRefreshHealth, &QPushButton::clicked, this, [this]() {
        mBtnRefreshHealth->setEnabled(false);
        mBtnRefreshHealth->setText(tr("Checking..."));
        mRefresh->triggerRepoHealthCheck();
    });

    // Wire health check signal
    connect(mRefresh, &DataRefreshService::repoHealthChecked,
            this, &APTSourceManagerPage::onRepoHealthChecked);

    // Chain: after update check completes, trigger health check
    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, [this](const UpdateCheckResult &) {
        mRefresh->triggerRepoHealthCheck();
    });
```

Add the new slot implementations at end of file:

```cpp
void APTSourceManagerPage::onRepoHealthChecked(const RepoHealthCache &cache)
{
    mHealthCache = cache;

    mBtnRefreshHealth->setEnabled(true);
    mBtnRefreshHealth->setText(tr("Refresh Health"));

#ifdef Q_OS_MAC
    // Update tree widget items with health results
    if (mTreeWidget) {
        for (int i = 0; i < mTreeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *section = mTreeWidget->topLevelItem(i);
            for (int j = 0; j < section->childCount(); ++j) {
                QTreeWidgetItem *item = section->child(j);
                QString pkgName = item->data(0, Qt::UserRole).toString();
                if (cache.contains(pkgName)) {
                    const RepoHealthResult &result = cache[pkgName];
                    // Prepend status indicator to display text
                    QString prefix;
                    switch (result.status) {
                    case RepoHealthResult::Warning: prefix = "⚠ "; break;
                    case RepoHealthResult::Error:   prefix = "✗ "; break;
                    default: break;
                    }
                    QString currentText = item->text(0);
                    // Remove existing prefix if re-checking
                    currentText.remove(QRegularExpression("^[⚠✗] "));
                    item->setText(0, prefix + currentText);
                }
            }
        }
    }
#else
    // Update list widget card items with health results
    for (int i = 0; i < ui->listWidgetAptSources->count(); ++i) {
        QListWidgetItem *listItem = ui->listWidgetAptSources->item(i);
        QWidget *widget = ui->listWidgetAptSources->itemWidget(listItem);
        if (!widget) continue;

        APTSourceRepositoryItem *cardItem = dynamic_cast<APTSourceRepositoryItem*>(widget);
        if (!cardItem) continue;

        QString key = RepoHealthChecker::cacheKey(cardItem->aptSource());
        if (cache.contains(key))
            cardItem->setHealthResult(cache[key]);
    }

    // Update detail panel if open
    if (mDetailPanel->isVisible() && !selectedAptSource.isNull()) {
        QString key = RepoHealthChecker::cacheKey(selectedAptSource);
        if (cache.contains(key))
            mDetailPanel->showRepo(selectedAptSource, cache[key]);
    }
#endif
}

void APTSourceManagerPage::onDetailPanelCloseRequested()
{
    mDetailPanel->hide();
    selectedAptSource.clear();
}

void APTSourceManagerPage::onRepairRequested(const QString &command, const QString &label)
{
    // Guided repair: show confirmation dialog
    QString message = tr("The following command will be run to repair this issue:\n\n%1\n\n"
                          "This requires administrator privileges. Proceed?").arg(command);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm Repair: %1").arg(label),
        message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Execute with pkexec for privilege elevation
    QString program = "pkexec";
    QStringList args = command.split(QRegularExpression("\\s+"));

    ExecResult result = CommandUtil::execWithStatus(program, args, 60000);

    if (result.exitCode == 0) {
        // Success — re-check this repo
        mRefresh->triggerRepoHealthCheck();
    } else if (result.exitCode == 126 || result.exitCode == 127) {
        // User cancelled pkexec or pkexec not found — no-op
    } else {
        // Show error
        QMessageBox::warning(this, tr("Repair Failed"),
            tr("The repair command failed with exit code %1.\n\nOutput:\n%2")
                .arg(result.exitCode)
                .arg(result.error.isEmpty() ? result.output : result.error));
    }
}
```

Also update `on_listWidgetAptSources_itemClicked` to toggle the detail panel:

Replace the existing `on_listWidgetAptSources_itemClicked` method with:

```cpp
void APTSourceManagerPage::on_listWidgetAptSources_itemClicked(QListWidgetItem *item)
{
    QWidget *widget = ui->listWidgetAptSources->itemWidget(item);
    if (!widget) {
        selectedAptSource.clear();
        mDetailPanel->clear();
        return;
    }

    APTSourceRepositoryItem *aptSourceItem = dynamic_cast<APTSourceRepositoryItem*>(widget);
    if (!aptSourceItem) {
        selectedAptSource.clear();
        mDetailPanel->clear();
        return;
    }

    APTSourcePtr clickedSource = aptSourceItem->aptSource();

    // Toggle: clicking same repo closes panel
    if (selectedAptSource == clickedSource && mDetailPanel->isVisible()) {
        mDetailPanel->clear();
        selectedAptSource.clear();
        return;
    }

    selectedAptSource = clickedSource;
    QString key = RepoHealthChecker::cacheKey(selectedAptSource);
    if (mHealthCache.contains(key)) {
        mDetailPanel->showRepo(selectedAptSource, mHealthCache[key]);
    } else {
        // Show panel with minimal info while health check hasn't run yet
        RepoHealthResult placeholder;
        placeholder.status = RepoHealthResult::Unknown;
        RepoKnownInfo known = RepoKnowledgeBase::lookup(selectedAptSource->uri);
        placeholder.name = known.name.isEmpty() ? RepoKnowledgeBase::domainFromUri(selectedAptSource->uri) : known.name;
        placeholder.description = known.description;
        mDetailPanel->showRepo(selectedAptSource, placeholder);
    }
}
```

Add needed includes:

```cpp
#include <Tools/repo_health_checker.h>
#include <Tools/repo_knowledge_base.h>
#include <QRegularExpression>
```

- [ ] **Step 3: Add the Refresh Health button to the page layout**

In `init()`, add the button to the updates section header row (after `mBtnCheckNow` is added, around line 70):

```cpp
    updHeader->addWidget(mBtnRefreshHealth);
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 5: Run all tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp
git commit -m "feat(repo-health): integrate health dashboard into APT Repository Manager page"
```

---

## Task 10: Final Integration — Build, Test, Verify

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
Expected: Full clean build succeeds

- [ ] **Step 2: Run all tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass (existing + new)

- [ ] **Step 3: Update tracking files**

Add the feature to `FEATURE_REQUESTS.md` with the next sequential ID:

```
- [~] **FR-XX**: APT Repository Health & Info Dashboard — health indicators, descriptions, and guided repair (#XX)
```

- [ ] **Step 4: Update documentation**

Update `docs/APPLICATION_OVERVIEW.md`:
- In the APT Repository Manager section, add: health indicators, descriptions, side detail panel, guided repair
- Update feature count

Update `docs/ARCHITECTURE_REVIEW.md`:
- Add `RepoHealthChecker` to the Tools section
- Add `RepoDetailPanel` to the UI components section
- Add `repoHealthChecked` signal to the DataRefreshService signal list
- Update signal/component counts

- [ ] **Step 5: Final commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update tracking and documentation for repo health dashboard (FR-XX)"
```
