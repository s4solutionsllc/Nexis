#include "settings_page.h"
#include "ui_settings_page.h"
#include "Managers/info_manager.h"
#include "Managers/schedule_manager.h"
#include "Managers/cleaner_service.h"
#include "Managers/data_refresh_service.h"
#include "Services/snapshot_service.h"
#include "Pages/SystemCleaner/schedule_editor_dialog.h"
#include "utilities.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QLineEdit>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDialog>
#include <QScrollArea>
#include <Utils/format_util.h>
#include <functional>

SettingsPage::~SettingsPage()
{
    delete ui;
}

SettingsPage::SettingsPage(QWidget *parent, AppManager *appManager,
                           SettingManager *settingManager, InfoManager *infoManager,
                           ScheduleManager *scheduleManager) :
    QWidget(parent),
    ui(new Ui::SettingsPage),
    apm(appManager ? appManager : AppManager::ins()),
    mInfoManager(infoManager ? infoManager : InfoManager::ins()),
    mScheduleManager(scheduleManager ? scheduleManager : ScheduleManager::ins()),
    mSettingManager(settingManager ? settingManager : SettingManager::ins())
{
    ui->setupUi(this);

    // Transparent scroll area — use QPalette approach (not inline setStyleSheet)
    // so the global QSS cascade reaches child widgets cleanly
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContent->setAutoFillBackground(false);

    auto updateCreditLink = [this]() {
        QSettings *sv = apm->getStyleValues();
        QString accent = sv ? sv->value("@accentColor").toString() : "#FF6B1A";
        ui->lblCreatedBy->setText(
            QString("<html><head/><body><p>Nexis v%1 "
                    "<a href=\"https://github.com/s4solutionsllc\">"
                    "<span style=\" text-decoration: underline; color:%2;\">"
                    "S4 Solutions, LLC</span></a></p></body></html>")
                .arg(qApp->applicationVersion(), accent));
    };
    updateCreditLink();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, updateCreditLink);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &SettingsPage::refreshThemeColors);

    init();
}

void SettingsPage::init()
{
    // load languages
    QMapIterator<QString, QString> lang(apm->getLanguageList());

    while (lang.hasNext()) {
        lang.next();
        ui->cmbLanguages->addItem(lang.value(), lang.key());
    }

    QString lc = mSettingManager->getLanguage();
    ui->cmbLanguages->setCurrentText(apm->getLanguageList().value(lc));

    // load disks
    mInfoManager->updateDiskInfo();
    const QList<Disk> disks = mInfoManager->getDisks();

    for (const Disk &disk : disks) {
        ui->cmbDisks->addItem(QString("%1  (%2)").arg(disk.device).arg(disk.name), disk.name);
    }

    QString dk = mSettingManager->getDiskName().isEmpty() ? QStorageInfo::root().displayName() : mSettingManager->getDiskName();
    if (! dk.isEmpty()) {
        ui->cmbDisks->setCurrentIndex(ui->cmbDisks->findData(dk));
    }

    // start on boot — platform-specific path and check
#ifdef Q_OS_MACOS
    mStartupAppPath = QDir::homePath() + "/Library/LaunchAgents";
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/io.github.s4solutionsllc.Nexis.plist");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        ui->checkAutostart->setChecked(true);
    } else {
        ui->checkAutostart->setChecked(false);
    }
#else
    mStartupAppPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation).append("/autostart");
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/nexis.desktop");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        QStringList appContent = FileUtil::readListFromFile(mStartupAppPath);
        QString isHidden = Utilities::getDesktopValue(QRegularExpression("^Hidden=.*"), appContent).toLower();
        ui->checkAutostart->setChecked(isHidden == "false");
    } else {
        ui->checkAutostart->setChecked(false);
    }
#endif

    // app quit dont ask
    ui->checkAppQuitDontAsk->setChecked(mSettingManager->getAppQuitDialogDontAsk());

    // minimize to tray
    ui->checkMinimizeToTray->setChecked(mSettingManager->getMinimizeToTray());

    // start minimized to tray (SSO-354)
    ui->checkStartMinimizedToTray->setChecked(mSettingManager->getStartMinimizedToTray());

    // dashboard footer visibility
    ui->checkDashboardFooter->setChecked(mSettingManager->getDashboardFooterVisible());

    // FW-20 (SSO-3748): menu-bar monitor is macOS-only surface
