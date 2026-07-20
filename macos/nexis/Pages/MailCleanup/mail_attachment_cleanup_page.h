#ifndef MAIL_ATTACHMENT_CLEANUP_PAGE_H
#define MAIL_ATTACHMENT_CLEANUP_PAGE_H

#include <QWidget>
#include <QFuture>
#include <Tools/mail_attachment_tool.h>

class QLabel;
class QPushButton;
class QProgressBar;
class QTableWidget;
class QFrame;

// macOS-only Privacy & Secure Deletion feature (SSO-15383).
// Scans ~/Library/Mail/.../Attachments/ paths, shows a preview table with
// size/date/sender per item, then deletes on explicit user confirmation.
//
// NOT shown on Linux builds — this entire class is compiled only on Apple
// platforms (APPLE guard in CMakeLists.txt).
class MailAttachmentCleanupPage : public QWidget
{
    Q_OBJECT

public:
    explicit MailAttachmentCleanupPage(QWidget *parent = nullptr);

private slots:
    void onScanClicked();
    void onScanFinished(const MailAttachmentScanResult &result);
    void onDeleteClicked();
    void onProgress(int done, int total, const QString &currentFile);
    void onDeleteFinished(qint64 freedBytes, int deletedCount, int failedCount);
    void onSelectAll();
    void onSelectionChanged();

private:
    void buildUI();
    void setPhase(int phase); // 0=idle, 1=scanning, 2=preview, 3=deleting, 4=done
    void populateTable(const QList<MailAttachmentEntry> &entries);
    QStringList selectedPaths() const;
    void updateSummaryLabel();

    MailAttachmentTool *mTool = nullptr;

    // UI
    QLabel       *mLblTitle           = nullptr;
    QLabel       *mLblDescription     = nullptr;
    QLabel       *mLblMailWarning     = nullptr;  // Mail.app running banner
    QPushButton  *mBtnScan            = nullptr;
    QLabel       *mLblScanStatus      = nullptr;
    QProgressBar *mScanProgress       = nullptr;  // indeterminate during scan
    QTableWidget *mTable              = nullptr;
    QLabel       *mLblSummary         = nullptr;  // "N items · X MB selected"
    QPushButton  *mBtnSelectAll       = nullptr;
    QPushButton  *mBtnDelete          = nullptr;
    QProgressBar *mDeleteProgress     = nullptr;
    QLabel       *mLblResult          = nullptr;
    QFrame       *mConfirmFrame       = nullptr;  // risk-disclosure + action bar

    // Scan result cache
    QList<MailAttachmentEntry> mEntries;

    // Background task handle
    QFuture<void> mWorkerFuture;

    bool mScanInProgress   = false;
    bool mDeleteInProgress = false;
};

#endif // MAIL_ATTACHMENT_CLEANUP_PAGE_H
