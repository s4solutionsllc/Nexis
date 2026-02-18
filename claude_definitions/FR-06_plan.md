# FR-06 Implementation Plan — ARM64 Linux Architecture Support

## Summary

The Nexis codebase is **already architecture-agnostic** — all C++ code uses portable Linux interfaces (`/proc/*`, `/sys/*`, standard CLI tools) with no inline assembly or x86-specific assumptions. The CMake build system already detects ARM64 via `GetTargetArch.cmake`, and the Debian `control` file declares `Architecture: any`.

**All work is CI/CD infrastructure and documentation** — zero application code changes required.

---

## Phase 1: CI Build Pipeline — Add Linux ARM64 Matrix Entry

**File:** `.github/workflows/build.yml`

- [x]**1.1** Add a `Linux (ARM64)` matrix entry using GitHub's `ubuntu-24.04-arm` runner (ARM64 native runner, available since late 2025)
- [x]**1.2** Set artifact name to `nexis-linux-arm64` and artifact path to `build/output/nexis`
- [x]**1.3** The existing Linux dependency install and CMake configure steps already use `runner.os == 'Linux'` conditionals — no changes needed there; the ARM64 runner uses the same apt packages

**Acceptance criteria:** CI builds and uploads an ARM64 Linux binary on push/PR to `native`.

---

## Phase 2: Release Pipeline — ARM64 .deb and AppImage

**File:** `.github/workflows/release.yml`

- [x]**2.1** Refactor `build-linux` into a matrix strategy with two entries:
  - `{ arch: x86_64, runner: ubuntu-24.04, linuxdeploy-arch: x86_64 }`
  - `{ arch: arm64, runner: ubuntu-24.04-arm, linuxdeploy-arch: aarch64 }`
- [x]**2.2** Parameterize the linuxdeploy download URLs to use `${{ matrix.linuxdeploy-arch }}` instead of hardcoded `x86_64`
- [x]**2.3** Update artifact names to include architecture: `linux-artifacts-x86_64`, `linux-artifacts-arm64`
- [x]**2.4** Update the `release` job to download both architecture artifacts
- [x]**2.5** Update the `Find artifact files` step to locate both x86_64 and arm64 .deb, .AppImage, and binary files
- [x]**2.6** Update the release body to include an ARM64 Linux download section:
  ```
  #### Linux (ARM64 / aarch64)
  | Format | File | Description |
  |--------|------|-------------|
  | `.deb` | `nexis_*_arm64.deb` | For Ubuntu 22.04+ ARM64 (Raspberry Pi 5, Jetson, Graviton, etc.) |
  | `.AppImage` | `Nexis-*-Linux-arm64.AppImage` | Portable ARM64 — runs on any ARM64 Linux distro |
  | Binary | `nexis` | Standalone ARM64 executable (requires Qt6 runtime) |
  ```
- [x]**2.7** Update the System Requirements line to include ARM64:
  ```
  - **Linux:** Ubuntu 22.04+ / Debian 12+ / Fedora 36+ — Qt 6.2+, x86_64 or ARM64 (aarch64)
  ```
- [x]**2.8** Add the ARM64 release files to the `files:` list in the GitHub Release action

**Acceptance criteria:** Tagging `v*` produces a release with both x86_64 and arm64 Linux artifacts.

---

## Phase 3: LLD Linker — Enable on ARM64

**File:** `shared/cmake/cxxbasics/accelerators/UseFasterLinkers.cmake`

- [x]**3.1** Update the LLD guard from `x86_64`-only to also include `armv8` (ARM64). LLD has been production-quality on ARM64 since LLVM 12 (2021). Change:
  ```cmake
  if("${__cxxbasics_target_arch}" STREQUAL "x86_64")
  ```
  to:
  ```cmake
  if("${__cxxbasics_target_arch}" STREQUAL "x86_64"
      OR "${__cxxbasics_target_arch}" STREQUAL "armv8")
  ```

**Acceptance criteria:** ARM64 builds use LLD when available, falling back to gold/default otherwise.

---

## Phase 4: Documentation Updates

- [x]**4.1** **README.md** — Update the downloads section to list ARM64 Linux alongside x86_64:
  ```
  - **Linux x86_64**: `.deb` package, `.AppImage` portable, standalone binary
  - **Linux ARM64**: `.deb` package, `.AppImage` portable, standalone binary
  ```
- [x]**4.2** **README.md** — Add ARM64 build-from-source instructions (identical to x86_64, just noting it works on ARM64 natively)
- [x]**4.3** **FEATURE_REQUESTS.md** — Mark FR-06 as `[x]` with resolution note

**Acceptance criteria:** README accurately reflects ARM64 availability; FR-06 marked complete.

---

## Out of Scope (for now)

These architectures are listed in the original FR-06 description but are **deprioritized** due to lack of GitHub Actions runners and minimal user demand:
- **armhf (32-bit ARM)**: No native GH runner; code is compatible if cross-compiled
- **i386 (32-bit x86)**: Declining relevance; Qt6 has dropped 32-bit support on most distros
- **powerpc / ppc64el**: Niche; no GH runner
- **riscv64**: Emerging; no GH runner yet
- **s390x**: Mainframe-only; no GH runner

The code is already portable to all of these — the only blocker is CI infrastructure. They can be added later via self-hosted runners or Docker+QEMU cross-compilation if there's demand.

---

## Task Summary

| Phase | Tasks | Effort |
|-------|-------|--------|
| 1. CI Build | 3 | Low |
| 2. Release Pipeline | 8 | Medium |
| 3. LLD Linker | 1 | Low |
| 4. Documentation | 3 | Low |
| **Total** | **15** | |