#ifdef Q_OS_MAC
    ui->checkMenuBarMonitor->setChecked(mSettingManager->getMenuBarMonitorEnabled());
#else
    ui->checkMenuBarMonitor->hide();
#endif

    // load pages — store a stable untranslated id as item data so the
    // saved start page survives a UI language change (SSO-3388 / audit Q3).
    ui->cmbStartPage->addItem(tr("Dashboard"),      "dashboard");
    ui->cmbStartPage->addItem(tr("Startup Apps"),   "startupApps");
    ui->cmbStartPage->addItem(tr("System Cleaner"), "systemCleaner");
    ui->cmbStartPage->addItem(tr("Search"),         "search");
    ui->cmbStartPage->addItem(tr("Services"),       "services");
    ui->cmbStartPage->addItem(tr("Processes"),      "processes");
    ui->cmbStartPage->addItem(tr("Helpers"),        "helpers");
#ifdef Q_OS_MAC
    ui->cmbStartPage->addItem(tr("Applications"),   "uninstaller");
#else
    ui->cmbStartPage->addItem(tr("Uninstaller"),    "uninstaller");
#endif
    ui->cmbStartPage->addItem(tr("Resources"),      "resources");

    ui->cmbStartPage->setCurrentIndex(
        ui->cmbStartPage->findData(mSettingManager->getStartPage()));

    // color scheme (appearance)
    ui->cmbColorScheme->addItem(tr("Auto"), "auto");
    ui->cmbColorScheme->addItem(tr("Light"), "light");
    ui->cmbColorScheme->addItem(tr("Dark"), "dark");
    ui->cmbColorScheme->setCurrentIndex(
        ui->cmbColorScheme->findData(mSettingManager->getColorScheme()));

    // font family
    ui->cmbFont->addItem(tr("Inter (Recommended)"), "Inter");
    ui->cmbFont->addItem("Ubuntu", "Ubuntu");
    ui->cmbFont->addItem("JetBrains Mono", "JetBrains Mono");
    ui->cmbFont->addItem(tr("System Default"), "system-ui");
    ui->cmbFont->setCurrentIndex(
        ui->cmbFont->findData(mSettingManager->getAppFont()));

    // tray icon style
    ui->cmbTrayIconStyle->addItem(tr("Color (Default)"), "color");
    ui->cmbTrayIconStyle->addItem(tr("Symbolic"), "symbolic");
    ui->cmbTrayIconStyle->addItem(tr("Outline"), "outline");
    ui->cmbTrayIconStyle->addItem(tr("Accent"), "accent");
    ui->cmbTrayIconStyle->addItem(tr("System Theme"), "system");
    ui->cmbTrayIconStyle->setCurrentIndex(
        ui->cmbTrayIconStyle->findData(mSettingManager->getTrayIconStyle()));

    // load resource percents
    ui->spinCpuPercent->setValue(mSettingManager->getCpuAlertPercent());
    ui->spinMemoryPercent->setValue(mSettingManager->getMemoryAlertPercent());
    ui->spinDiskPercent->setValue(mSettingManager->getDiskAlertPercent());

    // battery health alert (hide row if no battery)
    ui->spinBatteryHealthPercent->setValue(mSettingManager->getBatteryAlertPercent());
    if (!mInfoManager->hasBattery()) {
        ui->lblBatteryHealthPercent->hide();
        ui->spinBatteryHealthPercent->hide();
    }

    // disk health alert (hide if no SMART data). FR-96: disk discovery is
    // now async — if drives haven't been discovered at SettingsPage
    // construction time, the checkbox is initially hidden but unhidden on
    // the first diskHealthUpdated signal that reports any drives.
    ui->checkDiskHealthAlert->setChecked(mSettingManager->getDiskHealthAlertEnabled());
    if (!mInfoManager->hasDiskHealth()) {
        ui->checkDiskHealthAlert->hide();
    }
    connect(DataRefreshService::ins(), &DataRefreshService::diskHealthUpdated,
            this, [this](const QList<DriveHealth> &drives) {
        ui->checkDiskHealthAlert->setVisible(!drives.isEmpty());
    });

    // system update alert (hide if no package managers detected)
    ui->checkUpdateAlert->setChecked(mSettingManager->getUpdateAlertEnabled());
    if (!mInfoManager->hasUpdateSources()) {
        ui->checkUpdateAlert->hide();
    }

    // disk analyzer preference
    initDiskAnalyzerCombo();

    // DS §2/§3 (NEX F1/F2): page header, section-card headers, elevated
    // card chrome + shadow. Shadow color is theme-dependent (@shadowColor),
    // so refreshThemeColors() re-applies it on theme change.
    buildPageHeader();
    buildSectionHeader(ui->headerGeneral, tr("General"));
    buildSectionHeader(ui->headerAppearance, tr("Appearance"));
    buildSectionHeader(ui->headerAlerts, tr("Alerts"));
    buildSectionHeader(ui->headerTools, tr("Tools"));
    buildSectionHeader(ui->headerScheduledCleaning, tr("Scheduled Cleaning"));
    buildSectionCards();
    refreshThemeColors();

    // signal connections
    connect(ui->cmbLanguages, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbLanguagesChanged);
    connect(ui->cmbDisks, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbDiskChanged);
    connect(ui->cmbStartPage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbStartPageChanged);
    connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbColorSchemeChanged);
    connect(ui->cmbFont, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbFontChanged);
    connect(ui->cmbTrayIconStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbTrayIconStyleChanged);
    connect(ui->cmbDiskAnalyzer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbDiskAnalyzerChanged);

    // scheduled cleaning
    initScheduledCleaning();
}

