# Platform Compatibility Check

Verify cross-platform correctness after modifying shared code.

## Arguments

$ARGUMENTS — Optional: specific files or subsystem to check (e.g., "DiskInfo" or "shared/nexis-core/Info/").

## When to Run

- After editing any file in `shared/` that touches platform-dependent APIs
- After adding new Info or Tool classes
- After modifying CMakeLists.txt source lists

## Checklist

### Code Paths
- [ ] Changes in `shared/` don't break platform-specific assumptions
- [ ] `#ifdef Q_OS_MAC` / `#ifdef Q_OS_LINUX` guards present where needed
- [ ] No use of platform-specific APIs in shared code without guards:
  - Linux: `/proc/`, `/sys/`, `systemctl`, `apt-get`, `gsettings`
  - macOS: IOKit, `sysctl`, `launchctl`, `brew`, AppleScript

### Abstract Classes (FR-34 pattern)
- [ ] New Info/Tool classes follow the abstract base + platform subclass pattern
- [ ] Base class in `shared/nexis-core/` with pure virtual methods
- [ ] Platform implementations in `linux/nexis-core/` and `macos/nexis-core/`
- [ ] Factory construction via `#ifdef Q_OS_MACOS` in Manager constructor

### CMake Source Lists
- [ ] New `.cpp` files added to the correct `set()` list in CMakeLists.txt:
  - Shared sources: `SHARED_CORE_SOURCES` or `SHARED_GUI_SOURCES`
  - macOS sources: `MACOS_CORE_SOURCES` or `MACOS_GUI_SOURCES`
  - Linux sources: `LINUX_CORE_SOURCES` or `LINUX_GUI_SOURCES`
- [ ] New `.h` files added if they contain Q_OBJECT macros (needed for MOC)

### Tool/Command Paths
- [ ] External tool calls use absolute paths or PATH-safe detection:
  - macOS: `findBrew()` pattern for Homebrew (see BUG-15)
  - Linux: `which` or well-known paths for system tools
- [ ] `CommandUtil::exec()` calls use `LC_ALL=C` for parseable output (see FR-05)

### Conditional Pages
If adding a new conditional page:
- [ ] Detection method added to `ToolManager` (e.g., `checkDocker()`)
- [ ] Page hidden in `App::init()` when detection returns false
- [ ] Sidebar button and tray menu action conditionally created

### Build Both Platforms
If possible, verify on both platforms. Otherwise, ensure:
- [ ] macOS build passes: `cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] CI matrix covers both platforms (check `.github/workflows/build.yml`)
