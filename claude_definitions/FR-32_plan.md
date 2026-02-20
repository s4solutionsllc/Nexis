# FR-32 Plan: QSS Token Validation in Theme System

> Phase 3 of the Implementation Roadmap
> Date: 2026-02-20

---

## Overview

Add runtime validation to `AppManager::updateStylesheet()` that:
1. Checks all `@tokens` in `style.qss` have corresponding entries in the active theme's `values.ini`
2. Validates that color token values are valid hex format
3. Emits `qWarning()` diagnostics for any mismatches

**File to modify:** `shared/nexis/Managers/app_manager.cpp` (single file, ~15 lines of new code)

**No new files, no CMakeLists.txt changes, no new dependencies.**

---

## Tasks

### Task 3.1: Add token validation to `updateStylesheet()`

**Location:** `shared/nexis/Managers/app_manager.cpp`, inside `updateStylesheet()`, between the QSS load (line 100) and the replacement loop (line 103).

**What to do:**

1. After loading the raw QSS template into `mStylesheetFileContent` (line 100), scan it for all `@token` patterns using a regex: `@([a-zA-Z][a-zA-Z0-9_]*)`
2. For each matched token:
   - Skip if it matches `@dp\d+` (DPI tokens handled separately by the existing regex pass)
   - Check if the token exists in `mStyleValues->allKeys()`
   - If missing, emit `qWarning() << "Theme:" << themeName << "- QSS token" << token << "not found in values.ini"`
3. Collect all known keys from `mStyleValues->allKeys()` into a `QSet<QString>` for O(1) lookup

**Implementation sketch:**
```cpp
// --- Token validation: check QSS tokens against values.ini ---
{
    static const QRegularExpression tokenRx(QStringLiteral("@([a-zA-Z][a-zA-Z0-9_]*)"));
    static const QRegularExpression dpTokenRx(QStringLiteral("^dp\\d+$"));
    const QStringList allKeys = mStyleValues->allKeys();
    const QSet<QString> knownTokens(allKeys.begin(), allKeys.end());

    QRegularExpressionMatchIterator it = tokenRx.globalMatch(mStylesheetFileContent);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString tokenName = m.captured(1);  // without @
        QString fullToken = m.captured(0);  // with @

        // Skip @dpN tokens (handled by DPI scaling pass)
        if (dpTokenRx.match(tokenName).hasMatch())
            continue;

        if (!knownTokens.contains(fullToken))
            qWarning() << "Theme:" << themeName << "- QSS token" << fullToken << "not found in values.ini";
    }
}
```

**Acceptance criteria:**
- [x] Adding a fake `@missingToken` to `style.qss` produces a `qWarning` at runtime
- [x] All existing tokens resolve cleanly with no warnings
- [x] `@dpN` tokens do not trigger warnings
- [x] No performance impact (runs once per theme load, not per frame)

---

### Task 3.2: Validate color format in values.ini

**Location:** Same method, immediately after the token validation block (still before the replacement loop).

**What to do:**

1. Iterate over `mStyleValues->allKeys()`
2. Skip the `@themeName` token (its value is `"default"` or `"light"`, not a color)
3. For each remaining token, get its value and validate:
   - Must start with `#`
   - Remaining characters must be valid hex digits (`[0-9a-fA-F]`)
   - Length must be 4 (e.g. `#abc`), 5 (`#abcd`), 7 (`#aabbcc`), or 9 (`#aabbccdd`) characters total
4. Emit `qWarning()` for malformed values

**Implementation sketch:**
```cpp
// --- Color format validation ---
{
    static const QRegularExpression hexColorRx(QStringLiteral("^#[0-9a-fA-F]{3}([0-9a-fA-F]{1,2})?([0-9a-fA-F]{3})?$"));
    // Matches: #rgb, #rgba, #rrggbb, #rrggbbaa
    for (const QString &key : mStyleValues->allKeys()) {
        if (key == "@themeName")
            continue;
        QString value = mStyleValues->value(key).toString();
        if (!hexColorRx.match(value).hasMatch())
            qWarning() << "Theme:" << themeName << "- token" << key << "has invalid color value:" << value;
    }
}
```

**Acceptance criteria:**
- [x] A typo like `color01 = 1e1e1e` (missing `#`) produces a warning
- [x] A value like `#ZZZZZZ` (invalid hex) produces a warning
- [x] `@themeName = default` does NOT produce a warning
- [x] All current token values in both themes pass validation cleanly

---

### Task 3.3: Update Architecture Review

**Location:** `docs/ARCHITECTURE_REVIEW.md`

**What to do:**

1. Update weakness §7 (QSS Token Validation Gap) to note it has been addressed
2. Add a brief description of the validation approach (token scan + hex format check)
3. Note that the validation emits `qWarning()` at runtime, not compile-time

---

### Task 3.4: Build verification and tracking updates

**What to do:**

1. Run incremental build to verify compilation
2. Run the app and verify no warnings appear (current token set is in sync)
3. Mark FR-32 as `[x]` in `FEATURE_REQUESTS.md` with resolution note
4. Mark Phase 3 tasks as `[x]` in `docs/IMPLEMENTATION_ROADMAP.md`
5. Update `docs/APPLICATION_OVERVIEW.md` if relevant (theme system description)
6. Commit and push

---

## Validation Regex Details

### Why `@([a-zA-Z][a-zA-Z0-9_]*)` for token detection?

- Starts with `@` followed by a letter (matches current naming: `@color01`, `@accentColor`, `@themeName`)
- Includes digits and underscores for future-proofing
- Does NOT match `@dp8` style DPI tokens (those start with a letter `d`, so they DO match the outer regex but are filtered out by the `^dp\d+$` exclusion check)

### Why a separate `^dp\d+$` exclusion instead of negative lookahead?

- Clearer to read and maintain
- The `dp` prefix followed by only digits is unambiguous
- A hypothetical future token like `@dpSpecialColor` would NOT be excluded (correct — it's not a DPI token)

### Hex color regex: `^#[0-9a-fA-F]{3}([0-9a-fA-F]{1,2})?([0-9a-fA-F]{3})?$`

This is intentionally permissive within valid CSS color lengths:
- `#rgb` (3 hex digits) — shorthand
- `#rgba` (4 hex digits) — shorthand with alpha
- `#rrggbb` (6 hex digits) — standard
- `#rrggbbaa` (8 hex digits) — standard with alpha

Wait — that regex is overly complex. A simpler and more precise approach:

```cpp
static const QRegularExpression hexColorRx(QStringLiteral("^#(?:[0-9a-fA-F]{3}){1,2}$|^#(?:[0-9a-fA-F]{4}){1,2}$"));
```

Actually, even simpler — just check the length and hex validity separately:
```cpp
bool valid = value.startsWith('#')
    && (value.length() == 4 || value.length() == 5 || value.length() == 7 || value.length() == 9)
    && QRegularExpression("^#[0-9a-fA-F]+$").match(value).hasMatch();
```

The final implementation will use the simplest correct approach. All current values.ini entries use 7-character format (`#rrggbb`), so any length check is future-proofing.

---

## Risk Assessment

**Risk:** Zero. This adds `qWarning()` output only. No behavior change, no new files, no build changes. If validation logic has a bug, the worst case is a spurious warning in the debug log.

**Performance:** Negligible. Two regex scans over ~1285 lines of QSS and 30 INI keys, executed only when the theme loads (startup + theme switch). Not in any hot path.

---

## Estimated Effort

~30 minutes total.
