#include "running_app_warning_dialog.h"

#include "Services/package_service.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Bounded poll budget: 20 attempts x 500ms = 10s of waiting for the app to
// exit after a graceful quit request before falling back to "still running"
// and letting the user retry or cancel.
constexpr int kPollIntervalMs = 500;
constexpr int kMaxPollAttempts = 20;
}

RunningAppWarningDialog::RunningAppWarningDialog(const QString &appName,
                                                 const QString &bundlePath,
                                                 PackageService *packageService,
                                                 QWidget *parent)
    : QDialog(parent),
      mBundlePath(bundlePath),
      mPackageService(packageService ? packageService : PackageService::ins())
{
    setObjectName("runningAppWarningDialog");
    setWindowTitle(tr("Quit Before Removing"));
    setMinimumWidth(420);
    buildUI(appName);

    mPollTimer = new QTimer(this);
    mPollTimer->setInterval(kPollIntervalMs);
    connect(mPollTimer, &QTimer::timeout, this, &RunningAppWarningDialog::poll);
}

void RunningAppWarningDialog::buildUI(const QString &appName)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *lblTitle = new QLabel(tr("\"%1\" is running").arg(appName), this);
    lblTitle->setProperty("accessibleName", "dialog-title");
    lblTitle->setWordWrap(true);
    layout->addWidget(lblTitle);

    // Design Anchor: confirmation dialogs are one sentence maximum — retry/
    // cancel guidance is added to this same label once quitting is underway,
    // never as a second sentence at rest.
    mLblBody = new QLabel(
        tr("Quit \"%1\" to remove it — its files can't be deleted while it's running.").arg(appName),
        this);
    mLblBody->setWordWrap(true);
    layout->addWidget(mLblBody);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();

    mBtnCancel = new QPushButton(tr("Cancel"), this);
    mBtnCancel->setCursor(Qt::PointingHandCursor);
    connect(mBtnCancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(mBtnCancel);

    mBtnQuit = new QPushButton(tr("Quit App"), this);
    mBtnQuit->setCursor(Qt::PointingHandCursor);
    mBtnQuit->setDefault(true);
    connect(mBtnQuit, &QPushButton::clicked, this, &RunningAppWarningDialog::onQuitClicked);
    buttons->addWidget(mBtnQuit);

    layout->addLayout(buttons);
}

void RunningAppWarningDialog::setWaiting(bool waiting, const QString &statusText)
{
    mBtnQuit->setEnabled(!waiting);
    if (!statusText.isEmpty())
        mLblBody->setText(statusText);
}

void RunningAppWarningDialog::onQuitClicked()
{
    // SSO-15566 / CISO §4: graceful quit only — never SIGKILL. See
    // PackageToolMacOS::quitApp / app_quit_helper.mm.
    mPackageService->quitApp(mBundlePath);

    mAttemptsRemaining = kMaxPollAttempts;
    setWaiting(true, tr("Waiting for the app to quit…"));
    mPollTimer->start();
}

void RunningAppWarningDialog::poll()
{
    if (!mPackageService->isAppRunning(mBundlePath)) {
        mPollTimer->stop();
        accept();
        return;
    }

    if (--mAttemptsRemaining <= 0) {
        mPollTimer->stop();
        setWaiting(false, tr("Still running — try Quit App again or Cancel to skip this item."));
    }
}