void SettingsPage::cmbLanguagesChanged(const int &index)
{
    QString langCode = ui->cmbLanguages->itemData(index).toString();

    // Ignore programmatic re-selection that doesn't actually change the language
    // (e.g. the combo being repopulated) so we don't prompt spuriously.
    if (langCode == mSettingManager->getLanguage())
        return;

    mSettingManager->setLanguage(langCode);

    // Translations are installed once at startup (see AppManager), so a new
    // language only takes effect after a relaunch. Tell the user and offer to
    // restart now rather than leaving the UI silently unchanged.
    const auto choice = QMessageBox::question(
        this, tr("Language Changed"),
        tr("The language change will take effect after Nexis is restarted.\n\n"
           "Restart now?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (choice == QMessageBox::Yes) {
        // Inside an AppImage the executable lives on a transient mount that
        // disappears on exit; relaunch the .AppImage itself via $APPIMAGE.
        const QString target =
            qEnvironmentVariable("APPIMAGE", qApp->applicationFilePath());
        QProcess::startDetached(target, qApp->arguments().mid(1));
        qApp->quit();
    }
}

void SettingsPage::cmbDiskChanged(const int &index)
{
    QString diskName = ui->cmbDisks->itemData(index).toString();
    mSettingManager->setDiskName(diskName);
}

void SettingsPage::on_checkAutostart_clicked(bool checked)
{
    if (checked) {
#ifdef Q_OS_MACOS
        QString appTemplate = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>io.github.s4solutionsllc.Nexis</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "        <string>nexis</string>\n"
            "        <string>--hide</string>\n"
            "    </array>\n"
            "    <key>RunAtLoad</key>\n"
            "    <true/>\n"
            "</dict>\n"
            "</plist>\n");
#else
        QString appTemplate = QString("[Desktop Entry]\n"
                                      "Name=Nexis\n"
                                      "Comment=Linux System Optimizer and Monitoring\n"
                                      "Exec=nexis --hide \n"
                                      "Type=Application\n"
                                      "Terminal=false\n"
                                      "Hidden=false\n");
#endif
        FileUtil::writeFile(mStartupAppPath, appTemplate);
    } else {
        QFile::remove(mStartupAppPath);
    }
}

void SettingsPage::cmbStartPageChanged(const int index)
{
    const QString id = ui->cmbStartPage->itemData(index).toString();
    mSettingManager->setStartPage(id);
}

