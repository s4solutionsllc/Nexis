# FR-08 Research: Crowdin Translation Integration

## 1. Current Translation System

### 1.1 Translation Files

**Location:** `shared/translations/` — 26 `.ts` files (Qt Linguist XML format, version 2.1).

| File | Language | Internal Attribute | Strings | Completion |
|------|----------|-------------------|---------|------------|
| `nexis_en.ts` | English | `en_US` | 249 | Source file (0% — all unfinished) |
| `nexis_ar.ts` | Arabic | `ar_SY` | 249 | ~26% (67 translated) |
| `nexis_ca-es.ts` | Catalan | `ca_ES` | 249 | 100% |
| `nexis_cs.ts` | Czech | `cs_CZ` | 251 | ~99% |
| `nexis_de.ts` | German | `de_DE` | 251 | 100% |
| `nexis_es.ts` | Spanish | `es_ES` | 249 | ~99% |
| `nexis_fr.ts` | French | `fr_FR` | 249 | ~99% |
| `nexis_gl.ts` | Galician | `en_US` (!) | 249 | 0% (all unfinished) |
| `nexis_hi.ts` | Hindi | `hi_IN` | 249 | ~26% |
| `nexis_hu.ts` | Hungarian | `hu_HU` | 249 | 100% |
| `nexis_it.ts` | Italian | `it_IT` | 250 | 100% |
| `nexis_kn.ts` | Kannada | `kn_IN` | 249 | ~26% |
| `nexis_ko.ts` | Korean | `ko_KR` | 249 | 0% |
| `nexis_ml.ts` | Malayalam | `ml_IN` | 251 | ~80% |
| `nexis_nl.ts` | Dutch | `nl_NL` | 249 | 100% |
| `nexis_oc.ts` | Occitan | `oc_FR` | 249 | ~26% |
| `nexis_pl.ts` | Polish | `pl_PL` | 249 | 100% |
| `nexis_pt.ts` | Portuguese (Brazil) | `pt_PT` | 249 | 100% |
| `nexis_ro.ts` | Romanian | `ro_RO` | 249 | 100% |
| `nexis_ru.ts` | Russian | `ru_RU` | 249 | 100% |
| `nexis_sv.ts` | Swedish | `sv_SE` | 315 | 100% |
| `nexis_tr.ts` | Turkish | `tr_TR` | 249 | 100% |
| `nexis_ua.ts` | Ukrainian | `uk_UA` | 83 | 100% (subset) |
| `nexis_vn.ts` | Vietnamese | `vi_VN` | 83 | 100% (subset) |
| `nexis_zh-cn.ts` | Chinese (Simplified) | `zh_CN` | 249 | 100% |
| `nexis_zh-tw.ts` | Chinese (Traditional) | `zh_TW` | 83 | 100% (subset) |

**Key observations:**
- 16 languages are at 100% completion.
- 3 files (`nexis_ua.ts`, `nexis_vn.ts`, `nexis_zh-tw.ts`) have only 83 strings vs. the standard ~249, suggesting they were translated from an older version and never updated with `lupdate`.
- `nexis_gl.ts` has `language="en_US"` internally — this is a bug; it should be `gl_ES`.
- Non-standard filename codes: `ua` (should be `uk` per ISO 639-1), `vn` (should be `vi`). Crowdin uses ISO 639-1 codes, so these will need mapping.

### 1.2 Language Registry

**File:** `shared/nexis/static/languages.json` — embedded in QRC at `:/static/languages.json`.

Lists 24 languages (Galician excluded, Romanian included despite having a `.ts` file). Each entry has `value` (locale code used in filenames) and `text` (native display name shown in the Settings dropdown).

Note: `languages.json` does not list `ro` (Romanian) — but `nexis_ro.ts` exists and is 100% translated. This means Romanian translations compile but are not selectable in the UI.

### 1.3 Build Pipeline

**File:** `CMakeLists.txt` lines 82–86

```cmake
file(GLOB_RECURSE NEXIS_TRANSLATIONS "${SHARED_DIR}/translations/**.ts")
find_package(Qt6 COMPONENTS LinguistTools)
qt_create_translation(QM_FILES NEXIS_TRANSLATIONS ${GUI_SHARED_SRCS} ${GUI_PLAT_SRCS})
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${QM_FILES}")
```

- All `.ts` files in `shared/translations/` are auto-discovered via glob.
- `qt_create_translation()` compiles `.ts` → `.qm` (binary) at build time.
- The `.qm` files are included in the executable target (line 120: `${QM_FILES}`).
- No `.qm` files are tracked in git — they're build artifacts.

**Important:** `qt_create_translation()` also runs `lupdate` during the build, which means building the project updates the `.ts` source strings from the C++ code. This is relevant because Crowdin needs a stable source file.

### 1.4 Runtime Loading

**File:** `shared/nexis/Managers/app_manager.cpp` lines 25–30

```cpp
loadLanguageList();  // Reads languages.json from QRC

if (mTranslator.load(
        QString("nexis_%1").arg(mSettingManager->getLanguage()),
        qApp->applicationDirPath() + "/translations")) {
    qApp->installTranslator(&mTranslator);
    // RTL for Arabic
    (mSettingManager->getLanguage() == "ar")
        ? qApp->setLayoutDirection(Qt::RightToLeft)
        : qApp->setLayoutDirection(Qt::LeftToRight);
}
```

