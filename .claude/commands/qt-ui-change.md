# Qt UI Change Checklist

Run through the Qt/QSS verification checklist after making UI modifications.

## Arguments

$ARGUMENTS — Optional: description of what was changed (e.g., "added new settings group box").

## Checklist

### Theme Compliance
- [ ] No hardcoded hex colors in C++ — all colors from `values.ini` tokens via `AppManager::getStyleValues()`
- [ ] No hardcoded hex colors in inline `setStyleSheet()` calls — use object names + QSS rules instead
- [ ] New widgets have `setObjectName()` for QSS targeting
- [ ] If widget uses `QPainter`, colors resolved from theme tokens (not `palette()`)

### QScrollArea Viewport (if applicable)
When using `QScrollArea` in programmatic dialogs, apply the transparent pattern:
```cpp
scrollArea->setFrameShape(QFrame::NoFrame);
scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
scrollWidget->setStyleSheet("background-color:transparent;");
```

### Theme Token System
- [ ] New color values added to BOTH `default/style/values.ini` AND `light/style/values.ini`
- [ ] Token names use camelCase matching existing conventions (e.g., `@cardBg`, `@accentColor`)
- [ ] No token name is a substring of another (causes replacement collisions — see BUG-49)
- [ ] If new `@dpN` tokens needed, they follow the DPI scaling pattern

### Live Theme Refresh
- [ ] Widget implements `refreshThemeColors()` connected to `SignalMapper::sigChangedAppTheme`
- [ ] Constructor accepts token name strings, not resolved `QColor` values
- [ ] `refreshThemeColors()` re-resolves all color tokens from `AppManager::getStyleValues()`

### Cross-Platform
- [ ] No `#ifdef Q_OS_*` in shared UI code unless absolutely necessary
- [ ] Tested on macOS (both Intel and Apple Silicon if available)
- [ ] `QToolButton` used instead of `QPushButton` for icon-only buttons on macOS (see BUG-52)

### Dynamic Properties (QSS)
- [ ] `unpolish()` + `polish()` called after changing dynamic properties (see BUG-56)
- [ ] Child widgets re-polished when parent property changes

### Build Verification
Run `/build-verify` to confirm build and tests pass after UI changes.

### Signal Count
If new signals added to `SignalMapper`, update the count in `docs/ARCHITECTURE_REVIEW.md`.
