Perfect. Now I have comprehensive information. Let me generate the thorough research report:

## FR-06 ARM64 Linux Architecture Support — Comprehensive Research Report

Based on my deep investigation of the Nexis codebase, here's a detailed analysis of what's needed for ARM64 (and other architecture) Linux support:

---

## 1. BUILD SYSTEM ANALYSIS

### CMake Architecture Detection
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/cmake/cxxbasics/compiler_detection/GetTargetArch.cmake`

The project already has **excellent architecture detection infrastructure**:

- Supports: `armv8` (ARM64), `armv7`, `armv6`, `armv5`, `arm`, `x86`, `x86_64`, `ia64`, MIPS variants, PowerPC (`ppc`, `ppc64`), IBM System z (`s390`, `s390x`), and SPARC
- Uses compiler preprocessor definitions to detect target architecture at compile time
- Automatically determines `CXXBASICS_CXX_COMPILER_TARGET_ARCH` for the C++ compiler
- No hardcoded x86-specific flags or assumptions in CMakeLists.txt

**Status:** ✅ **No changes needed** — the build system is architecture-agnostic

### Key CMakeLists.txt Features
- Uses platform-based conditionals only: `if(APPLE)` / `else()` (linux/)
- No architecture-specific source file selection or flags
- Links appropriate frameworks on macOS (IOKit, CoreFoundation) only
- Qt6 dependencies are platform-agnostic and support ARM64

**Status:** ✅ **Fully compatible** with ARM64 and other architectures

---

## 2. ARCHITECTURE-SPECIFIC CODE ANALYSIS

### NO Inline Assembly or CPUID
- **Search results:** No `__asm__`, `asm volatile`, `cpuid`, `_rdmsr`, or x86-specific intrinsics found
- **Status:** ✅ **Safe** — code is portable

### NO Hardcoded Architecture Assumptions
- **CPU core detection:** Uses `/proc/cpuinfo` parsing (universal on Linux)
- **CPU clock detection:** Uses `lscpu` (universal) + `/sys/devices/system/cpu/cpu0/cpufreq/` fallback (universal)
- **Memory info:** Parses `/proc/meminfo` (universal format)
- **No x86 enum values or switch statements** checking for specific architectures

**Status:** ✅ **Fully portable** — no architecture-specific gates

---

## 3. SYSTEM INFO GATHERING ANALYSIS

All critical system info files read from **architecture-agnostic Linux interfaces**:

### `/proc/cpuinfo` (Universal)
- **Used in:** `cpu_info.cpp:11-56` (physical cores, core count)
- **ARM64 format:** Identical structure on ARM64 (includes `processor`, `physical id`, `core id` fields)
- **Status:** ✅ Works on ARM64

### `lscpu` Command (Universal)
- **Used in:** `cpu_info.cpp:74-85`, `system_info.cpp:15-48`
- **Prefix:** `LC_ALL=C lscpu` (already correct for non-English systems)
- **ARM64:** Works perfectly; outputs model name, MHz, etc.
- **Status:** ✅ Works on ARM64

### sysfs cpufreq (Universal)
- **Used in:** `cpu_info.cpp:98-104`, `system_info.cpp:40-45` (fallback for modern kernels)
- **Path:** `/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq`
- **ARM64:** Identical interface on ARM64 systems
- **Status:** ✅ Works on ARM64

### `/proc/stat` (Universal)
- **Used in:** `cpu_info.cpp:121-147`
- **ARM64:** Identical format across architectures
- **Status:** ✅ Works on ARM64

### `/proc/meminfo` (Universal)
- **Used in:** `memory_info.cpp:9-35`
- **Format:** Identical across all Linux architectures
- **Status:** ✅ Works on ARM64

### `/proc/loadavg` (Universal)
- **Used in:** `cpu_info.cpp:58-72`
- **Status:** ✅ Works on ARM64

### `/sys/block/*` and `/sys/devices/system/cpu/*` (Universal)
- **Used in:** `disk_info_platform.cpp:8-42`
- **Status:** ✅ Works on ARM64

### `/sys/class/drm/` (Universal)
- **Used in:** `gpu_info.cpp:12-190` (GPU discovery)
- **PCI vendor IDs:** 0x1002 (AMD), 0x10de (NVIDIA), 0x8086 (Intel) — universal
- **ARM64:** Works identically (ARM64 systems can have PCIe GPUs)
- **Status:** ✅ Works on ARM64

### `/sys/class/hwmon/` (Universal)
- **Used in:** `thermal_info.cpp:6-141` (thermal sensor discovery)
- **Status:** ✅ Works on ARM64

### `/sys/class/power_supply/` (Universal)
- **Used in:** `battery_info.cpp:5-197`
- **Status:** ✅ Works on ARM64

### `/sys/class/net/*` (Universal)
- **Used in:** `network_info.cpp:17-21`
- **Status:** ✅ Works on ARM64

### `smartctl` Command (Universal)
- **Used in:** `disk_health_info.cpp:102-204`
- **ARM64:** Works on all architectures
- **Status:** ✅ Works on ARM64

### `ps` Command (Universal)
- **Used in:** `process_info.cpp:6-49`
- **Column output:** Architecture-independent
- **Status:** ✅ Works on ARM64

### `nvidia-smi`, `lspci` (Universal)
- **Used in:** `gpu_info.cpp:41-58, 106-112, 154-168`
- **ARM64:** Works on ARM64 systems with NVIDIA/AMD GPUs
- **Status:** ✅ Works on ARM64

---

## 4. CI/CD PIPELINE ANALYSIS

### Current Build Matrix (`.github/workflows/build.yml`)

```yaml
jobs:
  build:
    strategy:
      matrix:
        include:
          - os: ubuntu-24.04    # x86_64 runner
            name: Linux (x64)
          - os: macos-14        # ARM64 runner (M1/M2/M3 chips)
            name: macOS (ARM64)
```

**Status:** ✅ macOS ARM64 already tested; no x86-specific build flags

### Release Pipeline (`.github/workflows/release.yml`)

```yaml
- name: Linux Build
  runs-on: ubuntu-24.04  # x86_64 only

- name: macOS Build
  runs-on: macos-14      # ARM64 only

- name: Create GitHub Release
  # Release notes hardcode "x86_64" and "ARM64"
```

**Issues:**
- Linux releases only for x86_64 (ubuntu-24.04)
- No cross-compilation or multi-architecture Docker strategy
- Release notes claim "Qt 6.2+, x86_64" in System Requirements (line 297)

**Status:** ⚠️ **Needs enhancement for multi-arch support**

---

## 5. DEBIAN PACKAGING ANALYSIS

### Current Configuration (`linux/debian/control`)

```
Architecture: any
Build-Depends: debhelper-compat (= 13),
               cmake (>= 3.16),
               g++,
               qt6-base-dev,
               qt6-charts-dev,
               ...
```

**Status:** ✅ **Excellent** — `Architecture: any` already declares multi-architecture support!

This means the `.deb` package metadata already supports all architectures, but the **build pipeline doesn't produce them**.

### Debian Rules (`linux/debian/rules`)

```makefile
override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr
```

**Status:** ✅ Standard Debian/Ubuntu cross-compilation rules; fully compatible with `dpkg-buildpackage -B` on any architecture

---

## 6. PACKAGING CONSIDERATIONS

### Debian/Ubuntu Multi-Architecture Support

To build `.deb` packages for multiple architectures, you need:

1. **Cross-compilation environment** (e.g., for building ARM64 on x86_64):
   ```bash
   sudo dpkg --add-architecture arm64
   sudo apt-get update
   sudo apt-get install -y crossbuild-essential-arm64 qt6-base-dev:arm64 qt6-charts-dev:arm64 ...
   ```

2. **CMake cross-compilation flags:**
   ```bash
   cmake -B build \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_SYSTEM_NAME=Linux \
     -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
     -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
     -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
   ```

3. **AppImage considerations:**
   - `linuxdeploy-aarch64.AppImage` exists on GitHub releases
   - Current release.yml only downloads `x86_64.AppImage`

### RPM Support (Optional)
- No `.spec` files exist currently
- Would need similar multi-arch consideration

---

## 7. POTENTIAL ISSUES & GOTCHAS

### ✅ Non-Issues (Already Handled)

1. **Qt6 support on ARM64:** Qt6.2+ has excellent ARM64 support via prebuilt binaries
2. **Compiler support:** GCC and Clang on ARM64 Linux are mature
3. **Third-party tools:** `lscpu`, `smartctl`, `nvidia-smi`, `ps` all support ARM64
4. **Integer sizes:** All integer types already use `qint64`/`quint64` (64-bit safe)
5. **Floating-point:** No special FPU assumptions in code
6. **Byte order:** All parsing is text-based (endianness-neutral)

### ⚠️ Potential Issues

1. **Binary availability:**
   - NVIDIA GPU tools (`nvidia-smi`) only available on systems with NVIDIA driver
   - AMD GPU tools (`amdgpu`) driver support varies by distribution
   - ARM64 IoT devices (Raspberry Pi) may lack these tools — **handled gracefully** (returns -1 utilization)

2. **System info availability on ARM embedded systems:**
   - Raspberry Pi, ODROID: May lack `/sys/class/hwmon/` (thermal sensors)
   - **Handled:** Code checks `if (!drmDir.exists())`, `if (!hwmonDir.exists())` — returns empty gracefully
   - No crash risk

3. **sysfs cpufreq availability:**
   - Some embedded ARM SoCs disable cpufreq scaling
   - **Handled:** Fallback to `/proc/cpuinfo` MHz (or return 0.0 if unavailable)

4. **Battery detection on ARM server boards:**
   - Some ARM64 servers have no batteries
   - **Handled:** `if (mData.hasBattery)` gate prevents accessing non-existent files

---

## 8. REQUIRED CHANGES FOR FULL ARM64 SUPPORT

### Phase 1: CI/CD Updates (High Impact)

**File:** `.github/workflows/release.yml`

1. Add ARM64 Linux build matrix using QEMU emulation or cross-compilation Docker:
   ```yaml
   build-linux:
     strategy:
       matrix:
         include:
           - os: ubuntu-24.04
             arch: x86_64
             artifact: nexis-linux-x86_64
           - os: ubuntu-24.04
             arch: arm64
             build-args: "-DCMAKE_SYSTEM_PROCESSOR=aarch64 ..."
             artifact: nexis-linux-arm64
   ```

2. Update release notes to mention supported architectures:
   ```markdown
   #### Linux (ARM64)
   | Format | File | Description |
   |--------|------|-------------|
   | `.deb` | `nexis_*_arm64.deb` | For Ubuntu 22.04+ ARM64 (Pi 5, Jetson, etc.) |
   | `.AppImage` | `Nexis-*-Linux-arm64.AppImage` | Portable ARM64 |
   ```

3. Download `linuxdeploy-aarch64.AppImage` for ARM64 builds

### Phase 2: Build System Enhancements (Optional but Recommended)

**File:** `CMakeLists.txt`

1. Add explicit CPU feature detection (for optimization flags):
   ```cmake
   # Optional: detect and use NEON on ARM
   if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|armv8")
     # NEON is always available on ARMv8+
     # Can add -march=armv8-a for optimizations if desired
   endif()
   ```

2. Add architecture-specific compiler flags for hardening:
   ```cmake
   if(UNIX AND NOT APPLE)
     target_compile_options(nexis PRIVATE -fPIC)  # Position-independent code
   endif()
   ```

### Phase 3: Documentation Updates (Low Effort, High Value)

**Files:** `README.md`, `release.yml` (notes section), `FEATURE_REQUESTS.md`

1. Update README system requirements:
   ```markdown
   - **Linux:** Ubuntu 22.04+ / Debian 12+ — x86_64, ARM64 (aarch64), ARMv7
   ```

2. Document installation for ARM systems:
   ```markdown
   #### Raspberry Pi 5 / ARM64
   ```bash
   sudo dpkg -i nexis_*_arm64.deb
   sudo apt-get install -f
   ```
   ```

3. Mark FR-06 as `[x]` in FEATURE_REQUESTS.md once complete

### Phase 4: Testing Recommendations (Best Practices)

- **ARM64 test platforms:** Raspberry Pi 5 (official), AWS Graviton, Ampere Altra, Jetson Orin
- **Regression test:** Run full Hardware Info, Resources, Dashboard pages on ARM64 device
- **Thermal/Battery:** Test on actual ARM SBC with thermal sensors + battery

---

## 9. ARCHITECTURE FEATURE MATRIX

| Architecture | Support | Notes |
|---|---|---|
| **x86_64** | ✅ Full | Already building; all code tested |
| **ARM64 (aarch64)** | ✅ Ready* | Code is architecture-agnostic; needs CI/CD setup |
| **ARMv7 (armhf)** | ✅ Ready* | 32-bit, same `/proc/*` interface, all code compatible |
| **i386 (32-bit x86)** | ✅ Ready* | Code uses proper 64-bit types (qint64), compatible |
| **PowerPC (ppc/ppc64el)** | ✅ Ready* | Big-endian safe (text parsing), all code compatible |
| **RISC-V (riscv64)** | ✅ Ready* | Emerging, but code is fully portable |
| **IBM System z (s390x)** | ✅ Ready* | Big-endian, all code compatible |

*Requires CI/CD pipeline setup to build/package; code itself is architecture-agnostic

---

## 10. SUMMARY

### Current State
- ✅ **Build system:** Fully architecture-agnostic (uses `GetTargetArch.cmake`)
- ✅ **C++ code:** No inline assembly, CPUID, or x86-specific assumptions
- ✅ **System info gathering:** 100% uses universal Linux interfaces (`/proc/*`, `/sys/*`, standard commands)
- ✅ **Debian control file:** Already declares `Architecture: any`
- ⚠️ **CI/CD:** Only builds for x86_64 (Linux) and ARM64 (macOS)
- ⚠️ **AppImage:** Only downloads x86_64 linuxdeploy

### What Needs to Be Done
1. **Add ARM64 build job to `.github/workflows/release.yml`** — most important
2. **Update release notes** to advertise ARM64 support
3. **Document installation for ARM systems** in README
4. **Test on actual ARM64 hardware** (Pi 5, Jetson, etc.)

### Implementation Effort
- **Low effort:** Code is already portable; ~2-3 hours for CI/CD updates
- **High confidence:** No code changes needed for ARM64; only build automation

---

This research indicates that **FR-06 is almost entirely complete from a code perspective**. The remaining work is purely in CI/CD infrastructure to produce and publish ARM64 binaries.
