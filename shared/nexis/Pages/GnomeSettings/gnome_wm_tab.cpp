#include "gnome_wm_tab.h"
#include "ui_gnome_wm_tab.h"

#include <QSignalBlocker>
#include <Tools/gnome_settings_tool.h>

GnomeWmTab::GnomeWmTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeWmTab),
    mLoading(false)
{
    ui->setupUi(this);

    // Make transparent so @pageContent background shows through in dark mode
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContents->setAutoFillBackground(false);

    // Hide groups for missing schemas
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::WM_PREFS))
        ui->groupWmPrefs->hide();
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::MUTTER))
        ui->groupMutter->hide();

    loadSettings();

    // WM Preferences connections
    connect(ui->editButtonLayout, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::BUTTON_LAYOUT);
        if (!GnomeSettingsTool::setS(GnomeSchema::WM_PREFS, GnomeKey::BUTTON_LAYOUT, ui->editButtonLayout->text())) {
            const QSignalBlocker blocker(ui->editButtonLayout);
            ui->editButtonLayout->setText(prev);
            emit settingFailed(tr("Failed to apply Button Layout"));
        }
    });
    connect(ui->cmbFocusMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::FOCUS_MODE);
        if (!GnomeSettingsTool::setS(GnomeSchema::WM_PREFS, GnomeKey::FOCUS_MODE,
                                     ui->cmbFocusMode->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbFocusMode);
            int idx = ui->cmbFocusMode->findData(prevVal);
            if (idx >= 0) ui->cmbFocusMode->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Focus Mode"));
        }
    });
    connect(ui->editTitlebarFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::TITLEBAR_FONT);
        if (!GnomeSettingsTool::setS(GnomeSchema::WM_PREFS, GnomeKey::TITLEBAR_FONT, ui->editTitlebarFont->text())) {
            const QSignalBlocker blocker(ui->editTitlebarFont);
            ui->editTitlebarFont->setText(prev);
            emit settingFailed(tr("Failed to apply Titlebar Font"));
        }
    });
    connect(ui->spinWorkspaces, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (mLoading) return;
        int prev = GnomeSettingsTool::getI(GnomeSchema::WM_PREFS, GnomeKey::NUM_WORKSPACES);
        if (!GnomeSettingsTool::setI(GnomeSchema::WM_PREFS, GnomeKey::NUM_WORKSPACES, val)) {
            const QSignalBlocker blocker(ui->spinWorkspaces);
            ui->spinWorkspaces->setValue(prev);
            emit settingFailed(tr("Failed to apply Workspaces"));
        }
    });

    // Titlebar actions
    connect(ui->cmbDblClick, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_DBL_CLICK);
        if (!GnomeSettingsTool::setS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_DBL_CLICK,
                                     ui->cmbDblClick->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbDblClick);
            int idx = ui->cmbDblClick->findData(prevVal);
            if (idx >= 0) ui->cmbDblClick->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Double Click Action"));
        }
    });
    connect(ui->cmbMidClick, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_MID_CLICK);
        if (!GnomeSettingsTool::setS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_MID_CLICK,
                                     ui->cmbMidClick->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbMidClick);
            int idx = ui->cmbMidClick->findData(prevVal);
            if (idx >= 0) ui->cmbMidClick->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Middle Click Action"));
        }
    });
    connect(ui->cmbRightClick, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_RIGHT_CLICK);
        if (!GnomeSettingsTool::setS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_RIGHT_CLICK,
                                     ui->cmbRightClick->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbRightClick);
            int idx = ui->cmbRightClick->findData(prevVal);
            if (idx >= 0) ui->cmbRightClick->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Right Click Action"));
        }
    });

    // WM checkboxes
    connect(ui->chkAutoRaise, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::WM_PREFS, GnomeKey::AUTO_RAISE, checked)) {
            const QSignalBlocker blocker(ui->chkAutoRaise);
            ui->chkAutoRaise->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Auto Raise"));
        }
    });
    connect(ui->chkRaiseOnClick, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::WM_PREFS, GnomeKey::RAISE_ON_CLICK, checked)) {
            const QSignalBlocker blocker(ui->chkRaiseOnClick);
            ui->chkRaiseOnClick->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Raise on Click"));
        }
    });

    // Mutter checkboxes
    connect(ui->chkDynamicWorkspaces, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::MUTTER, GnomeKey::DYNAMIC_WORKSPACES, checked)) {
            const QSignalBlocker blocker(ui->chkDynamicWorkspaces);
            ui->chkDynamicWorkspaces->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Dynamic Workspaces"));
        }
    });
    connect(ui->chkEdgeTiling, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::MUTTER, GnomeKey::EDGE_TILING, checked)) {
            const QSignalBlocker blocker(ui->chkEdgeTiling);
            ui->chkEdgeTiling->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Edge Tiling"));
        }
    });
    connect(ui->chkAutoMaximize, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::MUTTER, GnomeKey::AUTO_MAXIMIZE, checked)) {
            const QSignalBlocker blocker(ui->chkAutoMaximize);
            ui->chkAutoMaximize->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Auto Maximize"));
        }
    });
    connect(ui->chkCenterNewWindows, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::MUTTER, GnomeKey::CENTER_NEW_WINDOWS, checked)) {
            const QSignalBlocker blocker(ui->chkCenterNewWindows);
            ui->chkCenterNewWindows->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Center New Windows"));
        }
    });
    connect(ui->chkWorkspacesPrimary, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::MUTTER, GnomeKey::WORKSPACES_PRIMARY, checked)) {
            const QSignalBlocker blocker(ui->chkWorkspacesPrimary);
            ui->chkWorkspacesPrimary->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Workspaces on Primary"));
        }
    });
}

