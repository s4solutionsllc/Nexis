# SSO-15379: Per-Process Network Usage (eBPF, nethogs Fallback)

**Date:** 2026-07-20
**Issue:** SSO-15379 (per-process network throughput for the Processes page, Linux)

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Preferred mechanism | eBPF (two kprobes) | No external binary, lower overhead than shelling out, matches the issue's stated preference |
| Fallback mechanism | `nethogs -t` (persistent child) | Same "long-lived streaming child" shape as the existing macOS `NettopStreamer` (FR-102) |
| BPF struct access | None — scalar kprobe args only | Avoids a `vmlinux.h`/BTF CO-RE dependency entirely; only `bpf_get_current_pid_tgid()` + function args are read |
| Scope | TCP only | Matches BCC's `tcptop` (`tcp_sendmsg` + `tcp_cleanup_rbuf`), the reference this mirrors. UDP is a follow-up. |
| Build gating | CMake auto-detects `libbpf` (pkg-config) + a `clang` binary | Never fails the build when either is missing — falls back to nethogs/messaging at runtime instead |
| Missing-data messaging | New `ProcessInfo::NetIoStatus` enum + a DS §5 `[status="warning"]` notice on the Processes page | AC requires never silently showing blank/zero when permission is denied — mirrors the existing HardwareInfo "Limited data — smartctl requires elevated permissions" pattern (`hardware_info_page.cpp:674`) |

## Data flow

```
ProcessInfoLinux::collectProcesses()  (per tick, off the GUI thread)
  └─ mCollectNetIO?
       ├─ no  → clear state, NetIoStatus::Disabled, return
       └─ yes → mBpfNet->ensureLoaded()  (once; cached thereafter)
                ├─ Loaded            → per-PID bpf_map_lookup_elem, delta-track
                │                       like disk I/O (mPrevNetIo / mNetTimer)
                │                       NetIoStatus::ActiveEbpf
                └─ not Loaded        → nethogs on PATH?
                     ├─ no           → NetIoStatus::PermissionDenied or
                     │                  Unavailable (from mBpfNet->lastError()),
                     │                  netUpRate/netDownRate stay -1.0
                     └─ yes          → start/reuse NetHogsStreamer
                          ├─ hasFailed() → PermissionDenied, rates stay -1.0
                          └─ ok          → apply snapshot() directly (already
                                            a rate, no delta needed)
                                            NetIoStatus::ActiveNetHogs
```

`ProcessesPage::updateNetIoNotice()` reads `InfoManager::getProcessNetIoStatus()`
whenever the Net Down/Net Up columns are visible (on column-toggle and on every
`onProcessesUpdated` tick) and shows/hides an inline notice accordingly. Per-row
`—` cells (existing FR-58/59 convention for `rate < 0`) are unchanged — the
notice is additive context, not a replacement.

## Files changed

| File | Change |
|------|--------|
| `shared/nexis-core/Info/process_info.h` | Add `NetIoStatus` enum + `netIoStatus()`/`netIoStatusDetail()` |
| `shared/nexis/Managers/info_manager.{h,cpp}` | Forward `getProcessNetIoStatus()`/`getProcessNetIoStatusDetail()` |
| `linux/nexis-core/Info/net_acct_shared.h` | POD struct shared between the BPF program and the userspace loader |
| `linux/nexis-core/Info/ebpf/net_acct.bpf.c` | New — kprobes on `tcp_sendmsg`/`tcp_cleanup_rbuf`, pid-keyed hash map |
| `linux/nexis-core/Info/net_acct_bpf_loader.{h,cpp}` | New — libbpf loader/attacher, degrades to a stub when `NEXIS_HAVE_EBPF` isn't defined |
| `linux/nexis-core/Info/net_hogs_streamer.{h,cpp}` | New — persistent `nethogs -t` child, mirrors `NettopStreamer` |
| `linux/nexis-core/Info/process_info_linux.h` / `process_info.cpp` | Wire eBPF → nethogs → status fallback into `collectProcesses()`; **also fixes a latent bug** where the disk-I/O block's early `return processes` would have skipped network collection whenever disk columns were hidden but net columns weren't — changed to a non-returning `if/else` wrap |
| `macos/nexis-core/Info/process_info.cpp` | Set `NetIoStatus::ActiveNetTop`/`Disabled` so the status enum is meaningful cross-platform (macOS behavior itself is unchanged) |
| `shared/nexis/Pages/Processes/processes_page.{h,cpp}` | New `mNetIoNotice` label + `updateNetIoNotice()` |
| `CMakeLists.txt` | libbpf/clang detection, BPF object compile step, `NEXIS_HAVE_EBPF` (PUBLIC — see below), `NEXIS_EBPF_OBJ_PATH` |
| `.github/workflows/build.yml` | Install `libbpf-dev`/`clang`/`libelf-dev` on the Linux job so CI actually compiles the BPF program instead of silently skipping it |
| `tests/core/test_net_hogs_streamer.cpp` | New — pure trace-line parser, cross-platform |
| `tests/core/test_net_acct_bpf_loader.cpp` | New — Linux-only smoke test (never asserts `Loaded`, see below) |
| `tests/CMakeLists.txt` | Register both new test targets |

