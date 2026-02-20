#include "settings_page.h"
#include "ui_settings_page.h"
#include "Managers/info_manager.h"
#include "Managers/schedule_manager.h"
#include "Managers/cleaner_service.h"
#include "Pages/SystemCleaner/schedule_editor_dialog.h"
#include "utilities.h"
#include <QApplication>
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

    // Set version label dynamically from cmake-derived APP_VERSION
    ui->lblCreatedBy->setText(
        QString("<html><head/><body><p>Nexis v%1 "
                "<a href=\"https://github.com/lsimpsonsfdc\">"
                "<span style=\" text-decoration: underline; color:#E95420;\">"
                "Luke Simpson</span></a></p></body></html>")
            .arg(qApp->applicationVersion()));

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
    // macOS: LaunchAgent plist
    mStartupAppPath = QDir::homePath() + "/Library/LaunchAgents";
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/com.nexis.app.plist");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        ui->checkAutostart->setChecked(true);
    } else {
        ui->checkAutostart->setChecked(false);
    }
#else
    // Linux: XDG autostart .desktop entry
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

    // load pages
    ui->cmbStartPage->addItems({
        tr("Dashboard"), tr("Startup Apps"), tr("System Cleaner"), tr("Search"),
        tr("Services"), tr("Processes"), tr("Helpers"), tr("Uninstaller"), tr("Resources")
    });

    ui->cmbStartPage->setCurrentText(mSettingManager->getStartPage());

    // color scheme (appearance)
    ui->cmbColorScheme->addItem(tr("Auto"), "auto");
    ui->cmbColorScheme->addItem(tr("Light"), "light");
    ui->cmbColorScheme->addItem(tr("Dark"), "dark");
    ui->cmbColorScheme->setCurrentIndex(
        ui->cmbColorScheme->findData(mSettingManager->getColorScheme()));

    // load resource percents
    ui->spinCpuPercent->setValue(mSettingManager->getCpuAlertPercent());
    ui->spinMemoryPercent->setValue(mSettingManager->getMemoryAlertPercent());
    ui->spinDiskPercent->setValue(mSettingManager->getDiskAlertPercent());

    // battery health alert (hide on desktops with no battery)
    ui->spinBatteryHealthPercent->setValue(mSettingManager->getBatteryAlertPercent());
    if (!mInfoManager->hasBattery()) {
        ui->lblBatteryHealthPercent->hide();
        ui->spinBatteryHealthPercent->hide();
    }

    // disk health alert
    ui->checkDiskHealthAlert->setChecked(mSettingManager->getDiskHealthAlertEnabled());
    if (!mInfoManager->hasDiskHealth()) {
        ui->lblDiskHealthAlert->hide();
        ui->checkDiskHealthAlert->hide();
    }

    // disk analyzer preference
    initDiskAnalyzerCombo();

    // effects
    QList<QWidget*> widgets = {
        ui->cmbLanguages, ui->cmbDisks, ui->cmbStartPage, ui->cmbColorScheme,
        ui->spinCpuPercent, ui->spinMemoryPercent, ui->spinDiskPercent, ui->cmbDiskAnalyzer
    };

    Utilities::addDropShadow(widgets, 50);

    // connects (type-safe pointer-to-member syntax)
    connect(ui->cmbLanguages, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbLanguagesChanged);
    connect(ui->cmbDisks, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbDiskChanged);
    connect(ui->cmbStartPage, &QComboBox::currentTextChanged, this, &SettingsPage::cmbStartPageChanged);
    connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbColorSchemeChanged);
    connect(ui->cmbDiskAnalyzer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbDiskAnalyzerChanged);

    // scheduled cleaning
    initScheduledCleaning();
}

