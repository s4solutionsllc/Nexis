#include "gnome_appearance_tab.h"
#include "ui_gnome_appearance_tab.h"

#include <QSignalBlocker>
#include <Tools/gnome_settings_tool.h>

GnomeAppearanceTab::GnomeAppearanceTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeAppearanceTab),
    mLoading(false)
{
    ui->setupUi(this);

    // Make transparent so @pageContent background shows through in dark mode
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContents->setAutoFillBackground(false);

    loadSettings();

    // Color Scheme
    connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME,
                                     ui->cmbColorScheme->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbColorScheme);
            int idx = ui->cmbColorScheme->findData(prevVal);
            if (idx >= 0) ui->cmbColorScheme->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Color Scheme"));
        }
    });

    // GTK Theme
    connect(ui->editGtkTheme, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME, ui->editGtkTheme->text())) {
            const QSignalBlocker blocker(ui->editGtkTheme);
            ui->editGtkTheme->setText(prev);
            emit settingFailed(tr("Failed to apply GTK Theme"));
        }
    });

    // Icon Theme
    connect(ui->editIconTheme, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME, ui->editIconTheme->text())) {
            const QSignalBlocker blocker(ui->editIconTheme);
            ui->editIconTheme->setText(prev);
            emit settingFailed(tr("Failed to apply Icon Theme"));
        }
    });

    // Cursor Theme
    connect(ui->editCursorTheme, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME, ui->editCursorTheme->text())) {
            const QSignalBlocker blocker(ui->editCursorTheme);
            ui->editCursorTheme->setText(prev);
            emit settingFailed(tr("Failed to apply Cursor Theme"));
        }
    });

    // Cursor Size
    connect(ui->spinCursorSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (mLoading) return;
        int prev = GnomeSettingsTool::getI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE);
        if (!GnomeSettingsTool::setI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE, val)) {
            const QSignalBlocker blocker(ui->spinCursorSize);
            ui->spinCursorSize->setValue(prev);
            emit settingFailed(tr("Failed to apply Cursor Size"));
        }
    });

    // Fonts
    connect(ui->editFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME, ui->editFont->text())) {
            const QSignalBlocker blocker(ui->editFont);
            ui->editFont->setText(prev);
            emit settingFailed(tr("Failed to apply Font"));
        }
    });
    connect(ui->editDocFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT, ui->editDocFont->text())) {
            const QSignalBlocker blocker(ui->editDocFont);
            ui->editDocFont->setText(prev);
            emit settingFailed(tr("Failed to apply Document Font"));
        }
    });
    connect(ui->editMonoFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT, ui->editMonoFont->text())) {
            const QSignalBlocker blocker(ui->editMonoFont);
            ui->editMonoFont->setText(prev);
            emit settingFailed(tr("Failed to apply Monospace Font"));
        }
    });

    // Text Scaling
    connect(ui->spinTextScaling, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (mLoading) return;
        double prev = GnomeSettingsTool::getD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING);
        if (!GnomeSettingsTool::setD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING, val)) {
            const QSignalBlocker blocker(ui->spinTextScaling);
            ui->spinTextScaling->setValue(prev);
            emit settingFailed(tr("Failed to apply Text Scaling"));
        }
    });

    // Checkboxes
    connect(ui->chkAnimations, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_ANIMATIONS, checked)) {
            const QSignalBlocker blocker(ui->chkAnimations);
            ui->chkAnimations->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Animations"));
        }
    });
    connect(ui->chkHotCorners, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_HOT_CORNERS, checked)) {
            const QSignalBlocker blocker(ui->chkHotCorners);
            ui->chkHotCorners->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Hot Corners"));
        }
    });
    connect(ui->chkClockSeconds, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_SECONDS, checked)) {
            const QSignalBlocker blocker(ui->chkClockSeconds);
            ui->chkClockSeconds->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Clock Seconds"));
        }
    });
    connect(ui->chkClockWeekday, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_WEEKDAY, checked)) {
            const QSignalBlocker blocker(ui->chkClockWeekday);
            ui->chkClockWeekday->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Clock Weekday"));
        }
    });
    connect(ui->chkBatteryPct, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::SHOW_BATTERY_PCT, checked)) {
            const QSignalBlocker blocker(ui->chkBatteryPct);
            ui->chkBatteryPct->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Battery Percentage"));
        }
    });

    // Clock Format
    connect(ui->cmbClockFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT,
                                     ui->cmbClockFormat->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbClockFormat);
            int idx = ui->cmbClockFormat->findData(prevVal);
            if (idx >= 0) ui->cmbClockFormat->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Clock Format"));
        }
    });

    // Font Antialiasing
    connect(ui->cmbAntialiasing, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING,
                                     ui->cmbAntialiasing->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbAntialiasing);
            int idx = ui->cmbAntialiasing->findData(prevVal);
            if (idx >= 0) ui->cmbAntialiasing->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Font Antialiasing"));
        }
    });

    // Font Hinting
    connect(ui->cmbHinting, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING);
        if (!GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING,
                                     ui->cmbHinting->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbHinting);
            int idx = ui->cmbHinting->findData(prevVal);
            if (idx >= 0) ui->cmbHinting->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Font Hinting"));
        }
    });
}

