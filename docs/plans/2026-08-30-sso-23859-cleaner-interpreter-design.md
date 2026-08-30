# SSO-23859 — Cleaner action interpreter + sandboxed execution engine

Status: **blocked on SSO-23856** (CleanerML parser & internal cleaner model)
landing a working `Cleaner`/`Action` type. Per PO comment on SSO-23858, the
intended epic (SSO-15366) build order is parser → {fixture corpus, action
interpreter}. `SSO-23859` now carries a formal `blockedBy` link to SSO-23856.

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

Root is `QStandardPaths::HomeLocation` unioned with the platform cache dirs
(`QStandardPaths::CacheLocation`/`GenericCacheLocation`) — never the
filesystem root. Every `Action`'s configured path must resolve under one of
these before any executor runs; anything else is dropped at `scan()` time
and never reaches `performItem()`, i.e. the confinement check gates
discovery, not just execution. Needs alignment with SSO-23856 on whether
CleanerML variable tokens (`$HOME`, `$XDG_CACHE_HOME`, ...) arrive
pre-substituted from the parser or as literal tokens the interpreter must
expand — open question, see Integration checklist below.

### sqlite.vacuum

`performItem` for a vacuum action, `dryRun == false`:
1. Confirm the DB path is confined to the sandbox root (already gated at
   `scan()`, re-checked here defensively).
2. Open via a scoped `QSqlDatabase` connection (unique connection name per
   call, removed after use — no shared/global handle).
3. Run the action's configured `DELETE FROM ... WHERE ...` (rows) or
   `DROP TABLE` (tables) per the action config, inside a transaction.
4. `VACUUM`.
5. Report `bytesFreed` as the DB file's size delta (stat before/after).

`dryRun == true` opens the DB read-only, runs the row/table `SELECT`
equivalent of the delete predicate to count what *would* be removed, and
never runs `DELETE`/`DROP`/`VACUUM`. Unit test: throwaway fixture DB (a
`QTemporaryDir` + freshly created `.sqlite` file, not a shared fixture),
assert row/table counts pre/post and confirm `VACUUM` actually shrinks the
file for a DB with deleted rows.

### Live execution confirm

`TrustSafetyRunner::startExecution(items, dryRun=false)` is only reachable
from the existing `TrustSafetyPreviewDialog` confirm action — the page
wiring (likely `SystemCleanerPage` or a new Deep Cleaning entry point, TBD
by whoever lands SSO-23856's UI surface) constructs the interpreter, hands
it to a `TrustSafetyPreviewDialog`, and never calls `performItem`/
`startExecution` directly. No new dialog, per PO instruction.

## Integration checklist for whoever picks this back up

- [ ] Confirm `Action` struct field names/kinds from SSO-23856 match the
      table above (adjust executor dispatch, not `SandboxedPathResolver`).
- [ ] Confirm whether SSO-23856's parser resolves CleanerML `$HOME`-style
      variables, or leaves that to the interpreter.
- [ ] Implement `CleanerActionInterpreter` in
      `shared/nexis/Common/` (co-located with `trust_safety_*`) or
      `shared/nexis/Managers/`, matching whichever convention SSO-23856's
      `Cleaner`/`Option` types land in.
- [ ] Unit tests per action kind against SSO-23858's fixture corpus once
      that ticket lands (currently also in progress).
- [ ] Update `docs/APPLICATION_OVERVIEW.md` / `CHANGELOG.md` when the UI
      entry point ships — not part of this ticket's scope alone.