void SettingsPage::cmbLanguagesChanged(const int &index)
{
    QString langCode = ui->cmbLanguages->itemData(index).toString();

    mSettingManager->setLanguage(langCode);
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
        // macOS: write LaunchAgent plist
        QString appTemplate = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>com.nexis.app</string>\n"
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
        // Linux: write XDG autostart .desktop entry
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

void SettingsPage::cmbStartPageChanged(const QString text)
{
    mSettingManager->setStartPage(text);
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

void SettingsPage::cmbColorSchemeChanged(int index)
{
    QString scheme = ui->cmbColorScheme->itemData(index).toString();
    mSettingManager->setColorScheme(scheme);
    apm->updateStylesheet();
}

void SettingsPage::initDiskAnalyzerCombo()
{
    // Platform-specific disk analyzer tool list
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

    // Restore saved preference
    QString saved = mSettingManager->getDiskAnalyzerTool();
    int idx = ui->cmbDiskAnalyzer->findData(saved);
    if (idx >= 0)
        ui->cmbDiskAnalyzer->setCurrentIndex(idx);
    else
        ui->cmbDiskAnalyzer->setCurrentIndex(0); // fallback to Auto

    // Restore custom path
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

void SettingsPage::initScheduledCleaning()
{
    QGridLayout *grid = qobject_cast<QGridLayout *>(layout());
    if (!grid) return;

    // Remove the footer (row 10) and spacer (row 9) so we can insert
    // the Scheduled Cleaning section above them, then re-add them below.
    QLayoutItem *spacerItem = grid->itemAtPosition(9, 0);
    if (spacerItem) grid->removeItem(spacerItem);
    grid->removeWidget(ui->lblCreatedBy);

    // Section title
    QLabel *lblTitle = new QLabel(tr("Scheduled Cleaning"));
    lblTitle->setProperty("accessibleName", "title");
    grid->addWidget(lblTitle, 9, 0, 1, 6);

    // Quick setup
    mChkQuickSetup = new QCheckBox(tr("Enable automatic weekly cleaning"));
    mChkQuickSetup->setCursor(Qt::PointingHandCursor);
    grid->addWidget(mChkQuickSetup, 10, 0, 1, 2);

    mLblQuickSetupSummary = new QLabel;
    mLblQuickSetupSummary->setObjectName("lblQuickSetupSummary");
    grid->addWidget(mLblQuickSetupSummary, 10, 2, 1, 2);

    // Manage Schedules + View History buttons
    mBtnManageSchedules = new QPushButton(tr("Manage Schedules..."));
    mBtnManageSchedules->setCursor(Qt::PointingHandCursor);
    mBtnManageSchedules->setFocusPolicy(Qt::NoFocus);
    mBtnManageSchedules->setProperty("accessibleName", "primary");
    grid->addWidget(mBtnManageSchedules, 11, 0);

    mBtnViewHistory = new QPushButton(tr("View Cleaning History"));
    mBtnViewHistory->setCursor(Qt::PointingHandCursor);
    mBtnViewHistory->setFocusPolicy(Qt::NoFocus);
    mBtnViewHistory->setProperty("accessibleName", "primary");
    grid->addWidget(mBtnViewHistory, 11, 1);

    // Threshold alert
    mChkThresholdAlert = new QCheckBox(tr("Notify when junk exceeds"));
    mChkThresholdAlert->setCursor(Qt::PointingHandCursor);
    grid->addWidget(mChkThresholdAlert, 12, 0, 1, 2);

    mSpnThresholdGB = new QSpinBox;
    mSpnThresholdGB->setRange(1, 100);
    mSpnThresholdGB->setSuffix(tr(" GB"));
    mSpnThresholdGB->setFocusPolicy(Qt::ClickFocus);
    grid->addWidget(mSpnThresholdGB, 12, 2);

    // Cleaning notifications
    mChkCleaningNotifications = new QCheckBox(tr("Show notification after scheduled clean"));
    mChkCleaningNotifications->setCursor(Qt::PointingHandCursor);
    grid->addWidget(mChkCleaningNotifications, 13, 0, 1, 3);

    // Re-add spacer and footer below the new section
    if (spacerItem) grid->addItem(spacerItem, 14, 0);
    grid->addWidget(ui->lblCreatedBy, 15, 3, 1, 2, Qt::AlignRight);

    // Drop shadows matching existing Settings widgets
    Utilities::addDropShadow({mBtnManageSchedules, mBtnViewHistory, mSpnThresholdGB}, 50);

    // Restore saved state
    mChkThresholdAlert->setChecked(mSettingManager->getThresholdAlertEnabled());
    mSpnThresholdGB->setValue(mSettingManager->getThresholdGB());
    mSpnThresholdGB->setEnabled(mSettingManager->getThresholdAlertEnabled());
    mChkCleaningNotifications->setChecked(mSettingManager->getCleaningNotificationsEnabled());

    // Check if quick setup schedule exists
    bool hasQuickSetup = false;
    for (const auto &s : mScheduleManager->getAllSchedules()) {
        if (s.name == "Weekly Cleanup" && s.frequency == ScheduleManager::Weekly) {
            hasQuickSetup = true;
            break;
        }
    }
    mChkQuickSetup->setChecked(hasQuickSetup);
    updateScheduleSummary();

    // Connections
    connect(mChkQuickSetup, &QCheckBox::toggled, this, &SettingsPage::onQuickSetupToggled);
    connect(mChkThresholdAlert, &QCheckBox::toggled, this, &SettingsPage::onThresholdToggled);
    connect(mSpnThresholdGB, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsPage::onThresholdGBChanged);
    connect(mBtnManageSchedules, &QPushButton::clicked, this, &SettingsPage::onManageSchedules);
    connect(mBtnViewHistory, &QPushButton::clicked, this, &SettingsPage::onViewCleaningHistory);
    connect(mChkCleaningNotifications, &QCheckBox::toggled, this, &SettingsPage::onCleaningNotificationsToggled);
    connect(mScheduleManager, &ScheduleManager::schedulesChanged, this, &SettingsPage::updateScheduleSummary);
}

void SettingsPage::onQuickSetupToggled(bool checked)
{
    ScheduleManager *sm = mScheduleManager;

    if (checked) {
        ScheduleManager::CleaningSchedule s;
        s.name = "Weekly Cleanup";
        s.frequency = ScheduleManager::Weekly;
        s.dayOfWeek = 0; // Sunday
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
    mSpnThresholdGB->setEnabled(checked);
}

void SettingsPage::onThresholdGBChanged(int value)
{
    mSettingManager->setThresholdGB(value);
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
            editBtn->setFocusPolicy(Qt::NoFocus);
            QPushButton *deleteBtn = new QPushButton(tr("Delete"));
            deleteBtn->setFocusPolicy(Qt::NoFocus);
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

        // Show last 50 lines
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
        mLblQuickSetupSummary->setText(tr("No schedules active"));
        return;
    }

    // Find next upcoming schedule
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
        mLblQuickSetupSummary->setText(
            tr("Next: %1 — %2").arg(nextName, earliest.toString("ddd, MMM d h:mm AP")));
    } else {
        mLblQuickSetupSummary->setText(tr("%1 schedule(s) configured").arg(schedules.size()));
    }
}