void SettingsPage::on_spinCpuPercent_valueChanged(int value)
{
    mSettingManager->setCpuAlertPercent(value);
}

void SettingsPage::on_spinMemoryPercent_valueChanged(int value)
{
    mSettingManager->setMemoryAlertPercent(value);
}

void SettingsPage::on_spinDiskPercent_valueChanged(int value)
{
    mSettingManager->setDiskAlertPercent(value);
}

void SettingsPage::on_spinBatteryHealthPercent_valueChanged(int value)
{
    mSettingManager->setBatteryAlertPercent(value);
}

void SettingsPage::on_checkAppQuitDontAsk_clicked(bool checked)
{
    mSettingManager->setAppQuitDialogDontAsk(checked);
}

void SettingsPage::on_checkMinimizeToTray_clicked(bool checked)
{
    mSettingManager->setMinimizeToTray(checked);
}

void SettingsPage::on_checkStartMinimizedToTray_clicked(bool checked)
{
    mSettingManager->setStartMinimizedToTray(checked);
}

void SettingsPage::on_checkDashboardFooter_clicked(bool checked)
{
    mSettingManager->setDashboardFooterVisible(checked);
    emit SignalMapper::ins()->sigDashboardFooterChanged(checked);
}

void SettingsPage::on_checkMenuBarMonitor_clicked(bool checked)
{
    mSettingManager->setMenuBarMonitorEnabled(checked);
    emit SignalMapper::ins()->sigMenuBarMonitorToggled(checked);
}

void SettingsPage::cmbColorSchemeChanged(int index)
{
    QString scheme = ui->cmbColorScheme->itemData(index).toString();
    mSettingManager->setColorScheme(scheme);
    apm->updateStylesheet();
}

void SettingsPage::cmbFontChanged(int index)
{
    QString fontFamily = ui->cmbFont->itemData(index).toString();
    mSettingManager->setAppFont(fontFamily);
    apm->updateStylesheet();
}

void SettingsPage::cmbTrayIconStyleChanged(int index)
{
    QString style = ui->cmbTrayIconStyle->itemData(index).toString();
    mSettingManager->setTrayIconStyle(style);
    apm->updateTrayIcon();
}

void SettingsPage::initDiskAnalyzerCombo()
{
    ui->cmbDiskAnalyzer->addItem(tr("Auto (Detect)"), "auto");
#ifdef Q_OS_MACOS
    ui->cmbDiskAnalyzer->addItem(tr("GrandPerspective"), "grandperspective");
    ui->cmbDiskAnalyzer->addItem(tr("DaisyDisk"), "daisydisk");
    ui->cmbDiskAnalyzer->addItem(tr("OmniDiskSweeper"), "omnidisksweeper");
#else
    ui->cmbDiskAnalyzer->addItem(tr("Baobab (GNOME Disk Usage Analyzer)"), "baobab");
    ui->cmbDiskAnalyzer->addItem(tr("Filelight (KDE)"), "filelight");
    ui->cmbDiskAnalyzer->addItem(tr("QDirStat"), "qdirstat");
    ui->cmbDiskAnalyzer->addItem(tr("ncdu (Terminal)"), "ncdu");
#endif
    ui->cmbDiskAnalyzer->addItem(tr("Custom..."), "custom");

    QString saved = mSettingManager->getDiskAnalyzerTool();
    int idx = ui->cmbDiskAnalyzer->findData(saved);
    if (idx >= 0)
        ui->cmbDiskAnalyzer->setCurrentIndex(idx);
    else
        ui->cmbDiskAnalyzer->setCurrentIndex(0);

    ui->txtDiskAnalyzerCustomPath->setText(mSettingManager->getDiskAnalyzerCustomPath());
    updateCustomPathVisibility();
}

void SettingsPage::updateCustomPathVisibility()
{
    bool isCustom = (ui->cmbDiskAnalyzer->currentData().toString() == "custom");
    ui->lblDiskAnalyzerCustomPath->setVisible(isCustom);
    ui->txtDiskAnalyzerCustomPath->setVisible(isCustom);
}

void SettingsPage::cmbDiskAnalyzerChanged(int index)
{
    QString tool = ui->cmbDiskAnalyzer->itemData(index).toString();
    mSettingManager->setDiskAnalyzerTool(tool);
    updateCustomPathVisibility();
}

