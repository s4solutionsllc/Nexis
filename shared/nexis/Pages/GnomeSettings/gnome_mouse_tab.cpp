#include "gnome_mouse_tab.h"
#include "Managers/tool_manager.h"
#include "ui_gnome_mouse_tab.h"

#include <QSignalBlocker>
#include <QTimer>

GnomeMouseTab::GnomeMouseTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeMouseTab),
    mLoading(false)
{
    ui->setupUi(this);

    mMouseSpeedTimer = new QTimer(this);
    mMouseSpeedTimer->setSingleShot(true);
    mMouseSpeedTimer->setInterval(200);

    mTouchpadSpeedTimer = new QTimer(this);
    mTouchpadSpeedTimer->setSingleShot(true);
    mTouchpadSpeedTimer->setInterval(200);

    // Make transparent so @pageContent background shows through in dark mode
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContents->setAutoFillBackground(false);

    // Hide groups for missing schemas
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::MOUSE))
        ui->groupMouse->hide();
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::TOUCHPAD))
        ui->groupTouchpad->hide();

    loadSettings();

    // Mouse connections
    connect(ui->chkMouseNatural, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::MOUSE, GnomeKey::NATURAL_SCROLL, checked)) {
            const QSignalBlocker blocker(ui->chkMouseNatural);
            ui->chkMouseNatural->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Natural Scrolling"));
        }
    });
    connect(ui->sliderMouseSpeed, &QSlider::valueChanged, this, [this](int val) {
        if (mLoading) return;
        ui->lblMouseSpeedVal->setText(QString::number(val / 100.0, 'f', 2));
        mMouseSpeedTimer->start();
    });
    connect(mMouseSpeedTimer, &QTimer::timeout, this, [this]() {
        double speed = ui->sliderMouseSpeed->value() / 100.0;
        double prevSpeed = ToolManager::ins()->gnomeSettings()->getD(GnomeSchema::MOUSE, GnomeKey::SPEED);
        if (!ToolManager::ins()->gnomeSettings()->setD(GnomeSchema::MOUSE, GnomeKey::SPEED, speed)) {
            const QSignalBlocker blocker(ui->sliderMouseSpeed);
            ui->sliderMouseSpeed->setValue(static_cast<int>(prevSpeed * 100));
            ui->lblMouseSpeedVal->setText(QString::number(prevSpeed, 'f', 2));
            emit settingFailed(tr("Failed to apply Mouse Speed"));
        }
    });
    connect(ui->cmbAccelProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::MOUSE, GnomeKey::ACCEL_PROFILE);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::MOUSE, GnomeKey::ACCEL_PROFILE,
                                     ui->cmbAccelProfile->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbAccelProfile);
            int idx = ui->cmbAccelProfile->findData(prevVal);
            if (idx >= 0) ui->cmbAccelProfile->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Acceleration Profile"));
        }
    });
    connect(ui->chkLeftHanded, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::MOUSE, GnomeKey::LEFT_HANDED, checked)) {
            const QSignalBlocker blocker(ui->chkLeftHanded);
            ui->chkLeftHanded->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Left Handed"));
        }
    });

    // Touchpad connections
    connect(ui->chkTapToClick, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::TOUCHPAD, GnomeKey::TAP_TO_CLICK, checked)) {
            const QSignalBlocker blocker(ui->chkTapToClick);
            ui->chkTapToClick->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Tap to Click"));
        }
    });
    connect(ui->chkTouchpadNatural, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::TOUCHPAD, GnomeKey::NATURAL_SCROLL, checked)) {
            const QSignalBlocker blocker(ui->chkTouchpadNatural);
            ui->chkTouchpadNatural->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Touchpad Natural Scrolling"));
        }
    });
    connect(ui->sliderTouchpadSpeed, &QSlider::valueChanged, this, [this](int val) {
        if (mLoading) return;
        ui->lblTouchpadSpeedVal->setText(QString::number(val / 100.0, 'f', 2));
        mTouchpadSpeedTimer->start();
    });
    connect(mTouchpadSpeedTimer, &QTimer::timeout, this, [this]() {
        double speed = ui->sliderTouchpadSpeed->value() / 100.0;
        double prevSpeed = ToolManager::ins()->gnomeSettings()->getD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED);
        if (!ToolManager::ins()->gnomeSettings()->setD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED, speed)) {
            const QSignalBlocker blocker(ui->sliderTouchpadSpeed);
            ui->sliderTouchpadSpeed->setValue(static_cast<int>(prevSpeed * 100));
            ui->lblTouchpadSpeedVal->setText(QString::number(prevSpeed, 'f', 2));
            emit settingFailed(tr("Failed to apply Touchpad Speed"));
        }
    });
    connect(ui->chkTwoFingerScroll, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::TOUCHPAD, GnomeKey::TWO_FINGER_SCROLL, checked)) {
            const QSignalBlocker blocker(ui->chkTwoFingerScroll);
            ui->chkTwoFingerScroll->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Two Finger Scroll"));
        }
    });
    connect(ui->chkEdgeScrolling, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::TOUCHPAD, GnomeKey::EDGE_SCROLLING, checked)) {
            const QSignalBlocker blocker(ui->chkEdgeScrolling);
            ui->chkEdgeScrolling->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Edge Scrolling"));
        }
    });
    connect(ui->chkDisableTyping, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::TOUCHPAD, GnomeKey::DISABLE_TYPING, checked)) {
            const QSignalBlocker blocker(ui->chkDisableTyping);
            ui->chkDisableTyping->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Disable While Typing"));
        }
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
    if (ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::MOUSE)) {
        ui->chkMouseNatural->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::MOUSE, GnomeKey::NATURAL_SCROLL));

        double mouseSpeed = ToolManager::ins()->gnomeSettings()->getD(GnomeSchema::MOUSE, GnomeKey::SPEED);
        ui->sliderMouseSpeed->setValue(static_cast<int>(mouseSpeed * 100));
        ui->lblMouseSpeedVal->setText(QString::number(mouseSpeed, 'f', 2));

        ui->cmbAccelProfile->addItem(tr("Default"),  "default");
        ui->cmbAccelProfile->addItem(tr("Flat"),      "flat");
        ui->cmbAccelProfile->addItem(tr("Adaptive"), "adaptive");
        QString ap = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::MOUSE, GnomeKey::ACCEL_PROFILE);
        int apIdx = ui->cmbAccelProfile->findData(ap);
        if (apIdx >= 0) ui->cmbAccelProfile->setCurrentIndex(apIdx);

        ui->chkLeftHanded->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::MOUSE, GnomeKey::LEFT_HANDED));
    }

    // Touchpad
    if (ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::TOUCHPAD)) {
        ui->chkTapToClick->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::TOUCHPAD, GnomeKey::TAP_TO_CLICK));
        ui->chkTouchpadNatural->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::TOUCHPAD, GnomeKey::NATURAL_SCROLL));

        double tpSpeed = ToolManager::ins()->gnomeSettings()->getD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED);
        ui->sliderTouchpadSpeed->setValue(static_cast<int>(tpSpeed * 100));
        ui->lblTouchpadSpeedVal->setText(QString::number(tpSpeed, 'f', 2));

        ui->chkTwoFingerScroll->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::TOUCHPAD, GnomeKey::TWO_FINGER_SCROLL));
        ui->chkEdgeScrolling->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::TOUCHPAD, GnomeKey::EDGE_SCROLLING));
        ui->chkDisableTyping->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::TOUCHPAD, GnomeKey::DISABLE_TYPING));
    }

    mLoading = false;
}
