#include "gnome_settings_page.h"
#include "ui_gnome_settings_page.h"

#include <Tools/gnome_settings_tool.h>

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
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::WM_PREFS) &&
        !GnomeSettingsTool::schemaExists(GnomeSchema::MUTTER)) {
        ui->btnWindowManager->hide();
    }
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::MOUSE) &&
        !GnomeSettingsTool::schemaExists(GnomeSchema::TOUCHPAD)) {
        ui->btnMouse->hide();
    }
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::BACKGROUND) &&
        !GnomeSettingsTool::schemaExists(GnomeSchema::SOUND)) {
        ui->btnDesktop->hide();
    }

    for (int i = 0; i < tabButtons.size(); ++i) {
        connect(tabButtons[i], &QPushButton::clicked, this, [this, i]() {
            onTabButtonClicked(i);
        });
    }

    // Start on Appearance
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnAppearance->setChecked(true);
}

void GnomeSettingsPage::onTabButtonClicked(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
}