void SettingsPage::on_txtDiskAnalyzerCustomPath_editingFinished()
{
    mSettingManager->setDiskAnalyzerCustomPath(ui->txtDiskAnalyzerCustomPath->text().trimmed());
}

void SettingsPage::on_checkDiskHealthAlert_clicked(bool checked)
{
    mSettingManager->setDiskHealthAlertEnabled(checked);
}

void SettingsPage::on_checkUpdateAlert_clicked(bool checked)
{
    mSettingManager->setUpdateAlertEnabled(checked);
}

void SettingsPage::initScheduledCleaning()
{
    // Restore saved state
    ui->chkThresholdAlert->setChecked(mSettingManager->getThresholdAlertEnabled());
    ui->spnThresholdGB->setValue(mSettingManager->getThresholdGB());
    ui->spnThresholdGB->setEnabled(mSettingManager->getThresholdAlertEnabled());
    ui->chkCleaningNotifications->setChecked(mSettingManager->getCleaningNotificationsEnabled());

    // FR-112: hide the snapshot toggle when the platform tool (Timeshift /
    // tmutil) isn't available. Show a per-platform tool name in the label.
    SnapshotService *snap = SnapshotService::ins();
    if (snap->isAvailable()) {
        ui->chkPreCleanSnapshot->setText(
            tr("Create restore point before cleaning (%1)").arg(snap->toolDisplayName()));
        ui->chkPreCleanSnapshot->setChecked(mSettingManager->getPreCleanSnapshotEnabled());
    } else {
        ui->chkPreCleanSnapshot->hide();
    }

    // FR-113: Downloads auto-cleanup — checkbox gates the path/days sub-controls.
    const bool dlEnabled = mSettingManager->getDownloadsAutoCleanEnabled();
    ui->chkDownloadsAutoClean->setChecked(dlEnabled);
    ui->txtDownloadsPath->setText(mSettingManager->getDownloadsAutoCleanPath());
    ui->spnDownloadsDays->setValue(mSettingManager->getDownloadsAutoCleanDays());
    ui->txtDownloadsPath->setEnabled(dlEnabled);
    ui->btnDownloadsBrowse->setEnabled(dlEnabled);
    ui->spnDownloadsDays->setEnabled(dlEnabled);
    ui->lblDownloadsPath->setEnabled(dlEnabled);
    ui->lblDownloadsDays->setEnabled(dlEnabled);

    // Check if quick setup schedule exists
    bool hasQuickSetup = false;
    for (const auto &s : mScheduleManager->getAllSchedules()) {
        if (s.name == "Weekly Cleanup" && s.frequency == ScheduleManager::Weekly) {
            hasQuickSetup = true;
            break;
        }
    }
    ui->chkQuickSetup->setChecked(hasQuickSetup);
    updateScheduleSummary();

    // Connections
    connect(ui->chkQuickSetup, &QCheckBox::toggled, this, &SettingsPage::onQuickSetupToggled);
    connect(ui->chkThresholdAlert, &QCheckBox::toggled, this, &SettingsPage::onThresholdToggled);
    connect(ui->spnThresholdGB, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsPage::onThresholdGBChanged);
    connect(ui->btnManageSchedules, &QPushButton::clicked, this, &SettingsPage::onManageSchedules);
    connect(ui->btnViewHistory, &QPushButton::clicked, this, &SettingsPage::onViewCleaningHistory);
    connect(ui->chkCleaningNotifications, &QCheckBox::toggled, this, &SettingsPage::onCleaningNotificationsToggled);
    connect(ui->chkPreCleanSnapshot, &QCheckBox::toggled, this, &SettingsPage::onPreCleanSnapshotToggled);
    connect(ui->chkDownloadsAutoClean, &QCheckBox::toggled, this, &SettingsPage::onDownloadsAutoCleanToggled);
    connect(ui->btnDownloadsBrowse, &QPushButton::clicked, this, &SettingsPage::onDownloadsPathBrowse);
    connect(ui->spnDownloadsDays, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsPage::onDownloadsDaysChanged);
    connect(mScheduleManager, &ScheduleManager::schedulesChanged, this, &SettingsPage::updateScheduleSummary);
}

