#ifndef CLEANING_HISTORY_DIALOG_H
#define CLEANING_HISTORY_DIALOG_H

#include <QDialog>
#include <QString>
#include <Tools/lifecycle_audit_log.h>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// "Recently removed": every System Cleaner run (interactive, scheduled,
// headless) and lifecycle cleanup recorded in LifecycleAuditLog, with the
// items each one removed or skipped. Items that went to the Trash can be put
// back with Restore.
class CleaningHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CleaningHistoryDialog(QWidget *parent = nullptr);

    // User-facing text for an Entry::runSource value (empty = lifecycle).
    static QString runSourceLabel(const QString &runSource);
    // User-facing text for an Entry::skipReason value.
    static QString skipReasonLabel(const QString &skipReason);
    static QString statusLabel(const LifecycleAuditLog::Entry &entry,
                               bool restored);

private slots:
    void onSelectionChanged();
    void onRestoreSelected();

private:
    void buildUI();
    void refreshList();
    const LifecycleAuditLog::Entry *selectedRestorableEntry() const;

    QLabel *mLblTitle;
    QLabel *mLblNotice;
    QTreeWidget *mTree;
    QPushButton *mBtnRestore;
    QPushButton *mBtnClose;

    QList<LifecycleAuditLog::Run> mRuns;
};

#endif // CLEANING_HISTORY_DIALOG_H
