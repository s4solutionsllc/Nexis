#include "gnome_appearance_tab.h"
#include "Managers/tool_manager.h"
#include "ui_gnome_appearance_tab.h"

#include <QDir>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>

GnomeAppearanceTab::GnomeAppearanceTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeAppearanceTab),
    mLoading(false)
{
    ui->setupUi(this);

    // Make transparent so @pageContent background shows through in dark mode
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContents->setAutoFillBackground(false);

    // Monospace font filter
    ui->fontMonoFont->setFontFilters(QFontComboBox::MonospacedFonts);

    loadSettings();

    // Color Scheme
    connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME,
                                     ui->cmbColorScheme->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbColorScheme);
            int idx = ui->cmbColorScheme->findData(prevVal);
            if (idx >= 0) ui->cmbColorScheme->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Color Scheme"));
        }
    });

    // GTK Theme
    connect(ui->cmbGtkTheme, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (mLoading) return;
        QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME, text)) {
            const QSignalBlocker blocker(ui->cmbGtkTheme);
            ui->cmbGtkTheme->setCurrentText(prev);
            emit settingFailed(tr("Failed to apply GTK Theme"));
        }
    });

    // Icon Theme
    connect(ui->cmbIconTheme, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (mLoading) return;
        QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME, text)) {
            const QSignalBlocker blocker(ui->cmbIconTheme);
            ui->cmbIconTheme->setCurrentText(prev);
            emit settingFailed(tr("Failed to apply Icon Theme"));
        }
    });

    // Cursor Theme
    connect(ui->cmbCursorTheme, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (mLoading) return;
        QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME, text)) {
            const QSignalBlocker blocker(ui->cmbCursorTheme);
            ui->cmbCursorTheme->setCurrentText(prev);
            emit settingFailed(tr("Failed to apply Cursor Theme"));
        }
    });

    // Cursor Size
    connect(ui->spinCursorSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (mLoading) return;
        int prev = ToolManager::ins()->gnomeSettings()->getI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE);
        if (!ToolManager::ins()->gnomeSettings()->setI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE, val)) {
            const QSignalBlocker blocker(ui->spinCursorSize);
            ui->spinCursorSize->setValue(prev);
            emit settingFailed(tr("Failed to apply Cursor Size"));
        }
    });

    // UI Font
    connect(ui->fontFont, &QFontComboBox::currentFontChanged, this, [this]() {
        if (mLoading) return;
        applyFont(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME,
                  ui->fontFont, ui->spinFontSize, tr("Font"));
    });
    connect(ui->spinFontSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        if (mLoading) return;
        applyFont(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME,
                  ui->fontFont, ui->spinFontSize, tr("Font"));
    });

    // Document Font
    connect(ui->fontDocFont, &QFontComboBox::currentFontChanged, this, [this]() {
        if (mLoading) return;
        applyFont(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT,
                  ui->fontDocFont, ui->spinDocFontSize, tr("Document Font"));
    });
    connect(ui->spinDocFontSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        if (mLoading) return;
        applyFont(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT,
                  ui->fontDocFont, ui->spinDocFontSize, tr("Document Font"));
    });

    // Monospace Font
    connect(ui->fontMonoFont, &QFontComboBox::currentFontChanged, this, [this]() {
        if (mLoading) return;
        applyFont(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT,
                  ui->fontMonoFont, ui->spinMonoFontSize, tr("Monospace Font"));
    });
    connect(ui->spinMonoFontSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        if (mLoading) return;
        applyFont(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT,
                  ui->fontMonoFont, ui->spinMonoFontSize, tr("Monospace Font"));
    });

    // Text Scaling
    connect(ui->spinTextScaling, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (mLoading) return;
        double prev = ToolManager::ins()->gnomeSettings()->getD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING);
        if (!ToolManager::ins()->gnomeSettings()->setD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING, val)) {
            const QSignalBlocker blocker(ui->spinTextScaling);
            ui->spinTextScaling->setValue(prev);
            emit settingFailed(tr("Failed to apply Text Scaling"));
        }
    });

    // Checkboxes
    connect(ui->chkAnimations, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_ANIMATIONS, checked)) {
            const QSignalBlocker blocker(ui->chkAnimations);
            ui->chkAnimations->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Animations"));
        }
    });
    connect(ui->chkHotCorners, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_HOT_CORNERS, checked)) {
            const QSignalBlocker blocker(ui->chkHotCorners);
            ui->chkHotCorners->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Hot Corners"));
        }
    });
    connect(ui->chkClockSeconds, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_SECONDS, checked)) {
            const QSignalBlocker blocker(ui->chkClockSeconds);
            ui->chkClockSeconds->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Clock Seconds"));
        }
    });
    connect(ui->chkClockWeekday, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_WEEKDAY, checked)) {
            const QSignalBlocker blocker(ui->chkClockWeekday);
            ui->chkClockWeekday->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Clock Weekday"));
        }
    });
    connect(ui->chkBatteryPct, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::INTERFACE, GnomeKey::SHOW_BATTERY_PCT, checked)) {
            const QSignalBlocker blocker(ui->chkBatteryPct);
            ui->chkBatteryPct->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Battery Percentage"));
        }
    });

    // Clock Format
    connect(ui->cmbClockFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT,
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
        QString prevVal = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING,
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
        QString prevVal = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING,
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
    QString colorScheme = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME);
    int csIdx = ui->cmbColorScheme->findData(colorScheme);
    if (csIdx >= 0) ui->cmbColorScheme->setCurrentIndex(csIdx);

    // Themes — populate from filesystem, then set current value
    ui->cmbGtkTheme->addItems(discoverGtkThemes());
    ui->cmbGtkTheme->setCurrentText(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::GTK_THEME));

    ui->cmbIconTheme->addItems(discoverIconThemes());
    ui->cmbIconTheme->setCurrentText(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::ICON_THEME));

    ui->cmbCursorTheme->addItems(discoverCursorThemes());
    ui->cmbCursorTheme->setCurrentText(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::CURSOR_THEME));

    ui->spinCursorSize->setValue(ToolManager::ins()->gnomeSettings()->getI(GnomeSchema::INTERFACE, GnomeKey::CURSOR_SIZE));

    // Fonts — parse "FontFamily Size" and set combo + spin
    QString family;
    int size;

    parseFontValue(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::FONT_NAME), family, size);
    ui->fontFont->setCurrentFont(QFont(family));
    ui->spinFontSize->setValue(size);

    parseFontValue(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::DOCUMENT_FONT), family, size);
    ui->fontDocFont->setCurrentFont(QFont(family));
    ui->spinDocFontSize->setValue(size);

    parseFontValue(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::MONOSPACE_FONT), family, size);
    ui->fontMonoFont->setCurrentFont(QFont(family));
    ui->spinMonoFontSize->setValue(size);

    ui->spinTextScaling->setValue(ToolManager::ins()->gnomeSettings()->getD(GnomeSchema::INTERFACE, GnomeKey::TEXT_SCALING));

    // Checkboxes
    ui->chkAnimations->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_ANIMATIONS));
    ui->chkHotCorners->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::INTERFACE, GnomeKey::ENABLE_HOT_CORNERS));
    ui->chkClockSeconds->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_SECONDS));
    ui->chkClockWeekday->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::INTERFACE, GnomeKey::CLOCK_WEEKDAY));
    ui->chkBatteryPct->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::INTERFACE, GnomeKey::SHOW_BATTERY_PCT));

    // Clock Format
    ui->cmbClockFormat->addItem(tr("12 Hour"), "12h");
    ui->cmbClockFormat->addItem(tr("24 Hour"), "24h");
    QString clockFmt = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::CLOCK_FORMAT);
    int cfIdx = ui->cmbClockFormat->findData(clockFmt);
    if (cfIdx >= 0) ui->cmbClockFormat->setCurrentIndex(cfIdx);

    // Font Antialiasing
    ui->cmbAntialiasing->addItem(tr("None"),      "none");
    ui->cmbAntialiasing->addItem(tr("Grayscale"), "grayscale");
    ui->cmbAntialiasing->addItem(tr("RGBA"),      "rgba");
    QString aa = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::FONT_ANTIALIASING);
    int aaIdx = ui->cmbAntialiasing->findData(aa);
    if (aaIdx >= 0) ui->cmbAntialiasing->setCurrentIndex(aaIdx);

    // Font Hinting
    ui->cmbHinting->addItem(tr("None"),   "none");
    ui->cmbHinting->addItem(tr("Slight"), "slight");
    ui->cmbHinting->addItem(tr("Medium"), "medium");
    ui->cmbHinting->addItem(tr("Full"),   "full");
    QString hint = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::INTERFACE, GnomeKey::FONT_HINTING);
    int hIdx = ui->cmbHinting->findData(hint);
    if (hIdx >= 0) ui->cmbHinting->setCurrentIndex(hIdx);

    mLoading = false;
}

