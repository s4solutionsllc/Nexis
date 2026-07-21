#ifndef RUNNING_APP_WARNING_DIALOG_H
#define RUNNING_APP_WARNING_DIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QTimer;
class PackageService;

// SSO-15566 / SSO-15373 CISO §4: blocking gate shown before trashApps()
// removes a selected app that is currently running. Design Anchor
// confirmation-dialog convention — one sentence body. "Quit App" requests a
// graceful terminate (never SIGKILL — see PackageToolMacOS::quitApp) and
// polls isAppRunning() on a bounded schedule until the process exits or the
// poll budget is exhausted, at which point Quit App can be retried. Cancel
// skips only this app; it never affects the rest of the uninstall batch.
class RunningAppWarningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunningAppWarningDialog(const QString &appName,
                                     const QString &bundlePath,
                                     PackageService *packageService = nullptr,
                                     QWidget *parent = nullptr);

private slots:
    void onQuitClicked();
    void poll();

private:
    void buildUI(const QString &appName);
    void setWaiting(bool waiting, const QString &statusText = {});

    QString mBundlePath;
    PackageService *mPackageService;

    QLabel *mLblBody = nullptr;
    QPushButton *mBtnQuit = nullptr;
    QPushButton *mBtnCancel = nullptr;
    QTimer *mPollTimer = nullptr;
    int mAttemptsRemaining = 0;
};

#endif // RUNNING_APP_WARNING_DIALOG_H
