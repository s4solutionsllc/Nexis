# Pre-Marketing Repository Cleanup — Design

> Date: 2026-02-22
> Goal: Clean up the public-facing repository structure before launching a marketing campaign that will drive significant traffic to the GitHub repo.

## Problem

The repo root currently exposes 166+ internal development artifacts (claude_definitions/, BUGS.md, FEATURE_REQUESTS.md, CLAUDE.md, market_research.md, archive/) that make the project look like an AI experiment rather than a serious application. The README has 18 stacked screenshots that dominate the page and no competitive comparison table.

## Changes

### 1. Remove from git tracking (keep locally, add to .gitignore)

| File/Directory | Reason |
|---|---|
| `claude_definitions/` | Internal AI dev scaffolding (99 files) |
| `BUGS.md` | Internal bug tracker — bad optics at root |
| `FEATURE_REQUESTS.md` | Internal feature tracker |
| `CLAUDE.md` | AI workflow config |
| `FEATURES.md` | Feature matrix — key content folded into README |
| `market_research.md` | Competitive strategy doc |
| `archive/` | Old Stacer assets (40 files), no functional purpose |
| `test_screenshots/` | Generated test output (22 files) |
| `shared/nexis/static/nexis.icns` | Duplicate — build uses `macos/nexis.icns` |
| `shared/nexis/static/header.xcf` | GIMP binary, no value to contributors |

### 2. README Restructuring

- Keep: header, badges, features list, background, downloads, build instructions, development, translations
- Add: competitive comparison table (Nexis vs Stacer vs CleanMyMac X vs BleachBit)
- Reduce: 18 screenshots → 3 hero images + link to full gallery
- Update: Contributing section points to GitHub Issues instead of BUGS.md/FEATURE_REQUESTS.md

### 3. Misc Cleanup

- Remove empty `linux/nexis/Managers/` and `macos/nexis/Managers/` placeholder directories
- Move `shared/nexis/static/themes/nexis/` → `shared/nexis/static/branding/` (not a theme, not referenced by build/QRC)

### 4. Result — Root Directory

```
CHANGELOG.md
CMakeLists.txt
LICENSE
README.md
crowdin.yml
docs/
icons/
linux/
macos/
screenshots/
scripts/
shared/
tests/
website/
```

13 entries. Clean, professional, immediately legible.
