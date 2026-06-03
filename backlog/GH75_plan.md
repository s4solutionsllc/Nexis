# GH#75 Plan — All translations missing from AppImage

## Root Cause (summary)

Two bugs: (1) no CMake `install()` rule for `${QM_FILES}`, so `.qm` files never land in
AppDir / .deb during packaging; (2) runtime load path hardcodes `applicationDirPath() +
"/translations"` which is wrong for installed layouts (`/usr/bin/translations/` doesn't
exist — files are at `/usr/share/nexis/translations/`).

## Tasks

- [ ] **Task 1 — Add install rule for .qm files**
  - File: `CMakeLists.txt`
  - After line 484 (`set_directory_properties(...)`), insert:
    ```cmake
    install(
      FILES ${QM_FILES}
      DESTINATION share/nexis/translations
      CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    )
    ```

- [ ] **Task 2 — Fix runtime translation load path**
  - File: `shared/nexis/Managers/app_manager.cpp`
  - Replace lines 34–37 (the `if (mTranslator.load(...))` block) with a multi-path
    search: FHS-installed path (`../share/nexis/translations`) first, then
    beside-binary fallback for dev builds.

- [ ] **Task 3 — Build and verify**
  - Clean rebuild: `rm -rf build && cmake -B build ... && cmake --build build -j$(nproc)`
  - Confirm `.qm` files are generated: `find build -name "nexis_*.qm"`
  - Run tests: `ctest --test-dir build --output-on-failure`

- [ ] **Task 4 — Update CHANGELOG.md**
  - Add entry under the next version section.

- [ ] **Task 5 — Commit and push**
  - Branch: `claude/gh-75-translations-missing`
  - Conventional commit: `fix(i18n): include .qm translation files in all packages (GH#75)`
  - Open PR

## Acceptance Criteria

- `cmake --install build --prefix=/tmp/nexis-test` produces `/tmp/nexis-test/share/nexis/translations/nexis_fr.qm` (and all other locales)
- App loads a non-English locale correctly when `language` is set in settings (verifiable by reading the QTranslator load result in a debug build, or by setting to a lang with known translated strings)
- All existing tests continue to pass
- No hardcoded colors or regressions introduced

## Risk / Rollback

Low risk. No logic changes; only an additional install rule and a fallback in the load path. The fallback is ordered so the installed path is tried first — dev builds still work via the second search dir.
