# Contributing to Nexis

Thank you for your interest in contributing to Nexis! Nexis is a free, open-source Linux & macOS system optimizer and monitor, and community contributions help keep it useful for everyone. Whether you are fixing a bug, improving translations, adding tests, or proposing new features, your work is appreciated.

We aim to review external pull requests within **7 days** of submission and to acknowledge new issues within **7 days** of filing. Please open an issue first for anything beyond a small fix so we can discuss scope before you invest time in implementation.

---

## Table of Contents

1. [Build instructions](#1-build-instructions)
2. [Code style](#2-code-style)
3. [Running tests](#3-running-tests)
4. [Pull request process](#4-pull-request-process)
5. [Issue triage labels](#5-issue-triage-labels)
6. [Where to ask questions](#6-where-to-ask-questions)

---

## 1. Build instructions

Full environment setup is documented in the project [README](README.md). Quick reference:

### Linux

```bash
# Install Qt 6 and build tools (Debian/Ubuntu)
sudo apt install qt6-base-dev qt6-charts-dev cmake build-essential

# Clean build
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS

```bash
# Requires Homebrew and Qt 6
brew install qt@6 cmake

# Clean build
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Incremental rebuild (after first build)

```bash
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

---

## 2. Code style

Nexis is C++17 with Qt 6. Please follow these conventions:

- **Qt/C++17 idioms** — use `QObject`, signals/slots, and Qt container types consistently with the existing codebase.
- **No hardcoded colors in C++** — all colors come from `values.ini` theme tokens accessed via `AppManager::getStyleValues()`. Store token strings (e.g. `"@cpuColor"`) and implement `refreshThemeColors()` on any new widget.
- **No `Qt::NoFocus` on interactive controls (SSO-3502)** — buttons, toggles, checkboxes, sliders, combo boxes, line edits, and tree/table/list widgets that the user can act on must accept keyboard focus so screen-reader, switch-control, and tab-keyboard users can reach them. The default focus policy is already correct; do not call `setFocusPolicy(Qt::NoFocus)` on new interactive widgets. The visible focus ring is drawn by `general.qss` (`style.qss`) via the `@focusRingColor` token, so simply letting Qt assign the default focus policy is enough. The only acceptable uses of `Qt::NoFocus` are read-only data displays (a tree/table that is `NoSelection + NoEditTriggers`) and widgets whose focus is intentionally forwarded from a sibling (the command-palette result list, whose arrow-key navigation comes through the search box's event filter). When you do that, add an inline comment explaining *why* focus is forwarded.
- **Naming** — `camelCase` for variables and functions; `PascalCase` for class names; file names use `snake_case`.
- **clang-format** — a `.clang-format` file is not yet committed; follow the style of the file you are editing.
- **No debug output in submitted code** — remove `qDebug()` / `printf` statements before opening a PR.
- **Platform guards** — wrap Linux-only or macOS-only code in `#ifdef Q_OS_LINUX` / `#ifdef Q_OS_MACOS`.

---

## 3. Running tests

Tests use Qt Test (QTest) with CTest integration and build by default.

```bash
# Build (tests are included automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run all tests
ctest --test-dir build --output-on-failure

# Run a single test by name
ctest --test-dir build -R FormatUtil --output-on-failure
```

### Adding a new test

1. Create `tests/<category>/test_<classname>.cpp` (categories: `utils/`, `core/`, `managers/`).
2. Use `QTEST_MAIN(TestClassName)` and `#include "test_<classname>.moc"` at the bottom.
3. Add the file to the source list in `tests/CMakeLists.txt`.
4. Register with CTest via `add_test(NAME <TestName> COMMAND nexis-tests)`.

---

## 4. Pull request process

1. **Branch from `native`** — all development targets the `native` branch, not `main` or `master`.
   ```bash
   git checkout native && git pull
   git checkout -b your-name/short-description
   ```

2. **Conventional commits** — use the format `type(scope): description` under 72 characters.
   Common types: `feat`, `fix`, `docs`, `test`, `refactor`, `chore`.
   Example: `fix(network): detect wlp* Wi-Fi interfaces on Linux`

3. **One logical change per PR** — keep PRs focused. Unrelated cleanups belong in a separate PR.

4. **Tests required for bug fixes** — if you fix a bug, add a regression test.

5. **Update `CHANGELOG.md`** — add an entry under `## [Unreleased]` in [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format.

6. **Open the PR against `native`** — include a short description of what changed and why. Reference the issue number with `Closes #NN` or `Relates to #NN`.

7. **CI must pass** — the GitHub Actions build runs on Linux and macOS. Please ensure your changes do not break either platform.

---

## 5. Issue triage labels

| Label | Criteria |
|---|---|
| `good first issue` | Self-contained, clear acceptance criterion, testable via `ctest`, does not require deep codebase knowledge. Examples: adding tests for a single utility function, completing a translation file, fixing a narrowly-scoped UI issue with a clear reproduction path. |
| `bug` | Something that worked before is broken, or behavior diverges from documented expectations. |
| `enhancement` | A new feature or meaningful improvement to existing functionality. Use this for translation work (correcting / completing `.ts` files under `shared/translations/`) and tag `needs-native-qa` for sign-off when applicable. |
| `documentation` | Changes to `README.md`, `CONTRIBUTING.md`, `docs/`, or inline code comments. |
| `needs-native-qa` | Translation PRs requiring native-speaker review/sign-off before merge. |
| `help wanted` | We'd love an external contribution here. |
| `question` | Open question or discussion — see §6 below. |

When filing a bug report, please include: platform, Nexis version, steps to reproduce, expected behavior, and actual behavior. Screenshots or terminal output are very helpful.

---

## 6. Where to ask questions

Open a [GitHub Issue](https://github.com/s4solutionsllc/Nexis/issues) with the `question` label — that is the best place to ask for help, clarify expected behavior, or discuss an approach before investing time in a PR.

We do not currently have a chat channel; GitHub Issues are the single discussion venue.
