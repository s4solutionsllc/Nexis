#include "helpers_page.h"
#include "ui_helpers_page.h"

#include <Utils/command_util.h>
#include <Managers/app_manager.h>
#include <QMessageBox>
#include <QPushButton>

#ifdef Q_OS_MACOS
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#else
#include <QHBoxLayout>
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
    ui(new Ui::HelpersPage)
{
    ui->setupUi(this);

    init();
}

void HelpersPage::init()
{
    ui->stackedWidget->addWidget(widgetHostManage);

    QList<QWidget *> shadowWidgets = {
        ui->btnHostManage,
        ui->btnFlushDNS
    };

#ifdef Q_OS_MACOS
    mBtnRebuildSpotlight = new QPushButton(tr("Rebuild Spotlight"));
    mBtnVerifyDisk = new QPushButton(tr("Verify Disk"));
    mBtnRebuildLaunchServices = new QPushButton(tr("Rebuild Launch Services"));

    for (auto *btn : {mBtnRebuildSpotlight, mBtnVerifyDisk, mBtnRebuildLaunchServices}) {
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
    }

    int spacerIndex = ui->navLayout->count() - 1;
    ui->navLayout->insertWidget(spacerIndex, mBtnRebuildSpotlight);
    ui->navLayout->insertWidget(spacerIndex + 1, mBtnVerifyDisk);
    ui->navLayout->insertWidget(spacerIndex + 2, mBtnRebuildLaunchServices);

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
}

void HelpersPage::on_btnHostManage_clicked()
{
    widgetHostManage->loadIfNeeded();
    ui->stackedWidget->setCurrentIndex(0);
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

    QSettings *sv = AppManager::ins()->getStyleValues();
    QLabel *lblStatus = new QLabel;
    if (result.exitCode == 0) {
        lblStatus->setText(tr("\xe2\x9c\x93 Disk appears to be OK"));
        QString c = sv ? sv->value("@successColor").toString() : "#2ec27e";
        lblStatus->setStyleSheet(QString("color: %1; font-weight: bold;").arg(c));
    } else {
        lblStatus->setText(tr("\xe2\x9c\x97 Issues detected (exit code %1)").arg(result.exitCode));
        QString c = sv ? sv->value("@destructiveColor").toString() : "#E05454";
        lblStatus->setStyleSheet(QString("color: %1; font-weight: bold;").arg(c));
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

    int spacerIndex = ui->navLayout->count() - 1;
    ui->navLayout->insertWidget(spacerIndex, mPowerProfileWidget);

    if (!data.conflictWarning.isEmpty()) {
        mLblConflictWarning = new QLabel(data.conflictWarning);
        mLblConflictWarning->setObjectName("powerProfileWarning");
        QSettings *sv = AppManager::ins()->getStyleValues();
        QString warnColor = sv ? sv->value("@warningColor").toString() : "#FFB347";
        mLblConflictWarning->setStyleSheet(
            QString("color: %1; font-size: 11px; padding: 2px 0;").arg(warnColor));
        ui->gridLayout->addWidget(mLblConflictWarning, 1, 0, 1, 3);
    }

    applyPowerProfileStyle();
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

    applyPowerProfileStyle();
}

void HelpersPage::applyPowerProfileStyle()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString accent   = sv ? sv->value("@accentColor").toString() : "#FF6B1A";
    QString cardBg   = sv ? sv->value("@cardBg").toString() : "#2A2C32";
    QString textPri  = sv ? sv->value("@color05").toString() : "#F0F2F5";
    QString textSec  = sv ? sv->value("@color04").toString() : "#9A9DA6";
    QString border   = sv ? sv->value("@borderColor").toString() : "#4A4D5A";

    QString btnStyle = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 6px 14px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:checked {"
        "  background-color: %4;"
        "  color: #ffffff;"
        "  border-color: %4;"
        "}"
        "QPushButton:hover:!checked {"
        "  border-color: %4;"
        "}"
    ).arg(cardBg, textSec, border, accent);

    for (auto *btn : {mBtnPowerSaver, mBtnBalanced, mBtnPerformance}) {
        if (btn)
            btn->setStyleSheet(btnStyle);
    }

    if (mPowerProfileWidget) {
        mPowerProfileWidget->setStyleSheet(
            QString("#powerProfileWidget { background-color: %1; border-radius: 6px; }"
                    "#powerProfileLabel { color: %2; font-size: 12px; }")
                .arg(cardBg, textPri));
    }
}
#endif
