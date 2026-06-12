#include "helpers_page.h"
#include "network_diag_widget.h"
#include "open_ports_widget.h"
#include "firewall_widget.h"
#ifdef Q_OS_LINUX
#include "swappiness_widget.h"
#include "cpu_tuning_widget.h"
#include "battery_charge_threshold_widget.h"
#include <Info/battery_charge_threshold.h>
#endif
#include "trim_widget.h"
#include "wol_widget.h"
#include "ui_helpers_page.h"

#include <Utils/command_util.h>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <functional>

namespace {
class CardFilter : public QObject {
public:
    CardFilter(QObject *parent, std::function<void()> fn) : QObject(parent), mFn(std::move(fn)) {}
    bool eventFilter(QObject *, QEvent *e) override {
        if (e->type() == QEvent::MouseButtonRelease) { mFn(); return false; }
        return false;
    }
private:
    std::function<void()> mFn;
};
} // namespace

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
#ifdef Q_OS_LINUX
    mSwappinessWidget = new SwappinessWidget;
    ui->stackedWidget->addWidget(mSwappinessWidget);

    mCpuTuningWidget = new CpuTuningWidget;
    ui->stackedWidget->addWidget(mCpuTuningWidget);

    mBatteryThresholdWidget = new BatteryChargeThresholdWidget;
    ui->stackedWidget->addWidget(mBatteryThresholdWidget);

