#include "gnome_desktop_tab.h"
#include "Managers/tool_manager.h"
#include "ui_gnome_desktop_tab.h"

#include <QFileDialog>
#include <QSignalBlocker>

GnomeDesktopTab::GnomeDesktopTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GnomeDesktopTab),
    mLoading(false)
{
    ui->setupUi(this);

    // Make transparent so @pageContent background shows through in dark mode
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->scrollContents->setAutoFillBackground(false);

    // Hide groups for missing schemas
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::BACKGROUND))
        ui->groupBackground->hide();
    if (!ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::SOUND))
        ui->groupSound->hide();

    loadSettings();

    // Wallpaper light
    connect(ui->editWallpaper, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI, ui->editWallpaper->text())) {
            const QSignalBlocker blocker(ui->editWallpaper);
            ui->editWallpaper->setText(prev);
            emit settingFailed(tr("Failed to apply Wallpaper"));
        }
    });
    connect(ui->btnBrowseWallpaper, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, tr("Select Wallpaper"), QDir::homePath(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.svg *.webp)"));
        if (!file.isEmpty()) {
            QString uri = "file://" + file;
            QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI);
            if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI, uri)) {
                ui->editWallpaper->setText(prev);
                emit settingFailed(tr("Failed to apply Wallpaper"));
            } else {
                ui->editWallpaper->setText(uri);
            }
        }
    });

    // Wallpaper dark
    connect(ui->editWallpaperDark, &QLineEdit::editingFinished, this, [this]() {
        if (mLoading) return;
        QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI_DARK);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI_DARK, ui->editWallpaperDark->text())) {
            const QSignalBlocker blocker(ui->editWallpaperDark);
            ui->editWallpaperDark->setText(prev);
            emit settingFailed(tr("Failed to apply Dark Wallpaper"));
        }
    });
    connect(ui->btnBrowseWallpaperDark, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, tr("Select Dark Wallpaper"), QDir::homePath(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.svg *.webp)"));
        if (!file.isEmpty()) {
            QString uri = "file://" + file;
            QString prev = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI_DARK);
            if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI_DARK, uri)) {
                ui->editWallpaperDark->setText(prev);
                emit settingFailed(tr("Failed to apply Dark Wallpaper"));
            } else {
                ui->editWallpaperDark->setText(uri);
            }
        }
    });

    // Picture options
    connect(ui->cmbPictureOptions, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (mLoading) return;
        QString prevVal = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_OPTIONS);
        if (!ToolManager::ins()->gnomeSettings()->setS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_OPTIONS,
                                     ui->cmbPictureOptions->currentData().toString())) {
            const QSignalBlocker blocker(ui->cmbPictureOptions);
            int idx = ui->cmbPictureOptions->findData(prevVal);
            if (idx >= 0) ui->cmbPictureOptions->setCurrentIndex(idx);
            emit settingFailed(tr("Failed to apply Picture Options"));
        }
    });

    // Sound checkboxes
    connect(ui->chkEventSounds, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::SOUND, GnomeKey::EVENT_SOUNDS, checked)) {
            const QSignalBlocker blocker(ui->chkEventSounds);
            ui->chkEventSounds->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Event Sounds"));
        }
    });
    connect(ui->chkInputFeedback, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::SOUND, GnomeKey::INPUT_FEEDBACK, checked)) {
            const QSignalBlocker blocker(ui->chkInputFeedback);
            ui->chkInputFeedback->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Input Feedback Sounds"));
        }
    });
    connect(ui->chkVolumeOver100, &QCheckBox::toggled, this, [this](bool checked) {
        if (mLoading) return;
        if (!ToolManager::ins()->gnomeSettings()->setB(GnomeSchema::SOUND, GnomeKey::VOLUME_OVER_100, checked)) {
            const QSignalBlocker blocker(ui->chkVolumeOver100);
            ui->chkVolumeOver100->setChecked(!checked);
            emit settingFailed(tr("Failed to apply Volume Over 100%%"));
        }
    });
}

GnomeDesktopTab::~GnomeDesktopTab()
{
    delete ui;
}

void GnomeDesktopTab::loadSettings()
{
    mLoading = true;

    // Background
    if (ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::BACKGROUND)) {
        ui->editWallpaper->setText(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI));
        ui->editWallpaperDark->setText(ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_URI_DARK));

        ui->cmbPictureOptions->addItem(tr("None"),      "none");
        ui->cmbPictureOptions->addItem(tr("Wallpaper"), "wallpaper");
        ui->cmbPictureOptions->addItem(tr("Centered"),  "centered");
        ui->cmbPictureOptions->addItem(tr("Scaled"),    "scaled");
        ui->cmbPictureOptions->addItem(tr("Stretched"), "stretched");
        ui->cmbPictureOptions->addItem(tr("Zoom"),      "zoom");
        ui->cmbPictureOptions->addItem(tr("Spanned"),   "spanned");
        QString po = ToolManager::ins()->gnomeSettings()->getS(GnomeSchema::BACKGROUND, GnomeKey::PICTURE_OPTIONS);
        int poIdx = ui->cmbPictureOptions->findData(po);
        if (poIdx >= 0) ui->cmbPictureOptions->setCurrentIndex(poIdx);
    }

    // Sound
    if (ToolManager::ins()->gnomeSettings()->schemaExists(GnomeSchema::SOUND)) {
        ui->chkEventSounds->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::SOUND, GnomeKey::EVENT_SOUNDS));
        ui->chkInputFeedback->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::SOUND, GnomeKey::INPUT_FEEDBACK));
        ui->chkVolumeOver100->setChecked(ToolManager::ins()->gnomeSettings()->getB(GnomeSchema::SOUND, GnomeKey::VOLUME_OVER_100));
    }

    mLoading = false;
}
