#include "helpers_page.h"
#include "network_diag_widget.h"
#include "open_ports_widget.h"
#include "firewall_widget.h"
#include "ui_helpers_page.h"

#include <Utils/command_util.h>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#ifdef Q_OS_MACOS
#include <QDialog>
#include <QLabel>
#include <QPlainTextEdit>
#else
#include <QLabel>
#include <Managers/info_manager.h>
#include <Info/power_profile_info.h>
#endif

HelpersPage::~HelpersPage()
{
    delete ui;
}

HelpersPage::HelpersPage(QWidget *parent) :
    QWidget(parent),
    widgetHostManage(new HostManage),
    mNetworkDiagWidget(new NetworkDiagWidget),
    mOpenPortsWidget(new OpenPortsWidget),
    mFirewallWidget(new FirewallWidget),
    ui(new Ui::HelpersPage)
{
    ui->setupUi(this);

    init();
}

void HelpersPage::init()
{
    ui->stackedWidget->addWidget(widgetHostManage);
    ui->stackedWidget->addWidget(mNetworkDiagWidget);
    ui->stackedWidget->addWidget(mOpenPortsWidget);
    ui->stackedWidget->addWidget(mFirewallWidget);

    // Prevent buttons from shrinking below their text width
    for (auto *btn : {ui->btnHostManage, ui->btnFlushDNS, ui->btnNetDiag,
                      ui->btnOpenPorts, ui->btnFirewall}) {
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    QList<QWidget *> shadowWidgets = {
        ui->btnHostManage,
        ui->btnFlushDNS,
        ui->btnNetDiag,
        ui->btnOpenPorts,
        ui->btnFirewall
    };

#ifdef Q_OS_MACOS
    mBtnRebuildSpotlight = new QPushButton(tr("Rebuild Spotlight"));
    mBtnVerifyDisk = new QPushButton(tr("Verify Disk"));
    mBtnRebuildLaunchServices = new QPushButton(tr("Rebuild Launch Services"));

    for (auto *btn : {mBtnRebuildSpotlight, mBtnVerifyDisk, mBtnRebuildLaunchServices}) {
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    shadowWidgets << mBtnRebuildSpotlight << mBtnVerifyDisk << mBtnRebuildLaunchServices;

    connect(mBtnRebuildSpotlight, &QPushButton::clicked, this, &HelpersPage::onRebuildSpotlight);
    connect(mBtnVerifyDisk, &QPushButton::clicked, this, &HelpersPage::onVerifyDisk);
    connect(mBtnRebuildLaunchServices, &QPushButton::clicked, this, &HelpersPage::onRebuildLaunchServices);
#else
    initPowerProfileUI();
    if (mPowerProfileWidget)
        shadowWidgets << mPowerProfileWidget;
#endif

    Utilities::addDropShadow(shadowWidgets, 40);

    // Collect all nav items in display order
    mNavItems << ui->btnHostManage << ui->btnFlushDNS << ui->btnNetDiag
              << ui->btnOpenPorts << ui->btnFirewall;
#ifdef Q_OS_MACOS
    mNavItems << mBtnRebuildSpotlight << mBtnVerifyDisk << mBtnRebuildLaunchServices;
#else
    if (mPowerProfileWidget)
        mNavItems << mPowerProfileWidget;
#endif

    // Replace the static .ui navLayout with a managed one and compute the wrap threshold
    applyNavLayout(false);
    computeNavMinWidth();
}

void HelpersPage::on_btnHostManage_clicked()
{
    widgetHostManage->loadIfNeeded();
    ui->stackedWidget->setCurrentIndex(0);
}

void HelpersPage::on_btnNetDiag_clicked()
{
    mNetworkDiagWidget->runTestIfNeeded();
    ui->stackedWidget->setCurrentIndex(1);
}

void HelpersPage::on_btnOpenPorts_clicked()
{
    mOpenPortsWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentIndex(2);
}

void HelpersPage::on_btnFirewall_clicked()
{
    mFirewallWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentIndex(3);
}

void HelpersPage::on_btnFlushDNS_clicked()
{
    if (QMessageBox::question(this, tr("Flush DNS Cache"),
            tr("This will clear the local DNS cache. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    QString errorMsg;
    bool success = false;

#ifdef Q_OS_MACOS
    try {
        CommandUtil::exec("dscacheutil", {"-flushcache"});
        CommandUtil::sudoExec("killall", {"-HUP", "mDNSResponder"});
        success = true;
    } catch (const QString &ex) {
        errorMsg = ex;
    }
#else
    // Linux: try resolvers in order of likelihood
    if (CommandUtil::isExecutable("resolvectl")) {
        ExecResult r = CommandUtil::execWithStatus("resolvectl", {"flush-caches"});
        success = (r.exitCode == 0);
        if (!success) errorMsg = r.error;
    } else if (CommandUtil::isExecutable("systemd-resolve")) {
        ExecResult r = CommandUtil::execWithStatus("systemd-resolve", {"--flush-caches"});
        success = (r.exitCode == 0);
        if (!success) errorMsg = r.error;
    } else if (CommandUtil::isExecutable("nscd")) {
        ExecResult r = CommandUtil::execWithStatus("nscd", {"-i", "hosts"});
        success = (r.exitCode == 0);
        if (!success) errorMsg = r.error;
    } else {
        errorMsg = tr("No DNS cache service detected (systemd-resolved, nscd).");
    }
#endif

    if (success) {
        QMessageBox::information(this, tr("DNS Cache Flushed"),
            tr("The local DNS cache has been cleared successfully."));
    } else {
        QMessageBox::warning(this, tr("DNS Flush Failed"),
            tr("Could not flush DNS cache: %1").arg(errorMsg));
    }
}

void HelpersPage::onRebuildSpotlight()
{
#ifdef Q_OS_MACOS
    if (QMessageBox::question(this, tr("Rebuild Spotlight Index"),
            tr("This will delete and rebuild the Spotlight search index.\n\n"
               "Spotlight search will be temporarily unavailable while the index "
               "rebuilds. This may take 30 minutes to several hours depending on "
               "your disk size.\n\nContinue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    QString errorMsg;
    bool success = false;

    try {
        CommandUtil::sudoExec("mdutil", {"-E", "/"});
        success = true;
    } catch (const QString &ex) {
        errorMsg = ex;
    }

    if (success) {
        QMessageBox::information(this, tr("Spotlight Index Rebuild Started"),
            tr("The Spotlight index rebuild has been triggered. "
               "Reindexing will continue in the background."));
    } else {
        QMessageBox::warning(this, tr("Spotlight Rebuild Failed"),
            tr("Could not rebuild Spotlight index: %1").arg(errorMsg));
    }
#endif
}

void HelpersPage::onVerifyDisk()
{
#ifdef Q_OS_MACOS
    if (QMessageBox::question(this, tr("Verify Disk"),
            tr("This will verify the integrity of the startup disk.\n\n"
               "This may take 1" "\xe2\x80\x93" "5 minutes. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    ExecResult result = CommandUtil::execWithStatus("diskutil", {"verifyVolume", "/"}, 300000);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Disk Verification Results"));
    dlg.setMinimumSize(500, 350);
    dlg.setObjectName("verifyDiskDialog");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLabel *lblTitle = new QLabel(tr("Disk Verification Results"));
    lblTitle->setProperty("accessibleName", "dialog-title");
    layout->addWidget(lblTitle);

    QPlainTextEdit *txtOutput = new QPlainTextEdit;
    txtOutput->setReadOnly(true);
    QString output = result.output;
    if (!result.error.isEmpty()) {
        if (!output.isEmpty())
            output += "\n\n";
        output += result.error;
    }
    txtOutput->setPlainText(output);
    layout->addWidget(txtOutput);

    QLabel *lblStatus = new QLabel;
    lblStatus->setObjectName("verifyDiskStatus");
    if (result.exitCode == 0) {
        lblStatus->setText(tr("\xe2\x9c\x93 Disk appears to be OK"));
        lblStatus->setProperty("status", "success");
    } else {
        lblStatus->setText(tr("\xe2\x9c\x97 Issues detected (exit code %1)").arg(result.exitCode));
        lblStatus->setProperty("status", "error");
    }
    layout->addWidget(lblStatus);

    QPushButton *btnClose = new QPushButton(tr("Close"));
    btnClose->setProperty("accessibleName", "primary");
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    layout->addLayout(btnRow);

    dlg.exec();
#endif
}

void HelpersPage::onRebuildLaunchServices()
{
#ifdef Q_OS_MACOS
    if (QMessageBox::question(this, tr("Rebuild Launch Services"),
            tr("This will rescan the Launch Services database and restart Finder.\n\n"
               "This can fix issues with incorrect default apps and missing "
               "'Open With' entries.\n\nContinue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    QString lsregister = "/System/Library/Frameworks/CoreServices.framework"
                         "/Frameworks/LaunchServices.framework/Support/lsregister";

    QString errorMsg;
    bool success = false;

    ExecResult r = CommandUtil::execWithStatus(lsregister,
        {"-r", "-domain", "local", "-domain", "system", "-domain", "user"}, 60000);

    if (r.exitCode == 0) {
        ExecResult r2 = CommandUtil::execWithStatus("killall", {"Finder"});
        if (r2.exitCode == 0) {
            success = true;
        } else {
            errorMsg = tr("Database rebuilt but Finder restart failed: %1").arg(r2.error);
        }
    } else {
        errorMsg = r.error.isEmpty() ? r.output : r.error;
    }

    if (success) {
        QMessageBox::information(this, tr("Launch Services Rebuilt"),
            tr("The Launch Services database has been rebuilt and Finder restarted."));
    } else {
        QMessageBox::warning(this, tr("Launch Services Rebuild Failed"),
            tr("Could not rebuild Launch Services: %1").arg(errorMsg));
    }
#endif
}

void HelpersPage::onPowerProfileClicked()
{
#ifndef Q_OS_MACOS
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    QString label = btn->text();
    PowerProfileData data = InfoManager::ins()->getPowerProfileData();
    QString backendVal = PowerProfileInfo::userLabelToBackendValue(label, data.backend);

    if (backendVal == data.activeProfile)
        return;

    bool ok = InfoManager::ins()->setPowerProfile(backendVal);
    if (ok) {
        InfoManager::ins()->refreshPowerProfile();
        updatePowerProfileButtons();
    } else {
        QMessageBox::warning(this, tr("Power Profile"),
            tr("Failed to set power profile to \"%1\".").arg(label));
        updatePowerProfileButtons();
    }
#endif
}

#ifndef Q_OS_MACOS
void HelpersPage::initPowerProfileUI()
{
    if (!InfoManager::ins()->hasPowerProfiles())
        return;

    InfoManager::ins()->refreshPowerProfile();
    PowerProfileData data = InfoManager::ins()->getPowerProfileData();

    mPowerProfileWidget = new QWidget;
    mPowerProfileWidget->setObjectName("powerProfileWidget");
    QHBoxLayout *layout = new QHBoxLayout(mPowerProfileWidget);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    QLabel *lbl = new QLabel(tr("Power Profile:"));
    lbl->setObjectName("powerProfileLabel");
    layout->addWidget(lbl);

    mBtnPowerSaver = new QPushButton(tr("Power Saver"));
    mBtnBalanced = new QPushButton(tr("Balanced"));
    mBtnPerformance = new QPushButton(tr("Performance"));

    for (auto *btn : {mBtnPowerSaver, mBtnBalanced, mBtnPerformance}) {
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCheckable(true);
        layout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, &HelpersPage::onPowerProfileClicked);
    }

    bool hasBalanced = false;
    for (const QString &p : data.availableProfiles) {
        QString userLabel = PowerProfileInfo::backendValueToUserLabel(p, data.backend);
        if (userLabel == "Balanced") {
            hasBalanced = true;
            break;
        }
    }
    if (!hasBalanced)
        mBtnBalanced->hide();

    if (!data.conflictWarning.isEmpty()) {
        mLblConflictWarning = new QLabel(data.conflictWarning);
        mLblConflictWarning->setObjectName("powerProfileWarning");
        ui->gridLayout->addWidget(mLblConflictWarning, 1, 0, 1, 3);
    }

    updatePowerProfileButtons();
}

void HelpersPage::updatePowerProfileButtons()
{
    PowerProfileData data = InfoManager::ins()->getPowerProfileData();
    QString activeLabel = PowerProfileInfo::backendValueToUserLabel(
        data.activeProfile, data.backend);

    for (auto *btn : {mBtnPowerSaver, mBtnBalanced, mBtnPerformance}) {
        if (!btn)
            continue;
        btn->setChecked(btn->text() == activeLabel);
    }

}
#endif

void HelpersPage::computeNavMinWidth()
{
    mNavMinWidth = qMax(0, mNavItems.count() - 1) * 12;
    for (auto *w : mNavItems)
        mNavMinWidth += w->sizeHint().width();
    mNavMinWidth += 20;
}

void HelpersPage::applyNavLayout(bool compact)
{
    delete ui->nav->layout();
    mNavCompact = compact;

    if (!compact) {
        auto *row = new QHBoxLayout(ui->nav);
        row->setSpacing(12);
        row->setContentsMargins(0, 0, 0, 0);
        for (auto *w : mNavItems) {
            w->setParent(ui->nav);
            row->addWidget(w, 0, Qt::AlignLeft);
        }
        row->addStretch();
    } else {
        auto *col = new QVBoxLayout(ui->nav);
        col->setSpacing(8);
        col->setContentsMargins(0, 0, 0, 0);

        // Determine split index: macOS (8 items) = 5, Linux (5 items) = 3
        int splitIndex;
        if (mNavItems.count() >= 8)
            splitIndex = 5;
        else if (mNavItems.count() >= 5)
            splitIndex = 3;
        else
            splitIndex = (mNavItems.count() + 1) / 2;
        splitIndex = qBound(1, splitIndex, mNavItems.count() - 1);

        auto *row1 = new QHBoxLayout();
        row1->setSpacing(12);
        for (int i = 0; i < splitIndex; ++i) {
            mNavItems[i]->setParent(ui->nav);
            row1->addWidget(mNavItems[i], 0, Qt::AlignLeft);
        }
        row1->addStretch();
        col->addLayout(row1);

        auto *row2 = new QHBoxLayout();
        row2->setSpacing(12);
        for (int i = splitIndex; i < mNavItems.count(); ++i) {
            mNavItems[i]->setParent(ui->nav);
            row2->addWidget(mNavItems[i], 0, Qt::AlignLeft);
        }
        row2->addStretch();
        col->addLayout(row2);
    }
}

void HelpersPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (mNavItems.isEmpty())
        return;
    const bool compact = width() < mNavMinWidth;
    if (compact != mNavCompact)
        applyNavLayout(compact);
}