#endif

    // FR-118: TRIM widget — cross-platform. Hide the button if fstrim isn't
    // installed on Linux.
    mTrimWidget = new TrimWidget;
    ui->stackedWidget->addWidget(mTrimWidget);

    // FR-120: Wake-on-LAN — cross-platform.
    mWolWidget = new WolWidget;
    ui->stackedWidget->addWidget(mWolWidget);

    // Prevent buttons from shrinking below their text width
    for (auto *btn : {ui->btnHostManage, ui->btnNetDiag,
                      ui->btnOpenPorts, ui->btnFirewall}) {
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    QList<QWidget *> shadowWidgets = {
        ui->btnHostManage,
        ui->btnNetDiag,
        ui->btnOpenPorts,
        ui->btnFirewall
    };

#ifdef Q_OS_MACOS
    // FR-118: TRIM button (macOS — status-only).
    mBtnTrim = new QPushButton(tr("SSD TRIM"));
    mBtnTrim->setCheckable(true);
    mBtnTrim->setCursor(Qt::PointingHandCursor);
    mBtnTrim->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->buttonGroup->addButton(mBtnTrim);
    shadowWidgets << mBtnTrim;
    connect(mBtnTrim, &QPushButton::clicked, this, &HelpersPage::onTrimClicked);
#else
    // FR-81: Swappiness button. Always visible on Linux — the widget itself
    // handles the "not supported" state if /proc/sys/vm/swappiness isn't
    // readable (extreme corner cases like hardened LSM profiles).
    mBtnSwappiness = new QPushButton(tr("Swappiness"));
    mBtnSwappiness->setCheckable(true);
    mBtnSwappiness->setCursor(Qt::PointingHandCursor);
    mBtnSwappiness->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->buttonGroup->addButton(mBtnSwappiness);
    shadowWidgets << mBtnSwappiness;
    connect(mBtnSwappiness, &QPushButton::clicked, this, &HelpersPage::onSwappinessClicked);

    // FR-117: CPU tuning button.
    mBtnCpuTuning = new QPushButton(tr("CPU Tuning"));
    mBtnCpuTuning->setCheckable(true);
    mBtnCpuTuning->setCursor(Qt::PointingHandCursor);
    mBtnCpuTuning->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->buttonGroup->addButton(mBtnCpuTuning);
    shadowWidgets << mBtnCpuTuning;
    connect(mBtnCpuTuning, &QPushButton::clicked, this, &HelpersPage::onCpuTuningClicked);

    // FW-15: Battery charge threshold — gated on sysfs node existing.
    {
        ChargeThresholdStatus probe = BatteryChargeThreshold::readStatus();
        if (probe.available) {
            mBtnBatteryThreshold = new QPushButton(tr("Charge Threshold"));
            mBtnBatteryThreshold->setCheckable(true);
            mBtnBatteryThreshold->setCursor(Qt::PointingHandCursor);
            mBtnBatteryThreshold->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            ui->buttonGroup->addButton(mBtnBatteryThreshold);
            shadowWidgets << mBtnBatteryThreshold;
            connect(mBtnBatteryThreshold, &QPushButton::clicked,
                    this, &HelpersPage::onBatteryThresholdClicked);
        }
    }

    // FR-118: TRIM button (Linux — gated on fstrim being installed).
    if (CommandUtil::isExecutable("fstrim")) {
        mBtnTrim = new QPushButton(tr("SSD TRIM"));
        mBtnTrim->setCheckable(true);
        mBtnTrim->setCursor(Qt::PointingHandCursor);
        mBtnTrim->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        ui->buttonGroup->addButton(mBtnTrim);
        shadowWidgets << mBtnTrim;
        connect(mBtnTrim, &QPushButton::clicked, this, &HelpersPage::onTrimClicked);
    }

    initPowerProfileUI();
    if (mPowerProfileWidget)
        shadowWidgets << mPowerProfileWidget;
#endif

    // FR-120: Wake-on-LAN button — cross-platform.
    mBtnWol = new QPushButton(tr("Wake-on-LAN"));
    mBtnWol->setCheckable(true);
    mBtnWol->setCursor(Qt::PointingHandCursor);
    mBtnWol->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->buttonGroup->addButton(mBtnWol);
    shadowWidgets << mBtnWol;
    connect(mBtnWol, &QPushButton::clicked, this, &HelpersPage::onWolClicked);

    Utilities::addDropShadow(shadowWidgets, 40);

    // Collect tool nav items in display order (action buttons go to maintenance cards)
    mToolItems << ui->btnHostManage << ui->btnNetDiag
               << ui->btnOpenPorts << ui->btnFirewall;
#ifdef Q_OS_MACOS
    if (mBtnTrim)
        mToolItems << mBtnTrim;
#else
    if (mBtnSwappiness)
        mToolItems << mBtnSwappiness;
    if (mBtnCpuTuning)
        mToolItems << mBtnCpuTuning;
    if (mBtnBatteryThreshold)
        mToolItems << mBtnBatteryThreshold;
    if (mBtnTrim)
        mToolItems << mBtnTrim;
    if (mPowerProfileWidget)
        mToolItems << mPowerProfileWidget;
#endif
    if (mBtnWol)
        mToolItems << mBtnWol;

    // Set up ui->nav as a VBox containing tools container + maintenance section
    delete ui->nav->layout();

    auto *navVbox = new QVBoxLayout(ui->nav);
    navVbox->setSpacing(8);
    navVbox->setContentsMargins(0, 0, 0, 0);

    mToolsContainer = new QWidget;
    mToolsContainer->setObjectName("toolsContainer");
    navVbox->addWidget(mToolsContainer);

    buildMaintenanceSection();
    navVbox->addWidget(mMaintenanceSection);

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

void HelpersPage::onSwappinessClicked()
{
#ifdef Q_OS_LINUX
    if (!mSwappinessWidget)
        return;
    mSwappinessWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentWidget(mSwappinessWidget);
#endif
}

void HelpersPage::onCpuTuningClicked()
{
#ifdef Q_OS_LINUX
    if (!mCpuTuningWidget)
        return;
    mCpuTuningWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentWidget(mCpuTuningWidget);
#endif
}

void HelpersPage::onBatteryThresholdClicked()
{
#ifdef Q_OS_LINUX
    if (!mBatteryThresholdWidget)
        return;
    mBatteryThresholdWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentWidget(mBatteryThresholdWidget);
#endif
}

void HelpersPage::onTrimClicked()
{
    if (!mTrimWidget)
        return;
    mTrimWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentWidget(mTrimWidget);
}

void HelpersPage::onWolClicked()
{
    if (!mWolWidget)
        return;
    mWolWidget->loadIfNeeded();
    ui->stackedWidget->setCurrentWidget(mWolWidget);
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
    mNavMinWidth = qMax(0, mToolItems.count() - 1) * 12;
    for (auto *w : mToolItems)
        mNavMinWidth += w->sizeHint().width();
    mNavMinWidth += 20;
}

void HelpersPage::applyNavLayout(bool compact)
{
    delete mToolsContainer->layout();
    mNavCompact = compact;

    if (!compact) {
        auto *row = new QHBoxLayout(mToolsContainer);
        row->setSpacing(12);
        row->setContentsMargins(0, 0, 0, 0);
        ui->lblToolsSection->setParent(mToolsContainer);
        row->addWidget(ui->lblToolsSection, 0, Qt::AlignVCenter);
        row->addSpacing(4);
        for (auto *w : mToolItems) {
            w->setParent(mToolsContainer);
            row->addWidget(w, 0, Qt::AlignLeft);
        }
        row->addStretch();
    } else {
        auto *col = new QVBoxLayout(mToolsContainer);
        col->setSpacing(8);
        col->setContentsMargins(0, 0, 0, 0);

        int splitIndex = (mToolItems.count() + 1) / 2;
        splitIndex = qBound(1, splitIndex, mToolItems.count() - 1);

        auto *row1 = new QHBoxLayout();
        row1->setSpacing(12);
        ui->lblToolsSection->setParent(mToolsContainer);
        row1->addWidget(ui->lblToolsSection, 0, Qt::AlignVCenter);
        row1->addSpacing(4);
        for (int i = 0; i < splitIndex; ++i) {
            mToolItems[i]->setParent(mToolsContainer);
            row1->addWidget(mToolItems[i], 0, Qt::AlignLeft);
        }
        row1->addStretch();
        col->addLayout(row1);

        auto *row2 = new QHBoxLayout();
        row2->setSpacing(12);
        for (int i = splitIndex; i < mToolItems.count(); ++i) {
            mToolItems[i]->setParent(mToolsContainer);
            row2->addWidget(mToolItems[i], 0, Qt::AlignLeft);
        }
        row2->addStretch();
        col->addLayout(row2);
    }
}

void HelpersPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (mToolItems.isEmpty())
        return;
    const bool compact = width() < mNavMinWidth;
    if (compact != mNavCompact)
        applyNavLayout(compact);
}

