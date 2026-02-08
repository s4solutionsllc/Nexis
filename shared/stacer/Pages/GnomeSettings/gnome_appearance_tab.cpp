#include "gnome_appearance_tab.h"
#include "ui_gnome_appearance_tab.h"

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
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME,
                                ui->cmbColorScheme->currentData().toString());
    });

    // GTK Theme
    connect(ui->editGtkTheme, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME, ui->editGtkTheme->text());
    });

    // Icon Theme
    connect(ui->editIconTheme, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME, ui->editIconTheme->text());
    });

    // Cursor Theme
    connect(ui->editCursorTheme, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME, ui->editCursorTheme->text());
    });

    // Cursor Size
    connect(ui->spinCursorSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (mLoading) return;
        GnomeSettingsTool::setI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE, val);
    });

    // Fonts
    connect(ui->editFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME, ui->editFont->text());
    });
    connect(ui->editDocFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT, ui->editDocFont->text());
    });
    connect(ui->editMonoFont, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT, ui->editMonoFont->text());
    });

    // Text Scaling
    connect(ui->spinTextScaling, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (mLoading) return;
        GnomeSettingsTool::setD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING, val);
    });

    // Checkboxes
    connect(ui->chkAnimations, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_ANIMATIONS, checked);
    });
    connect(ui->chkHotCorners, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_HOT_CORNERS, checked);
    });
    connect(ui->chkClockSeconds, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_SECONDS, checked);
    });
    connect(ui->chkClockWeekday, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_WEEKDAY, checked);
    });
    connect(ui->chkBatteryPct, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        GnomeSettingsTool::setB(GnomeSchema::INTERFACE, GnomeKey::SHOW_BATTERY_PCT, checked);
    });

    // Clock Format
    connect(ui->cmbClockFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT,
                                ui->cmbClockFormat->currentData().toString());
    });

    // Font Antialiasing
    connect(ui->cmbAntialiasing, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING,
                                ui->cmbAntialiasing->currentData().toString());
    });

    // Font Hinting
    connect(ui->cmbHinting, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING,
                                ui->cmbHinting->currentData().toString());
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