- Loads `nexis_{locale}.qm` from `{appDir}/translations/`.
- Language preference stored in `~/.config/Nexis/settings.ini` via `SettingManager`.
- Default is `en` (English).
- Language change requires app restart (no hot-reload).

### 1.5 Settings UI

**File:** `shared/nexis/Pages/Settings/settings_page.cpp` lines 35–46

- Populates `cmbLanguages` combobox from `AppManager::getLanguageList()`.
- On change, calls `SettingManager::setLanguage(locale)` to persist to `settings.ini`.

## 2. QuentiumYT/Stacer Crowdin Setup (Prior Art)

The most active Stacer fork (QuentiumYT) uses Crowdin. Their setup:

### 2.1 Configuration

**File:** `crowdin.yml` at repo root:
```yaml
files:
  - source: translations/stacer_en.ts
    translation: /translations/stacer_%two_letters_code%.ts
```

### 2.2 Integration Method

Uses **Crowdin GitHub App** (not the GitHub Action):
- Crowdin monitors the `main` branch automatically.
- Pushes translations to an `l10n_main` service branch.
- Opens PRs titled "New Crowdin updates" from `l10n_main` → `main`.
- Maintainer manually reviews and merges.

### 2.3 lupdate CI Workflow

**File:** `.github/workflows/lupdate.yml` — triggers on push to `l10n_main`:
1. Installs Qt6 LinguistTools on Ubuntu.
2. Runs `lupdate stacer/stacer.pro -no-obsolete` to normalize `.ts` files.
3. Auto-commits cleaned files back to `l10n_main`.

This ensures translations are consistent before the PR to `main` is reviewed.

### 2.4 Crowdin Project

- URL: `https://crowdin.com/project/stacer`
- Badge: `https://badges.crowdin.net/stacer/localized.svg`
- Free tier (open source).

## 3. Crowdin + Qt .ts Integration

### 3.1 Native Support

Crowdin natively supports Qt `.ts` files. No conversion needed. It parses `<context>`, `<message>`, `<source>`, `<translation>`, `<comment>`, and `<numerusform>` elements.

### 3.2 Configuration Format

```yaml
project_id_env: CROWDIN_PROJECT_ID
api_token_env: CROWDIN_PERSONAL_TOKEN
base_path: "."

files:
  - source: /shared/translations/nexis_en.ts
    translation: /shared/translations/nexis_%two_letters_code%.ts
    languages_mapping:
      two_letters_code:
        zh-CN: zh-cn
        zh-TW: zh-tw
        ca-ES: ca-es
        uk: ua          # Non-standard: our file uses "ua" not "uk"
        vi: vn          # Non-standard: our file uses "vn" not "vi"
```

### 3.3 Integration Options

| Approach | Pros | Cons |
|----------|------|------|
| **Crowdin GitHub App** | Zero CI config, automatic sync | Requires Crowdin app authorization on repo |
| **Crowdin GitHub Action** (`crowdin/github-action@v2`) | Granular control, runs in our CI, no external app access | More config, needs secrets |
| **Crowdin CLI** (manual) | Full control, good for initial setup | No automation |

### 3.4 Known Gotchas

1. **Language code hyphen vs underscore:** Crowdin exports `language="pt-BR"` in the XML attribute, but Qt requires `language="pt_BR"` for correct pluralization. Need a post-processing step to fix this.
2. **Non-standard filename codes:** Our `ua` (Ukrainian) and `vn` (Vietnamese) don't match ISO 639-1 (`uk`, `vi`). Crowdin uses ISO codes, so `languages_mapping` is required.
3. **`qt_create_translation()` runs `lupdate`:** Our CMake config runs `lupdate` during build, which modifies `.ts` source files. This can cause merge conflicts if Crowdin also modifies them. Consider switching to `qt_add_translations()` which separates the `lupdate` step.
4. **Galician `.ts` has wrong language attribute:** `nexis_gl.ts` uses `language="en_US"` instead of `gl_ES`. Crowdin would export the correct attribute, fixing this automatically.
5. **Romanian missing from `languages.json`:** `nexis_ro.ts` exists and is fully translated but isn't listed in the language registry, making it unselectable in the UI.

## 4. Files That Will Be Created/Modified

| File | Action | Purpose |
|------|--------|---------|
| `crowdin.yml` | Create | Crowdin configuration |
| `.github/workflows/crowdin-sync.yml` | Create | GitHub Action for automated sync |
| `.github/workflows/lupdate.yml` | Create | Normalize `.ts` files on translation branch |
| `shared/translations/nexis_en.ts` | Refresh | Run `lupdate` to ensure source strings are current |
| `shared/nexis/static/languages.json` | Update | Add `ro` (Romanian), fix any missing entries |
| `README.md` | Update | Add Crowdin badge and translation contribution link |
| `FEATURE_REQUESTS.md` | Update | Mark FR-08 status |

## 5. Open Questions for Planning

1. **GitHub App vs GitHub Action?** The GitHub Action gives more control and doesn't require authorizing a third-party app. Recommended: GitHub Action.
2. **Crowdin project setup:** User needs to create a Crowdin project at crowdin.com and provide the project ID + personal token as GitHub secrets. This is a manual step outside of code.
3. **Source string refresh:** Should we run `lupdate` first to ensure `nexis_en.ts` has all current strings before uploading to Crowdin? (Recommended: yes.)
4. **Non-standard locale codes (`ua`, `vn`):** Should we rename the files to use standard ISO codes (`uk`, `vi`) and update `languages.json` accordingly? This would simplify the Crowdin mapping but is a breaking change for existing users' saved language preferences.
