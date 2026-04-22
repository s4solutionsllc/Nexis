#ifndef ROOTKIT_SCANNER_WIDGET_H
#define ROOTKIT_SCANNER_WIDGET_H

#ifdef Q_OS_LINUX

#include <QWidget>

class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QProcess;

// FR-122: Optional frontend for chkrootkit / rkhunter. Card is only created in
// HelpersPage when at least one scanner binary is present on the system. Runs
// the scanner via pkexec (root required) and streams its stdout/stderr to a
// QPlainTextEdit in real-time. A one-line summary ("Clean" / "Issues found")
// is produced by a simple post-scan string search — no brittle line parsing.
class RootKitScannerWidget : public QWidget
{
    Q_OBJECT

    enum class ScanState { Idle, Scanning, Done, Error };

public:
    explicit RootKitScannerWidget(QWidget *parent = nullptr);
    ~RootKitScannerWidget() override;

    void loadIfNeeded();

private slots:
    void onScanClicked();
    void onCancelClicked();
    void onReadyRead();
    void onReadyReadStderr();
    void onScanFinished(int exitCode, int exitStatus);
    void refreshThemeColors();

private:
    void buildUI();
    void setState(ScanState s);

    QString mTool;

    QFrame        *mCard        = nullptr;
    QLabel        *mLblTitle    = nullptr;
    QLabel        *mLblIntro    = nullptr;
    QPlainTextEdit *mOutput     = nullptr;
    QLabel        *mLblSummary  = nullptr;
    QPushButton   *mBtnScan     = nullptr;
    QPushButton   *mBtnCancel   = nullptr;

    QProcess *mProcess     = nullptr;
    QString   mLineBuffer;
    QString   mFullOutput;
    ScanState mState       = ScanState::Idle;
    bool      mLoaded      = false;
};

#endif // Q_OS_LINUX
#endif // ROOTKIT_SCANNER_WIDGET_H
