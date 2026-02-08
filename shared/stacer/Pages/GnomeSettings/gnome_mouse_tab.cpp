#include "gnome_mouse_tab.h"
#include "ui_gnome_mouse_tab.h"

#include <Tools/gnome_settings_tool.h>

GnomeMouseTab::GnomeMouseTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeMouseTab),
    mLoading(false)
{
    ui->setupUi(this);

    // Make transparent so @pageContent background shows through in dark mode
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContents->setAutoFillBackground(false);

    // Hide groups for missing schemas
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::MOUSE))
        ui->groupMouse->hide();
    if (!GnomeSettingsTool::schemaExists(GnomeSchema::TOUCHPAD))
        ui->groupTouchpad->hide();

    loadSettings();

    // Mouse connections
    connect(ui->chkMouseNatural, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::MOUSE, GnomeKey::NATURAL_SCROLL, checked);
    });
    connect(ui->sliderMouseSpeed, &QSlider::valueChanged, this, [this](int val) {
        if (mLoading) return;
        double speed = val / 100.0;
        ui->lblMouseSpeedVal->setText(QString::number(speed, 'f', 2));
        GnomeSettingsTool::setD(GnomeSchema::MOUSE, GnomeKey::SPEED, speed);
    });
    connect(ui->cmbAccelProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::MOUSE, GnomeKey::ACCEL_PROFILE,
                                ui->cmbAccelProfile->currentData().toString());
    });
    connect(ui->chkLeftHanded, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::MOUSE, GnomeKey::LEFT_HANDED, checked);
    });

    // Touchpad connections
    connect(ui->chkTapToClick, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::TOUCHPAD, GnomeKey::TAP_TO_CLICK, checked);
    });
    connect(ui->chkTouchpadNatural, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::TOUCHPAD, GnomeKey::NATURAL_SCROLL, checked);
    });
    connect(ui->sliderTouchpadSpeed, &QSlider::valueChanged, this, [this](int val) {
        if (mLoading) return;
        double speed = val / 100.0;
        ui->lblTouchpadSpeedVal->setText(QString::number(speed, 'f', 2));
        GnomeSettingsTool::setD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED, speed);
    });
    connect(ui->chkTwoFingerScroll, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::TOUCHPAD, GnomeKey::TWO_FINGER_SCROLL, checked);
    });
    connect(ui->chkEdgeScrolling, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::TOUCHPAD, GnomeKey::EDGE_SCROLLING, checked);
    });
    connect(ui->chkDisableTyping, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::TOUCHPAD, GnomeKey::DISABLE_TYPING, checked);
    });
}

GnomeMouseTab::~GnomeMouseTab()
{
    delete ui;
}

void GnomeMouseTab::loadSettings()
{
    mLoading = true;

    // Mouse
    if (GnomeSettingsTool::schemaExists(GnomeSchema::MOUSE)) {
        ui->chkMouseNatural->setChecked(GnomeSettingsTool::getB(GnomeSchema::MOUSE, GnomeKey::NATURAL_SCROLL));

        double mouseSpeed = GnomeSettingsTool::getD(GnomeSchema::MOUSE, GnomeKey::SPEED);
        ui->sliderMouseSpeed->setValue(static_cast<int>(mouseSpeed * 100));
        ui->lblMouseSpeedVal->setText(QString::number(mouseSpeed, 'f', 2));

        ui->cmbAccelProfile->addItem(tr("Default"),  "default");
        ui->cmbAccelProfile->addItem(tr("Flat"),      "flat");
        ui->cmbAccelProfile->addItem(tr("Adaptive"), "adaptive");
        QString ap = GnomeSettingsTool::getS(GnomeSchema::MOUSE, GnomeKey::ACCEL_PROFILE);
        int apIdx = ui->cmbAccelProfile->findData(ap);
        if (apIdx >= 0) ui->cmbAccelProfile->setCurrentIndex(apIdx);

        ui->chkLeftHanded->setChecked(GnomeSettingsTool::getB(GnomeSchema::MOUSE, GnomeKey::LEFT_HANDED));
    }

    // Touchpad
    if (GnomeSettingsTool::schemaExists(GnomeSchema::TOUCHPAD)) {
        ui->chkTapToClick->setChecked(GnomeSettingsTool::getB(GnomeSchema::TOUCHPAD, GnomeKey::TAP_TO_CLICK));
        ui->chkTouchpadNatural->setChecked(GnomeSettingsTool::getB(GnomeSchema::TOUCHPAD, GnomeKey::NATURAL_SCROLL));

        double tpSpeed = GnomeSettingsTool::getD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED);
        ui->sliderTouchpadSpeed->setValue(static_cast<int>(tpSpeed * 100));
        ui->lblTouchpadSpeedVal->setText(QString::number(tpSpeed, 'f', 2));

        ui->chkTwoFingerScroll->setChecked(GnomeSettingsTool::getB(GnomeSchema::TOUCHPAD, GnomeKey::TWO_FINGER_SCROLL));
        ui->chkEdgeScrolling->setChecked(GnomeSettingsTool::getB(GnomeSchema::TOUCHPAD, GnomeKey::EDGE_SCROLLING));
        ui->chkDisableTyping->setChecked(GnomeSettingsTool::getB(GnomeSchema::TOUCHPAD, GnomeKey::DISABLE_TYPING));
    }

    mLoading = false;
}
