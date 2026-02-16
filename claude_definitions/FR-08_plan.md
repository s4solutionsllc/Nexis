# FR-08 Implementation Plan: Crowdin Translation Integration

## Overview

Add Crowdin integration to enable community-driven translations via a professional translation management platform. This creates a GitHub Action–based sync pipeline and fixes existing translation data issues.

---

## Task 1: Fix Translation Data Issues

Before connecting Crowdin, clean up existing translation inconsistencies.

### 1.1 Fix `nexis_gl.ts` language attribute
- [x] Change `language="en_US"` to `language="gl_ES"` in the `<TS>` root element of `shared/translations/nexis_gl.ts`.

### 1.2 Add Romanian to `languages.json`
- [x] Add `{"value": "ro", "text": "Română"}` to `shared/nexis/static/languages.json`. The `.ts` file exists and is 100% translated but was never registered in the UI.

### 1.3 Refresh source strings with `lupdate`
- [x] Run `lupdate` against the codebase to ensure `nexis_en.ts` contains all current translatable strings. This is the file Crowdin will use as its source of truth. (Result: 417 source strings — 272 new, 145 existing, 104 obsolete removed.)
- [x] Verify the string count is consistent across files after refresh.

**Build verification:** Incremental build to confirm nothing broke.

---

## Task 2: Create Crowdin Configuration

### 2.1 Create `crowdin.yml`
- [x] Create `crowdin.yml` at project root with:
  - Source: `shared/translations/nexis_en.ts`
  - Translation pattern: `shared/translations/nexis_%two_letters_code%.ts`
  - `languages_mapping` for non-standard codes:
    - `zh-CN` → `zh-cn`, `zh-TW` → `zh-tw`, `ca-ES` → `ca-es`
    - `uk` → `ua` (Ukrainian filename uses `ua`)
    - `vi` → `vn` (Vietnamese filename uses `vn`)

**Acceptance criteria:** Valid YAML. The `languages_mapping` section correctly maps all locale codes that differ from Crowdin's ISO 639-1 defaults to our filename convention.

---

## Task 3: Create GitHub Actions Workflows

### 3.1 Create Crowdin sync workflow
- [x] Create `.github/workflows/crowdin-sync.yml`:
  - Triggers on push to `native` branch (when source `.ts` file changes) and on a schedule (every 12 hours).
  - Uses `crowdin/github-action@v2`.
  - Uploads source strings from `nexis_en.ts`.
  - Downloads translations and creates a PR to `native`.
  - Requires repository secrets: `CROWDIN_PROJECT_ID`, `CROWDIN_PERSONAL_TOKEN`.

### 3.2 Create lupdate lint workflow
- [x] Create `.github/workflows/lupdate.yml`:
  - Triggers on push to the Crowdin localization branch (`l10n_crowdin_translations`).
  - Installs Qt6 LinguistTools.
  - Runs `lupdate` with the project's source directories to normalize downloaded `.ts` files.
  - Auto-commits cleaned files back to the localization branch.

**Acceptance criteria:** Both workflows have valid YAML syntax. The sync workflow correctly references `crowdin.yml`. The lupdate workflow targets the correct branch and source directories.

---

## Task 4: Update Documentation

### 4.1 Update README.md
- [x] Add a Crowdin localization badge near the top of the README.
- [x] Add a "Translations" or "Contributing Translations" section explaining how to contribute via Crowdin, with a link to the Crowdin project page.

### 4.2 Add setup instructions for maintainers
- [x] Add a brief section to README (or a `docs/crowdin-setup.md`) documenting:
  - How to create the Crowdin project (manual step).
  - Which GitHub repository secrets to set (`CROWDIN_PROJECT_ID`, `CROWDIN_PERSONAL_TOKEN`).
  - How the sync pipeline works end-to-end.
  - How to do the initial upload of existing translations via Crowdin CLI.

**Acceptance criteria:** A contributor can follow the docs to understand how to translate. A maintainer can follow the docs to set up the Crowdin project from scratch.

---

## Task 5: Update Tracking Files

- [x] Mark FR-08 as `[x]` in `FEATURE_REQUESTS.md` with resolution note.
- [x] Commit and push all changes.

---

## Manual Steps (User Action Required)

These cannot be automated and must be done by the project owner:

1. **Create Crowdin project** at [crowdin.com](https://crowdin.com) (free for open source).
2. **Add GitHub secrets** to the repository:
   - `CROWDIN_PROJECT_ID` — numeric project ID from Crowdin dashboard.
   - `CROWDIN_PERSONAL_TOKEN` — personal access token from Crowdin account settings.
3. **Initial translation upload** — run `crowdin upload translations` via CLI to seed the Crowdin project with existing translated strings (one-time step).
4. **Update Crowdin badge URL** in README once the project is created (project-specific badge URL).
