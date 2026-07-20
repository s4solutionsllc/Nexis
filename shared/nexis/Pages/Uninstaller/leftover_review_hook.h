#ifndef LEFTOVER_REVIEW_HOOK_H
#define LEFTOVER_REVIEW_HOOK_H

#include <QStringList>

class QWidget;

// SSO-15385: facade so shared/nexis/Pages never includes a *_linux.h header
// directly (WI-27 architecture gate, scripts/check-pages-no-platform-headers.sh).
// Implemented in linux/nexis/Pages/Uninstaller/leftover_review_dialog_linux.cpp.
// Only called from UninstallerPage under #ifdef Q_OS_LINUX, so no macOS stub
// is required — the symbol is never referenced on that platform.
namespace LeftoverReviewHook {

// Scans for leftovers belonging to `packageNames` and, if any are found,
// opens a review dialog (parented to `parent`) so the user can move them to
// Trash. No-op if nothing is found — the caller never sees an empty dialog.
void maybeShowReviewDialog(const QStringList &packageNames, QWidget *parent);

} // namespace LeftoverReviewHook

#endif // LEFTOVER_REVIEW_HOOK_H