void SettingsPage::onQuickSetupToggled(bool checked)
{
    ScheduleManager *sm = mScheduleManager;

    if (checked) {
        ScheduleManager::CleaningSchedule s;
        s.name = "Weekly Cleanup";
        s.frequency = ScheduleManager::Weekly;
        s.dayOfWeek = 0;
        s.hour = 3;
        s.minute = 0;
        s.categories = {
            CleanerService::PACKAGE_CACHE,
            CleanerService::CRASH_REPORTS,
            CleanerService::APPLICATION_LOGS,
            CleanerService::APPLICATION_CACHES,
            CleanerService::DEV_TOOL_CACHES
        };
        s.minFileAgeSecs = 86400;
        sm->createSchedule(s);
    } else {
        for (const auto &s : sm->getAllSchedules()) {
            if (s.name == "Weekly Cleanup" && s.frequency == ScheduleManager::Weekly) {
                sm->deleteSchedule(s.id);
                break;
            }
        }
    }
}

void SettingsPage::onThresholdToggled(bool checked)
{
    mSettingManager->setThresholdAlertEnabled(checked);
    ui->spnThresholdGB->setEnabled(checked);
}

void SettingsPage::onThresholdGBChanged(int value)
{
    mSettingManager->setThresholdGB(value);
}

void SettingsPage::onPreCleanSnapshotToggled(bool checked)
{
    mSettingManager->setPreCleanSnapshotEnabled(checked);
}

void SettingsPage::onDownloadsAutoCleanToggled(bool checked)
{
    mSettingManager->setDownloadsAutoCleanEnabled(checked);
    ui->txtDownloadsPath->setEnabled(checked);
    ui->btnDownloadsBrowse->setEnabled(checked);
    ui->spnDownloadsDays->setEnabled(checked);
    ui->lblDownloadsPath->setEnabled(checked);
    ui->lblDownloadsDays->setEnabled(checked);
}

void SettingsPage::onDownloadsPathBrowse()
{
    const QString current = ui->txtDownloadsPath->text();
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Select Downloads folder"), current);
    if (chosen.isEmpty())
        return;
    ui->txtDownloadsPath->setText(chosen);
    mSettingManager->setDownloadsAutoCleanPath(chosen);
}

void SettingsPage::onDownloadsDaysChanged(int value)
{
    mSettingManager->setDownloadsAutoCleanDays(value);
}

void SettingsPage::onCleaningNotificationsToggled(bool checked)
{
    mSettingManager->setCleaningNotificationsEnabled(checked);
}

