# SSO-23859 — Cleaner action interpreter + sandboxed execution engine

Status: **done**. SSO-23856 (CleanerML parser & internal cleaner model)
landed on `native` (#408); `CleanerActionInterpreter`
(`shared/nexis/Managers/cleaner_action_interpreter.{h,cpp}`) implements the
architecture below against the real `Cleaner`/`Action` types. Deviations from
the original plan are called out inline; see the PR for the final diff.

This doc captures the interpreter architecture so integration is a thin
wiring pass once the parser's Action struct exists, plus the piece of this
ticket that has no dependency on that struct and is done now.

## Delivered in this pass (not blocked)

`shared/nexis-core/Utils/sandboxed_path_resolver.{h,cpp}` —
`SandboxedPathResolver::resolve(baseDir, subPath, pattern, kind)` for
`MatchKind::{Glob, Walk, Regex}`. Confines every result to the canonical
(symlink- and `..`-resolved) form of `baseDir`; a traversal segment, an
absolute-path pattern, or a symlink hop inside the sandbox all get silently
dropped rather than surfaced. Covered by
`tests/utils/test_sandboxed_path_resolver.cpp`, including the AC-required
crafted-traversal-cannot-escape case and a symlink-escape case for both
directories (not followed during Walk/Regex) and files (dropped by the
canonical confinement check). This satisfies the "path resolution is
confined to the resolved match set" acceptance criterion independently of
how SSO-23856 shapes its Action model.

Action executors do their own filesystem/directory walking; they call this
resolver rather than using `QDir`/`QDirIterator` directly.

## Planned architecture (once SSO-23856 lands)

### CleanerActionInterpreter implements TrustSafetyActionProvider

`shared/nexis/Common/trust_safety_types.h` (SSO-15380) already defines the
exact seam this ticket needs: `scan()` discovers `TrustSafetyActionItem`s
incrementally, `performItem(item, dryRun)` performs or simulates one. The
existing `TrustSafetyRunner` + `TrustSafetyPreviewDialog` give dry-run
preview, live-confirm, cancel, and progress for free — per the PO
instruction, this ticket must not build a new confirmation dialog, and with
this interface it doesn't need to.

```
class CleanerActionInterpreter : public TrustSafetyActionProvider {
public:
    CleanerActionInterpreter(const Cleaner &cleaner, QStringList selectedOptionIds);

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override;
    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override;
};
```

`scan()` walks the cleaner's selected-option actions, dispatches each by
`Action::Kind`, and reports one `TrustSafetyActionItem` per resolved file (or
per DB for `sqlite.vacuum`) with `estimatedSizeBytes` filled in — this is
the "byte-size estimate before any destructive action runs" AC. `item.id`
encodes enough to re-locate the target in `performItem` (e.g. the canonical
path, or `dbPath + "#vacuum"`).

### Action kind → executor mapping

| CleanerML action | Executor behavior |
|---|---|
| `delete` (literal path) | Confine the literal path to the sandbox root via `SandboxedPathResolver::isPathConfinedTo`; one item. |
| `glob` | `SandboxedPathResolver::resolve(..., MatchKind::Glob)`; one item per match. |
| `walk` | `SandboxedPathResolver::resolve(..., MatchKind::Walk)`; one item per match. |
| `regex` | `SandboxedPathResolver::resolve(..., MatchKind::Regex)`; one item per match. |
| `truncate` | Resolve like `delete`/`glob`, but `performItem` opens with `QIODevice::WriteOnly \| QIODevice::Truncate` (or `ftruncate`) instead of removing the file; dry-run reports current size as bytes that would be freed. |
| `sqlite.vacuum` | See below. `winreg` is out of scope per SSO-23856 (skipped at parse time, never reaches the interpreter). |

### Sandbox root

Root is `QStandardPaths::HomeLocation` unioned with the platform cache dir
(`QStandardPaths::CacheLocation`) — never the filesystem root. Every
`Action`'s configured path must resolve under one of these before any
executor runs; anything else is dropped at `scan()` time and never reaches
`performItem()`, i.e. the confinement check gates discovery, not just
execution.

**Resolved:** SSO-23856's parser leaves CleanerML variable tokens
(`$$home$$`, `$$profile$$`, ...) as literal `$$var$$` text in `Action::path`
— nothing arrives pre-substituted. The interpreter expands only the two
generic tokens, `$$home$$`/`$$cache$$` (case-insensitive); an app-specific
token such as `$$profile$$` (browser-profile discovery, real BleachBit
cleaners use this constantly) has no resolution here and the action is
dropped at scan time. That profile-aware resolution is SSO-23860's job.

### sqlite.vacuum

**Deviation from the original plan:** the landed `CleanerML::Action` model
(SSO-23856) has no field for a delete predicate — `type`, `path`, `regex`,
`command`, `search` only. Real CleanerML's `sqlite.vacuum` command is
VACUUM-only in BleachBit too; row/table deletion is a separate, unmodeled
command class. So `performItem` for a vacuum action, `dryRun == false`:
1. Confirm the DB path is confined to the sandbox root (already gated at
   `scan()`, re-checked here defensively).
2. Open via a scoped `QSqlDatabase` connection (unique connection name per
   call, removed after use — no shared/global handle).
3. Run `VACUUM` (must run outside any explicit transaction).
4. Report `bytesFreed` as the DB file's size delta (stat before/after).

`dryRun == true` never opens the DB for writing; it estimates reclaimable
bytes via `PRAGMA freelist_count * PRAGMA page_size` on a read-only
connection — SQLite's own way of predicting VACUUM's effect without running
it. Unit test: throwaway fixture DB (a `QTemporaryDir` + freshly created
`.sqlite` file, not a shared fixture) with rows inserted then deleted to
produce free pages; asserts `VACUUM` actually shrinks the file and that
dry-run touches neither its size nor its row count.

### Live execution confirm

`TrustSafetyRunner::startExecution(items, dryRun=false)` is only reachable
from the existing `TrustSafetyPreviewDialog` confirm action — the page
wiring (likely `SystemCleanerPage` or a new Deep Cleaning entry point, TBD
by whoever lands SSO-23856's UI surface) constructs the interpreter, hands
it to a `TrustSafetyPreviewDialog`, and never calls `performItem`/
`startExecution` directly. No new dialog, per PO instruction.

## Integration checklist

- [x] Confirmed `Action` struct field names/kinds from SSO-23856 match the
      table above.
- [x] Confirmed SSO-23856's parser leaves CleanerML variables as literal
      tokens; the interpreter expands `$$home$$`/`$$cache$$` only.
- [x] Implemented `CleanerActionInterpreter` in `shared/nexis/Managers/`
      (matches `cleaner_service.h`'s convention; `Common/` is reserved for
      the domain-agnostic `TrustSafetyActionProvider` seam itself).
- [x] Unit tests per action kind — hand-built fixtures constructed directly
      in C++ (not SSO-23858's XML fixture corpus, which is still in
      progress and doesn't gate this ticket's own action-type coverage).
- [x] Updated `docs/APPLICATION_OVERVIEW.md` / `CHANGELOG.md` for the
      interpreter itself (no UI entry point yet — that update is still
      pending whoever wires this into a page).

## Follow-on work (not this ticket)

- Wire `CleanerActionInterpreter` into a Deep Cleaning Engine page/entry
  point (constructs the interpreter + selected options, hands it to a
  `TrustSafetyPreviewDialog`).
- App-specific variable resolution (`$$profile$$` and friends) for browser
  cleaners — SSO-23860.
- Surgical sqlite row/table deletion (e.g. cookie cleanup) as its own
  command class, since `sqlite.vacuum` alone doesn't carry that config —
  also SSO-23860 territory.