GnomeAppearanceTab::~GnomeAppearanceTab()
{
    delete ui;
}

void GnomeAppearanceTab::loadSettings()
{
    mLoading = true;

    // Color Scheme
    ui->cmbColorScheme->addItem(tr("Default"),      "default");
    ui->cmbColorScheme->addItem(tr("Prefer Dark"),   "prefer-dark");
    ui->cmbColorScheme->addItem(tr("Prefer Light"),  "prefer-light");
    QString colorScheme = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME);
    int csIdx = ui->cmbColorScheme->findData(colorScheme);
    if (csIdx >= 0) ui->cmbColorScheme->setCurrentIndex(csIdx);

    // Themes
    ui->editGtkTheme->setText(GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME));
    ui->editIconTheme->setText(GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME));
    ui->editCursorTheme->setText(GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME));
    ui->spinCursorSize->setValue(GnomeSettingsTool::getI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE));

    // Fonts
    ui->editFont->setText(GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME));
    ui->editDocFont->setText(GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT));
    ui->editMonoFont->setText(GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT));
    ui->spinTextScaling->setValue(GnomeSettingsTool::getD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING));

    // Checkboxes
    ui->chkAnimations->setChecked(GnomeSettingsTool::getB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_ANIMATIONS));
    ui->chkHotCorners->setChecked(GnomeSettingsTool::getB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_HOT_CORNERS));
    ui->chkClockSeconds->setChecked(GnomeSettingsTool::getB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_SECONDS));
    ui->chkClockWeekday->setChecked(GnomeSettingsTool::getB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_WEEKDAY));
    ui->chkBatteryPct->setChecked(GnomeSettingsTool::getB(GnomeSchema::INTERFACE, GnomeKey::SHOW_BATTERY_PCT));

    // Clock Format
    ui->cmbClockFormat->addItem(tr("12 Hour"), "12h");
    ui->cmbClockFormat->addItem(tr("24 Hour"), "24h");
    QString clockFmt = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT);
    int cfIdx = ui->cmbClockFormat->findData(clockFmt);
    if (cfIdx >= 0) ui->cmbClockFormat->setCurrentIndex(cfIdx);

    // Font Antialiasing
    ui->cmbAntialiasing->addItem(tr("None"),      "none");
    ui->cmbAntialiasing->addItem(tr("Grayscale"), "grayscale");
    ui->cmbAntialiasing->addItem(tr("RGBA"),      "rgba");
    QString aa = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING);
    int aaIdx = ui->cmbAntialiasing->findData(aa);
    if (aaIdx >= 0) ui->cmbAntialiasing->setCurrentIndex(aaIdx);

    // Font Hinting
    ui->cmbHinting->addItem(tr("None"),   "none");
    ui->cmbHinting->addItem(tr("Slight"), "slight");
    ui->cmbHinting->addItem(tr("Medium"), "medium");
    ui->cmbHinting->addItem(tr("Full"),   "full");
    QString hint = GnomeSettingsTool::getS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING);
    int hIdx = ui->cmbHinting->findData(hint);
    if (hIdx >= 0) ui->cmbHinting->setCurrentIndex(hIdx);

    mLoading = false;
}