void SettingsPage::onManageSchedules()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Manage Cleaning Schedules"));
    dialog.setObjectName("manageSchedulesDialog");
    dialog.setMinimumSize(550, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *dlgTitle = new QLabel(tr("Manage Cleaning Schedules"));
    dlgTitle->setProperty("accessibleName", "dialog-title");
    layout->addWidget(dlgTitle);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
    QWidget *scrollWidget = new QWidget;
    scrollWidget->setStyleSheet("background-color:transparent;");
    QVBoxLayout *listLayout = new QVBoxLayout(scrollWidget);

    std::function<void()> refreshList = [&]() {
        QLayoutItem *item;
        while ((item = listLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        QList<ScheduleManager::CleaningSchedule> schedules = mScheduleManager->getAllSchedules();

        if (schedules.isEmpty()) {
            listLayout->addWidget(new QLabel(tr("No schedules configured.")));
        }

        for (const auto &s : schedules) {
            QGroupBox *card = new QGroupBox;
            QHBoxLayout *cardLayout = new QHBoxLayout(card);

            QCheckBox *enableCheck = new QCheckBox;
            enableCheck->setChecked(s.enabled);
            cardLayout->addWidget(enableCheck);

            QVBoxLayout *infoLayout = new QVBoxLayout;
            QLabel *nameLabel = new QLabel(QString("<b>%1</b>").arg(s.name));
            QLabel *freqLabel = new QLabel(ScheduleManager::frequencyDisplayText(s));
            freqLabel->setProperty("accessibleName", "dimmed");

            QString lastRunText;
            if (s.lastRun.isValid()) {
                lastRunText = tr("Last: %1 — %2")
                    .arg(s.lastRun.toString("MMM d, h:mm AP"))
                    .arg(FormatUtil::formatBytes(s.lastBytesFreed));
            } else {
                lastRunText = tr("Never run");
            }
            QLabel *lastLabel = new QLabel(lastRunText);
            lastLabel->setProperty("accessibleName", "dimmed-small");

            infoLayout->addWidget(nameLabel);
            infoLayout->addWidget(freqLabel);
            infoLayout->addWidget(lastLabel);
            cardLayout->addLayout(infoLayout, 1);

            QPushButton *editBtn = new QPushButton(tr("Edit"));
            QPushButton *deleteBtn = new QPushButton(tr("Delete"));
            deleteBtn->setProperty("accessibleName", "danger");
            cardLayout->addWidget(editBtn);
            cardLayout->addWidget(deleteBtn);

            QString schedId = s.id;

            connect(enableCheck, &QCheckBox::toggled, [this, schedId](bool checked) {
                ScheduleManager::CleaningSchedule updated = mScheduleManager->getSchedule(schedId);
                updated.enabled = checked;
                mScheduleManager->updateSchedule(updated);
            });

            connect(editBtn, &QPushButton::clicked, [this, schedId, &dialog, &refreshList]() {
                ScheduleManager::CleaningSchedule existing = mScheduleManager->getSchedule(schedId);
                ScheduleEditorDialog editor(existing, &dialog);
                connect(&editor, &ScheduleEditorDialog::scheduleUpdated, this, [this](const ScheduleManager::CleaningSchedule &s) {
                    mScheduleManager->updateSchedule(s);
                });
                editor.exec();
                refreshList();
            });

            connect(deleteBtn, &QPushButton::clicked, [this, schedId, &refreshList]() {
                mScheduleManager->deleteSchedule(schedId);
                refreshList();
            });

            listLayout->addWidget(card);
        }

        listLayout->addStretch();
    };

    refreshList();

    scrollArea->setWidget(scrollWidget);
    layout->addWidget(scrollArea);

    QPushButton *addBtn = new QPushButton(tr("Add Schedule"));
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setProperty("accessibleName", "primary");
    connect(addBtn, &QPushButton::clicked, [this, &dialog, &refreshList]() {
        ScheduleEditorDialog editor(&dialog);
        connect(&editor, &ScheduleEditorDialog::scheduleCreated, this, [this](const ScheduleManager::CleaningSchedule &s) {
            mScheduleManager->createSchedule(s);
        });
        editor.exec();
        refreshList();
    });

    QPushButton *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(addBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dialog.exec();
}

void SettingsPage::onViewCleaningHistory()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Cleaning History"));
    dialog.setObjectName("cleaningHistoryDialog");
    dialog.setMinimumSize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *dlgTitle = new QLabel(tr("Cleaning History"));
    dlgTitle->setProperty("accessibleName", "dialog-title");
    layout->addWidget(dlgTitle);

    QPlainTextEdit *textEdit = new QPlainTextEdit;
    textEdit->setReadOnly(true);

    QString logPath = mSettingManager->getConfigPath() + "/clean_history.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QStringList lines;
        QTextStream stream(&logFile);
        while (!stream.atEnd()) {
            lines.append(stream.readLine());
        }
        logFile.close();

        int start = qMax(0, lines.size() - 50);
        QStringList recent = lines.mid(start);
        textEdit->setPlainText(recent.join('\n'));
    } else {
        textEdit->setPlainText(tr("No cleaning history available."));
    }

    layout->addWidget(textEdit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *clearBtn = new QPushButton(tr("Clear History"));
    clearBtn->setProperty("accessibleName", "danger");
    connect(clearBtn, &QPushButton::clicked, [logPath, textEdit]() {
        QFile::remove(logPath);
        textEdit->setPlainText("");
    });
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dialog.exec();
}

void SettingsPage::updateScheduleSummary()
{
    QList<ScheduleManager::CleaningSchedule> schedules = mScheduleManager->getAllSchedules();

    if (schedules.isEmpty()) {
        ui->lblQuickSetupSummary->setText(tr("No schedules active"));
        return;
    }

    QDateTime earliest;
    QString nextName;
    for (const auto &s : schedules) {
        if (!s.enabled) continue;
        QDateTime next = mScheduleManager->getNextRunTime(s);
        if (!earliest.isValid() || next < earliest) {
            earliest = next;
            nextName = s.name;
        }
    }

    if (earliest.isValid()) {
        ui->lblQuickSetupSummary->setText(
            tr("Next: %1 — %2").arg(nextName, earliest.toString("ddd, MMM d h:mm AP")));
    } else {
        ui->lblQuickSetupSummary->setText(tr("%1 schedule(s) configured").arg(schedules.size()));
    }
}

void SettingsPage::refreshThemeColors()
{
    // DS §2 elevated-card shadow (alpha 90, blur 26) — one per section card,
    // never per control (DS §7/§9). Re-applied on theme change because the
    // shadow color resolves from @shadowColor at call time.
    Utilities::addDropShadow(ui->groupGeneral, 90, 26);
    Utilities::addDropShadow(ui->groupAppearance, 90, 26);
    Utilities::addDropShadow(ui->groupAlerts, 90, 26);
    Utilities::addDropShadow(ui->groupTools, 90, 26);
    Utilities::addDropShadow(ui->groupScheduledCleaning, 90, 26);
}

void SettingsPage::buildPageHeader()
{
    // DS §3 page-level header (NEX F2 shared recipe): non-compact accent
    // bar (>=26px) + title + muted source line, mirrors
    // MetricTileBase::buildChrome() / SystemCleanerPage::buildCategoryHeader().
    ui->pageHeader->setObjectName("sectionHeaderRow");

    ui->pageHeaderAccent->setObjectName("sectionHeaderAccent");
    ui->pageHeaderAccent->setProperty("accentToken", "accent");
    ui->pageHeaderAccent->setFixedWidth(3);
    ui->pageHeaderAccent->setMinimumHeight(26);
    ui->pageHeaderAccent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    ui->pageHeaderTitle->setObjectName("sectionHeaderTitle");
    ui->pageHeaderSource->setObjectName("sectionHeaderSource");
}

void SettingsPage::buildSectionHeader(QWidget *headerContainer, const QString &title)
{
    // DS §3 section-card header (NEX F2 shared recipe, "compact" variant —
    // >=18px accent bar instead of the >=26px page/tile-header bar) built
    // programmatically, mirroring SystemCleanerPage::buildCategoryHeader().
    headerContainer->setObjectName("sectionHeaderRow");

    QHBoxLayout *row = new QHBoxLayout(headerContainer);
    row->setContentsMargins(14, 12, 14, 8);
    row->setSpacing(8);

    QFrame *accentBar = new QFrame(headerContainer);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("compact", true);
    accentBar->setProperty("accentToken", "accent");
    accentBar->setFrameShape(QFrame::NoFrame);
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(18);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    row->addWidget(accentBar);

    QLabel *lblTitle = new QLabel(title, headerContainer);
    lblTitle->setObjectName("sectionHeaderTitle");
    row->addWidget(lblTitle);
    row->addStretch();
}

void SettingsPage::buildSectionCards()
{
    // DS §2 elevated card chrome (NEX F1 shared recipe) — background,
    // border, radius come from [cardRole="elevated"] in style.qss.
    const QList<QWidget *> cards = {
        ui->groupGeneral, ui->groupAppearance, ui->groupAlerts,
        ui->groupTools, ui->groupScheduledCleaning
    };
    for (QWidget *card : cards) {
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setProperty("cardRole", "elevated");
    }

    // Inner content padding now that the QGroupBox native title/frame is
    // gone — the header row above supplies its own top padding.
    ui->gridGeneral->setContentsMargins(14, 0, 14, 14);
    ui->gridAppearance->setContentsMargins(14, 0, 14, 14);
    ui->gridAlerts->setContentsMargins(14, 0, 14, 14);
    ui->gridTools->setContentsMargins(14, 0, 14, 14);
    ui->layoutScheduledCleaning->setContentsMargins(14, 0, 14, 14);
}