QStringList GnomeAppearanceTab::discoverGtkThemes()
{
    QSet<QString> themes;
    QStringList searchPaths = {
        QStringLiteral("/usr/share/themes"),
        QDir::homePath() + QStringLiteral("/.local/share/themes"),
        QDir::homePath() + QStringLiteral("/.themes")
    };

    for (const QString &basePath : searchPaths) {
        QDir baseDir(basePath);
        if (!baseDir.exists()) continue;
        for (const QString &entry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir themeDir(basePath + "/" + entry);
            if (themeDir.exists("gtk-3.0") || themeDir.exists("gtk-4.0"))
                themes.insert(entry);
        }
    }

    QStringList result(themes.begin(), themes.end());
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList GnomeAppearanceTab::discoverIconThemes()
{
    QSet<QString> themes;
    QStringList searchPaths = {
        QStringLiteral("/usr/share/icons"),
        QDir::homePath() + QStringLiteral("/.local/share/icons"),
        QDir::homePath() + QStringLiteral("/.icons")
    };

    for (const QString &basePath : searchPaths) {
        QDir baseDir(basePath);
        if (!baseDir.exists()) continue;
        for (const QString &entry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry == "default") continue;
            QDir iconDir(basePath + "/" + entry);
            if (iconDir.exists("index.theme"))
                themes.insert(entry);
        }
    }

    QStringList result(themes.begin(), themes.end());
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList GnomeAppearanceTab::discoverCursorThemes()
{
    QSet<QString> themes;
    QStringList searchPaths = {
        QStringLiteral("/usr/share/icons"),
        QDir::homePath() + QStringLiteral("/.local/share/icons"),
        QDir::homePath() + QStringLiteral("/.icons")
    };

    for (const QString &basePath : searchPaths) {
        QDir baseDir(basePath);
        if (!baseDir.exists()) continue;
        for (const QString &entry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir iconDir(basePath + "/" + entry);
            if (iconDir.exists("cursors"))
                themes.insert(entry);
        }
    }

    QStringList result(themes.begin(), themes.end());
    result.sort(Qt::CaseInsensitive);
    return result;
}

void GnomeAppearanceTab::parseFontValue(const QString &value, QString &family, int &size)
{
    size = 11;
    family = value;

    int lastSpace = value.lastIndexOf(' ');
    if (lastSpace > 0) {
        bool ok;
        int parsed = value.mid(lastSpace + 1).toInt(&ok);
        if (ok && parsed > 0) {
            family = value.left(lastSpace);
            size = parsed;
        }
    }
}

void GnomeAppearanceTab::applyFont(const QString &schema, const QString &key,
                                    QFontComboBox *combo, QSpinBox *spin, const QString &label)
{
    QString newValue = combo->currentFont().family() + " " + QString::number(spin->value());
    QString prev = ToolManager::ins()->gnomeSettings()->getS(schema, key);
    if (!ToolManager::ins()->gnomeSettings()->setS(schema, key, newValue)) {
        QString prevFamily;
        int prevSize;
        parseFontValue(prev, prevFamily, prevSize);
        const QSignalBlocker b1(combo);
        const QSignalBlocker b2(spin);
        combo->setCurrentFont(QFont(prevFamily));
        spin->setValue(prevSize);
        emit settingFailed(tr("Failed to apply %1").arg(label));
    }
}