void HelpersPage::buildMaintenanceSection()
{
    mMaintenanceSection = new QWidget;
    mMaintenanceSection->setObjectName("maintenanceSection");

    auto *outer = new QVBoxLayout(mMaintenanceSection);
    outer->setSpacing(6);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *lblHeader = new QLabel(tr("MAINTENANCE"), mMaintenanceSection);
    lblHeader->setObjectName("lblMaintenanceSectionHeader");
    outer->addWidget(lblHeader);

    auto *cardRow = new QHBoxLayout;
    cardRow->setSpacing(10);
    cardRow->setContentsMargins(0, 0, 0, 0);

    auto makeCard = [&](const QString &title, const QString &desc,
                        std::function<void()> action) -> QFrame * {
        auto *card = new QFrame(mMaintenanceSection);
        card->setObjectName("maintenanceCard");
        card->setCursor(Qt::PointingHandCursor);
        auto *vbox = new QVBoxLayout(card);
        vbox->setSpacing(2);
        vbox->setContentsMargins(10, 8, 10, 8);
        auto *lblTitle = new QLabel(title, card);
        lblTitle->setObjectName("lblCardTitle");
        auto *lblDesc = new QLabel(desc, card);
        lblDesc->setObjectName("lblCardDesc");
        lblDesc->setWordWrap(true);
        vbox->addWidget(lblTitle);
        vbox->addWidget(lblDesc);
        card->installEventFilter(new CardFilter(card, std::move(action)));
        return card;
    };

    cardRow->addWidget(makeCard(
        tr("Flush DNS Cache"),
        tr("Clear the system DNS resolver cache"),
        [this] { on_btnFlushDNS_clicked(); }
    ));

#ifdef Q_OS_MACOS
    cardRow->addWidget(makeCard(
        tr("Rebuild Spotlight"),
        tr("Delete and rebuild the Spotlight search index"),
        [this] { onRebuildSpotlight(); }
    ));
    cardRow->addWidget(makeCard(
        tr("Verify Disk"),
        tr("Verify the integrity of the startup disk"),
        [this] { onVerifyDisk(); }
    ));
    cardRow->addWidget(makeCard(
        tr("Rebuild Launch Services"),
        tr("Rescan app database and restart Finder"),
        [this] { onRebuildLaunchServices(); }
    ));
#endif

    cardRow->addStretch();
    outer->addLayout(cardRow);
}
