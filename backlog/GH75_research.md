# GH#75 Research — All translations missing from AppImage

## Issue Summary

`nexis_*.qm` compiled translation files are absent from the distributed AppImage.
Qt framework translations (`qtbase_*.qm`) are present because `linuxdeploy-plugin-qt`
copies them from the Qt installation. App-level translations are missing entirely.

## Root Cause: Two compounding bugs

### Bug 1 — No CMake install rule for `.qm` files (primary)

**File:** `CMakeLists.txt:480–484, 585–591`

`qt_create_translation` is called correctly and generates `${QM_FILES}`:

```cmake
qt_create_translation(QM_FILES NEXIS_TRANSLATIONS
    ${GUI_SHARED_SRCS} ${GUI_PLAT_SRCS}
    ${CORE_SHARED_SRCS} ${CORE_PLAT_SRCS})
```

The generated `.qm` files are listed as sources in `add_executable(nexis ...)` (line 587)
to establish a build-time dependency, ensuring they're compiled before linking. However,
there is **no `install()` rule** for `${QM_FILES}`.

When the CI runs `DESTDIR=${{ github.workspace }}/AppDir cmake --install build`, the
binary, `.desktop` file, icons, and metainfo are installed — but the `.qm` files stay
only in the build directory and are never staged into `AppDir`.

`linuxdeploy-plugin-qt` then runs over `AppDir` and copies Qt's own translations, but
it has no mechanism to discover or include the app's `.qm` files since they were never
installed into `AppDir`.

**The `.deb` package has the same issue:** the `dpkg-buildpackage` flow also relies on
CMake's install rules, so `.qm` files are missing from `.deb` installs too.

### Bug 2 — Wrong runtime translation search path (secondary)

**File:** `shared/nexis/Managers/app_manager.cpp:34`

```cpp
mTranslator.load(QString("nexis_%1").arg(mSettingManager->getLanguage()),
                 qApp->applicationDirPath() + "/translations")
```

`applicationDirPath()` returns the directory containing the executable binary:
- Dev build: `<project>/build/output` → looks in `build/output/translations/` (OK for dev if we put files there)
- `.deb` install: `/usr/bin` → looks in `/usr/bin/translations/` (**wrong**)
- AppImage: `/tmp/.mount_NexisXXX/usr/bin` → looks in `/tmp/.mount_NexisXXX/usr/bin/translations/` (**wrong**)

The FHS-correct path for app data files installed alongside a binary at `/usr/bin/nexis`
is `/usr/share/nexis/translations/`, which is `../share/nexis/translations` relative to
`applicationDirPath()`.

Even if Bug 1 were fixed by installing `.qm` files to `share/nexis/translations/`, the
app would still fail to load them because it's looking in the wrong place.

## Impact

- All non-English locales broken in AppImage, `.deb`, and `.deb` (plucky) packages
- Language selector in Settings silently has no effect
- This has likely been broken since the Nexis rebranding (no evidence it was ever fixed)

## What works today

Dev builds running directly from `build/output/nexis` would work **only** if `.qm` files
happen to exist at `build/output/translations/`. Currently the build puts `.qm` files in
`build/output/` (matching `CMAKE_BINARY_DIR`) not a `translations/` subdirectory, so
even dev builds are broken.

## Proposed Fix

### 1. CMakeLists.txt — add install rule

After line 484 (`set_directory_properties...`), add:

```cmake
install(
  FILES ${QM_FILES}
  DESTINATION share/nexis/translations
  CONFIGURATIONS Release RelWithDebInfo MinSizeRel
)
```

This stages `.qm` files into `AppDir/usr/share/nexis/translations/` during the
`cmake --install` step, making them available inside the AppImage, .deb, and any
future packaging format.

### 2. app_manager.cpp — fix runtime load path

Replace the single `mTranslator.load(...)` call with a multi-path search:

```cpp
QString lang = mSettingManager->getLanguage();
QStringList searchDirs = {
    QDir::cleanPath(qApp->applicationDirPath() + "/../share/nexis/translations"),
    qApp->applicationDirPath() + "/translations",  // dev build fallback
};
for (const QString &dir : searchDirs) {
    if (mTranslator.load("nexis_" + lang, dir)) {
        qApp->installTranslator(&mTranslator);
        qApp->setLayoutDirection(lang == "ar" ? Qt::RightToLeft : Qt::LeftToRight);
        break;
    }
}
```

Search order: installed FHS path first (correct for .deb and AppImage), then
beside-the-binary fallback (useful for dev builds and any portable layout).

## Files Affected

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add `install(FILES ${QM_FILES} ...)` rule |
| `shared/nexis/Managers/app_manager.cpp` | Replace load path with multi-dir search |

## No Changes Needed

- `.github/workflows/release.yml` — `cmake --install` already runs; once the install rule exists, files will land in AppDir automatically
- `linuxdeploy` invocation — no changes needed; the `.qm` files will be inside AppDir already
- `.ts` source files — no changes needed
- `qt_create_translation` call — works correctly, only the install rule is missing