GnomeWmTab::~GnomeWmTab()
{
    delete ui;
}

void GnomeWmTab::loadSettings()
{
    mLoading = true;

    auto addTitlebarActions = [](QComboBox *combo) {
        combo->addItem("Toggle Maximize", "toggle-maximize");
        combo->addItem("Minimize",        "minimize");
        combo->addItem("Lower",           "lower");
        combo->addItem("Menu",            "menu");
        combo->addItem("None",            "none");
    };

    // WM Preferences
    if (GnomeSettingsTool::schemaExists(GnomeSchema::WM_PREFS)) {
        ui->editButtonLayout->setText(GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::BUTTON_LAYOUT));

        ui->cmbFocusMode->addItem(tr("Click"),  "click");
        ui->cmbFocusMode->addItem(tr("Sloppy"), "sloppy");
        ui->cmbFocusMode->addItem(tr("Mouse"),  "mouse");
        QString fm = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::FOCUS_MODE);
        int fmIdx = ui->cmbFocusMode->findData(fm);
        if (fmIdx >= 0) ui->cmbFocusMode->setCurrentIndex(fmIdx);

        ui->editTitlebarFont->setText(GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::TITLEBAR_FONT));
        ui->spinWorkspaces->setValue(GnomeSettingsTool::getI(GnomeSchema::WM_PREFS, GnomeKey::NUM_WORKSPACES));

        addTitlebarActions(ui->cmbDblClick);
        addTitlebarActions(ui->cmbMidClick);
        addTitlebarActions(ui->cmbRightClick);

        QString dbl = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_DBL_CLICK);
        int dblIdx = ui->cmbDblClick->findData(dbl);
        if (dblIdx >= 0) ui->cmbDblClick->setCurrentIndex(dblIdx);

        QString mid = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_MID_CLICK);
        int midIdx = ui->cmbMidClick->findData(mid);
        if (midIdx >= 0) ui->cmbMidClick->setCurrentIndex(midIdx);

        QString right = GnomeSettingsTool::getS(GnomeSchema::WM_PREFS, GnomeKey::ACTION_RIGHT_CLICK);
        int rightIdx = ui->cmbRightClick->findData(right);
        if (rightIdx >= 0) ui->cmbRightClick->setCurrentIndex(rightIdx);

        ui->chkAutoRaise->setChecked(GnomeSettingsTool::getB(GnomeSchema::WM_PREFS, GnomeKey::AUTO_RAISE));
        ui->chkRaiseOnClick->setChecked(GnomeSettingsTool::getB(GnomeSchema::WM_PREFS, GnomeKey::RAISE_ON_CLICK));
    }

    // Mutter
    if (GnomeSettingsTool::schemaExists(GnomeSchema::MUTTER)) {
        ui->chkDynamicWorkspaces->setChecked(GnomeSettingsTool::getB(GnomeSchema::MUTTER, GnomeKey::DYNAMIC_WORKSPACES));
        ui->chkEdgeTiling->setChecked(GnomeSettingsTool::getB(GnomeSchema::MUTTER, GnomeKey::EDGE_TILING));
        ui->chkAutoMaximize->setChecked(GnomeSettingsTool::getB(GnomeSchema::MUTTER, GnomeKey::AUTO_MAXIMIZE));
        ui->chkCenterNewWindows->setChecked(GnomeSettingsTool::getB(GnomeSchema::MUTTER, GnomeKey::CENTER_NEW_WINDOWS));
        ui->chkWorkspacesPrimary->setChecked(GnomeSettingsTool::getB(GnomeSchema::MUTTER, GnomeKey::WORKSPACES_PRIMARY));
    }

    mLoading = false;
}
