# Trust & Safety shared component (SSO-15380)

Reusable Qt component for any maintenance/cleaning surface (System Cleaner,
Uninstaller, Disk Tools, Helpers, ...) that needs, for free:

1. **Explain-before-run** — plain-English description + exact command, always visible.
2. **Itemized preview** — categorized, checkbox-driven, per-item + category + global select-all.
3. **Working Stop/cancel** — actually halts the in-flight scan/execution, not just the dialog.
4. **Live progress** — real partial progress during scan and execution.
5. **Dry-run mode** — same preview pipeline, zero side effects.
6. **Risky-category warnings** — distinct styling + an explicit extra confirmation step.

It also bakes in every binding constraint from the Design Anchor
([SSO-1785](/SSO/issues/SSO-1785) `design` doc) — one-sentence confirmation
copy, size+count summaries, incremental category sizes, always-visible
primary/secondary actions, the thin anchored progress bar, and the
red-accent-for-destructive / status-bar rules — so adopters don't have to
re-derive them per surface.

This issue ships the component only. No existing cleaning surface is
modified here; adoption is scoped to separate per-surface issues under the
epic ([SSO-15364](/SSO/issues/SSO-15364)).

## Files

- `trust_safety_types.h` — data model: `TrustSafetyActionItem`,
  `TrustSafetyActionResult`, `TrustSafetyRunSummary`, and the
  `TrustSafetyActionProvider` interface adopters implement.
- `trust_safety_runner.{h,cpp}` — async scan/execute orchestration
  (`TrustSafetyRunner`), mirroring the existing `DirSizeScanner` pattern
  (`shared/nexis/Managers/dir_size_scanner.h`): a `QObject` worker that runs
  off the UI thread via `QtConcurrent`, polls a `QAtomicInt` cancel flag
  between units of work, and exposes the pure logic as static synchronous
  helpers (`scanSynchronous()` / `executeSynchronous()`) for unit testing
  without a thread or `QApplication`.
- `trust_safety_preview_dialog.{h,cpp}` — the `QDialog` UI: itemized tree,
  explain-before-run columns, dry-run toggle, progress bar, status bar,
  Stop/Close button, risky-category confirmation.

## Adopting it on a new surface

1. **Implement a provider.** Subclass `TrustSafetyActionProvider`:

   ```cpp
   class MyCleanerProvider : public TrustSafetyActionProvider {
   public:
       void scan(QAtomicInt *cancelled,
                 const std::function<void(const TrustSafetyActionItem &)> &itemFound) override {
           // Discover candidate items. Call itemFound() as soon as each item
           // (and its size) is known — don't buffer until the end. Check
           // `cancelled` between units of work and return promptly once set;
           // this is what makes Stop real instead of a UI-only dismiss.
       }

       TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override {
           // Perform the real operation, or simulate it with zero side
           // effects when dryRun is true. Return succeeded + bytesFreed (or
           // an error string). Called once per selected item — the runner
           // checks the cancel flag between calls, so keep one call short.
       }
   };
   ```

2. **Build `TrustSafetyActionItem`s** from your domain data: `label`,
   `description` (one plain-English sentence), `command` (the exact
   underlying operation, e.g. `rm -rf ~/Library/Caches/com.example.app`),
   `categoryId`/`categoryLabel`, `riskTier`, and `estimatedSizeBytes` (`-1`
   if not yet known — the preview renders it as pending and fills it in
   once `scan()` reports it).

3. **Open the dialog:**

   ```cpp
   MyCleanerProvider provider;
   TrustSafetyPreviewDialog::Config config;
   config.windowTitle = tr("Clean App Caches");
   config.primaryActionLabel = tr("Clean Selected");
   // One sentence. Debug builds assert this — see Design Anchor above.
   config.confirmationSentence = tr("This will permanently delete the selected items.");

   TrustSafetyPreviewDialog dialog(&provider, config, this);
   dialog.exec();

   const TrustSafetyRunSummary summary = dialog.lastRunSummary();
   // summary.cancelled / summary.dryRun / summary.totalBytesFreed / summary.results
   ```

   The dialog starts scanning immediately on construction; the caller just
   `exec()`s it. `dialog.lastRunSummary()` reports whatever the most recent
   execution pass did (empty if the user closed the dialog before running
   anything).

## Notes for provider authors

- `scan()` and `performItem()` are called from a worker thread (via
  `QtConcurrent`), not the UI thread — keep your provider's state
  thread-safe, and don't touch widgets from inside them.
- Mark higher-risk items (e.g. system caches vs. browser cookies) with
  `TrustSafetyActionItem::RiskTier::Risky` — the dialog handles the visual
  distinction and the extra confirmation gate; you don't need to build that
  per surface.
- Real items that `performItem()` reports as succeeded are removed from the
  preview tree once execution finishes (they no longer exist); failed or
  not-yet-reached items (Stop was hit) stay visible so the user can retry.
