#ifndef ORPHAN_LEFTOVERS_DIALOG_H
#define ORPHAN_LEFTOVERS_DIALOG_H

#include <QDialog>
#include <QFuture>
#include <QList>
#include <QString>
#include <QStringList>

#include <Tools/package_tool_shared.h>

class QLabel;
class QPushButton;
class QTableWidget;

// SSO-15429 (SSO-15373 §1/§5, Design Anchor SSO-1768): itemized review for
// PackageTool::findOrphanLeftovers() matches.
//
// Higher risk than the post-uninstall findAppLeftovers()/CrumbsReviewDialog/
// LeftoverReviewDialogLinux flow — there is no known just-uninstalled app to
// correlate against, only corroborating signals — so this surface adds two
// CISO gates neither of those dialogs has:
//   - no "Select All": every item requires individual review (CISO
//     Psychological Acceptability — a bulk action could silently sweep in
//     an unreviewed item).
//   - a one-sentence confirmation dialog before the trash call, on top of
//     the always-visible size+count summary.
//
// Trashing reuses PackageTool::trashLeftovers() via PackageService — the
// same T3/T4 signal-carrying, deny-list-checked, audit-logged, fail-secure
// (QFile::moveToTrash / freedesktop.org Trash, never a permanent-delete
// fallback) path the other leftover dialogs use.
class OrphanLeftoversDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OrphanLeftoversDialog(QWidget *parent = nullptr);
    // SSO-3362-class worker-lifetime backstop: waits for an in-flight scan
    // so the dialog is never destroyed mid-worker. The QPointer guard in
    // scan()'s lambdas is the primary defense (see scan()); this is belt-
    // and-suspenders, mirroring MaintenanceWizardDialog.
    ~OrphanLeftoversDialog() override;

    // Starts the scan (PackageService::findOrphanLeftovers(), off the UI
    // thread — it walks the user's home directory) and populates the table
    // when it completes. Call once after construction; separated from the
    // constructor so tests can populate() a fixture list directly instead.
    void scan();

    // One row of the itemized list, derived from an OrphanLeftover match.
    // `checked` always starts false — CISO §5: every orphan match is
    // presented unchecked, no exceptions. Pure data, no Qt widget
    // dependency, so the default-selection-state rule is unit-testable
    // without a QApplication.
    struct Row {
        QString path;
        QString category;
        quint64 size = 0;
        QStringList signalLabels;   // full matched-signal set, not just a score
        bool checked = false;
    };

    // Pure mapping from scan results to display rows. Every row's `checked`
    // is hardcoded false here — this is the single source of truth the
    // "default selection empty" requirement rests on.
    static QList<Row> buildRows(const QList<OrphanLeftover> &items);

    // One-sentence confirmation copy shown before the trash call (Design
    // Anchor SSO-1768), e.g. "Move 3 items (12.4 MB) to Trash?". No
    // permanent-delete wording — orphan scanner is trash-only.
    static QString confirmationSentence(int itemCount, quint64 totalBytes);

protected:
    // Test seam: populate the table directly instead of running the real
    // scan, mirroring the Testable* subclass pattern used for the platform
    // PackageTool scanners.
    void populate(const QList<OrphanLeftover> &items);

    QTableWidget *table() const { return mTable; }
    QPushButton *trashButton() const { return mBtnTrash; }
    QLabel *summaryLabel() const { return mLblSummary; }

private slots:
    void onTableItemChanged();
    void onMoveToTrash();

private:
    void buildUI();
    void updateSummary();
    QStringList checkedPaths() const;

    QLabel       *mLblSummary = nullptr;
    QTableWidget *mTable      = nullptr;
    QPushButton  *mBtnTrash   = nullptr;
    QPushButton  *mBtnSkip    = nullptr;

    qint64 mTotalBytes = 0;
    int    mItemCount  = 0;

    QFuture<void> mScanFuture;
};

#endif // ORPHAN_LEFTOVERS_DIALOG_H
