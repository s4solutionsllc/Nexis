#include "gnome_settings_page.h"
#include "Managers/tool_manager.h"
#include "ui_gnome_settings_page.h"

#include <QTimer>

GnomeSettingsPage::GnomeSettingsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeSettingsPage)
{
    ui->setupUi(this);
    init();
}

GnomeSettingsPage::~GnomeSettingsPage()
{
    delete ui;
}

void GnomeSettingsPage::init()
{
    // Create sub-tabs
    mAppearanceTab = new GnomeAppearanceTab(this);
    mWmTab = new GnomeWmTab(this);
    mMouseTab = new GnomeMouseTab(this);
    mDesktopTab = new GnomeDesktopTab(this);

    ui->stackedWidget->addWidget(mAppearanceTab);
    ui->stackedWidget->addWidget(mWmTab);
    ui->stackedWidget->addWidget(mMouseTab);
    ui->stackedWidget->addWidget(mDesktopTab);

    QList<QPushButton*> tabButtons = {
        ui->btnAppearance, ui->btnWindowManager, ui->btnMouse, ui->btnDesktop
    };

    // Hide tabs whose schemas are missing
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::WM_PREFS) &&
        !ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::MUTTER)) {
        ui->btnWindowManager->hide();
    }
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::MOUSE) &&
        !ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::TOUCHPAD)) {
        ui->btnMouse->hide();
    }
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::BACKGROUND) &&
        !ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::SOUND)) {
        ui->btnDesktop->hide();
    }

    for (int i = 0; i < tabButtons.size(); ++i) {
        connect(tabButtons[i], &QPushButton::clicked, this, [this, i]() {
            onTabButtonClicked(i);
        });
    }

    connect(mAppearanceTab, &GnomeAppearanceTab::settingFailed, this, &GnomeSettingsPage::showError);
    connect(mWmTab, &GnomeWmTab::settingFailed, this, &GnomeSettingsPage::showError);
    connect(mMouseTab, &GnomeMouseTab::settingFailed, this, &GnomeSettingsPage::showError);
    connect(mDesktopTab, &GnomeDesktopTab::settingFailed, this, &GnomeSettingsPage::showError);

    // Start on Appearance
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnAppearance->setChecked(true);
}

void GnomeSettingsPage::onTabButtonClicked(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
}

void GnomeSettingsPage::showError(const QString &message)
{
    ui->lblStatus->setText(message);
    ui->lblStatus->setVisible(true);
    QTimer::singleShot(4000, this, [this]() {
        ui->lblStatus->setVisible(false);
        ui->lblStatus->clear();
    });
}