## A layout hazard worth flagging explicitly

`NEXIS_HAVE_EBPF` changes `NetAcctBpfLoader`'s member layout (extra `bpf_object*`/
`bpf_link*`/`int` fields only exist when it's defined). It is set as a `PUBLIC`
compile definition on the `nexis-core` CMake target rather than `PRIVATE`
specifically so that `NetAcctBpfLoaderTests` (which links `nexis-core` and
includes the header, rather than recompiling the `.cpp`) sees the exact same
macro state the library itself was built with. Getting this wrong would be an
ODR violation — two translation units disagreeing about the size of the same
class — that a normal build would not catch until something corrupted memory
at runtime.

## What could not be verified in this environment

Per the epic's test-hardware/environment instructions, this sandbox has no
cmake/g++/Qt6 toolchain at all (see `project_nexis_no_local_build_toolchain`),
so **nothing** in this change was locally compiled — verification relies
entirely on CI plus this document's reasoning. Specifically:

- **eBPF compile.** CI's Linux job (`build.yml`) now installs `libbpf-dev`,
  `clang`, and `libelf-dev` specifically so `net_acct.bpf.c` and
  `net_acct_bpf_loader.cpp` get real compiler verification instead of the
  CMake detection silently skipping them (which it would do gracefully, but
  that means zero verification of the eBPF path if the CI image happened to
  lack libbpf). **This is the first real compile-level check this code will
  get.**
- **eBPF load/attach at runtime.** Needs `CAP_BPF` (kernel ≥ 5.8) or root.
  GitHub Actions' `ubuntu:26.04` container job almost certainly runs as an
  unprivileged (or at least non-`CAP_BPF`) user, so `NetAcctBpfLoaderTests`
  is written to accept `PermissionDenied`/`Unavailable` as a pass — it proves
  the loader doesn't crash and always explains itself, not that the kprobes
  actually attach and count real traffic.
- **Byte-count correctness against real traffic.** The AC asks for output
  that "roughly matches nethogs/iftop ground truth" — that can only be
  checked by someone running the built binary, with CAP_BPF/root, generating
  real TCP traffic, and comparing against `nethogs`/`iftop` side by side.
  Flagging this as **unverified and needing a manual pass on real hardware**
  before this is trusted as a release feature.
- **`__TARGET_ARCH_*` macro correctness.** `bpf_tracing.h`'s `BPF_KPROBE`
  macro picks its register-to-argument mapping from this macro. It is
  normalized from `CMAKE_SYSTEM_PROCESSOR` (x86_64→x86, aarch64→arm64, ...)
  rather than passed through raw, because passing it raw would silently
  mis-map registers on the most common builder architecture (x86_64) instead
  of failing to compile — exactly the kind of bug that survives a green build
  and only shows up as wrong byte counts. Reasoned through carefully but not
  compiled, let alone run, in this pass.
- **nethogs trace-mode output shape.** `NetHogsStreamer::parseTraceLine` is
  written against the documented `-t` format
  (`<program-path>/<pid>/<uid>\t<sent KB/s>\t<received KB/s>`) and unit-tested
  against synthetic fixtures, but was not checked against a real `nethogs`
  binary's actual stdout — version-to-version formatting drift is plausible
  and would only surface as "notice says Unavailable" (safe/loud) or, worse,
  silently-wrong parses that happen to still pass the `parts.size() >= 3`
  shape check. Recommend a manual side-by-side run before this ships in a
  release.
- **Threading assumptions for `NetHogsStreamer`.** It's built to the exact
  same `QObject` + `QProcess` + `readyReadStandardOutput` shape as the
  existing, shipped `NettopStreamer` (FR-102), which itself runs from
  `ProcessInfoLinux::collectProcesses()` on a `QtConcurrent` worker (per the
  WI-21 audit-M2 comment in that file). Whatever makes `NettopStreamer`'s
  signal delivery work in that context should equally apply here — this
  reuses that precedent rather than re-deriving it, since the underlying
  question (event-loop/dispatcher availability on `QThreadPool` worker
  threads for this codebase's specific dispatch pattern) can't be settled by
  reading code and wasn't newly introduced by this change.

**Recommendation before marking this feature release-ready:** someone with a
Linux box that has `CAP_BPF`/root (or can run Nexis via `sudo` for a manual
check), a modern kernel, and both `nethogs`/`iftop` installed should do a
manual pass: (1) confirm the eBPF path attaches and produces plausible
numbers against known traffic (e.g. a large `curl` download), (2) force the
nethogs fallback (e.g. temporarily rename `net_acct.bpf.o` or run without
`CAP_BPF`) and confirm the same, (3) confirm the permission-denied notice
appears when run fully unprivileged with nethogs absent.
