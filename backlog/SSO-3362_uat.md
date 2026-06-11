# SSO-3362 UAT — Maintenance Wizard close-mid-scan UAF

## Why a manual UAT step (no headless test)

`MaintenanceWizardDialog` is a modal `QDialog` with `WA_DeleteOnClose` whose
constructor hard-defaults `mAppManager`/`mInfoManager`/`mToolManager` to their
respective singletons. Booting those singletons in a headless QTest target pulls
in the same heavy system-coupled state as starting the real app (filesystem
scanning, package-manager probes, polkit handles), which is unreliable in CI
and only catches the UAF if the test happens to be run under
`-fsanitize=address`. The audit (WI-01 in
[SSO-3360 plan](/SSO/issues/SSO-3360#document-2026-06-10-audit-remediation-plan))
explicitly accepts a manual UAT step here in lieu of an automated regression.

## What to verify

Close (X or Esc) the **System Checkup** dialog while any of the four checks are
still running. The app must not crash, and no worker may dereference the
deleted dialog.

## ASan reproduction (Linux)

```bash
# Clean ASan build
rm -rf build-asan
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan -j$(nproc)

# Launch the app under ASan
ASAN_OPTIONS=halt_on_error=1:detect_leaks=0 ./build-asan/nexis
```

In the running app:

1. Open the **Dashboard** page.
2. Click the **System Checkup** quick-action button on the Health Score tile.
3. As soon as the four "Checking…"/"Scanning…" rows appear, press **Esc** (or
   click **Close**).
4. Observe the terminal. Repeat 5–10 times to cover different worker-completion
   orderings.

### Acceptance

- No crash, no ASan diagnostic (`heap-use-after-free`, `SEGV`, etc.).
- The dialog closes cleanly. With the backstop `waitForFinished()` in the
  destructor, the close may briefly stall (typically <2s; up to the slowest
  in-flight check) before the window disappears — that is expected behavior,
  not a regression.

## macOS variant

Repeat the same flow with `-fsanitize=address` on an Apple-clang build
(`-DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)`). The macOS clean build line in
`CLAUDE.md` becomes:

```bash
rm -rf build-asan && cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  && cmake --build build-asan -j$(sysctl -n hw.ncpu)
```
