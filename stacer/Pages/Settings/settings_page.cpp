#include "settings_page.h"
#include "ui_settings_page.h"
#include "Managers/info_manager.h"
#include "utilities.h"
#include <QDesktopServices>
#include <QRegularExpression>
#include <QUrl>

SettingsPage::~SettingsPage()
{
    delete ui;
}

SettingsPage::SettingsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingsPage),
    apm(AppManager::ins()),
    mSettingManager(SettingManager::ins())
{
    ui->setupUi(this);

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

    // load themes
//    QMapIterator<QString, QString> theme(apm->getThemeList());

//    while (theme.hasNext()) {
//        theme.next();
//        ui->cmbThemes->addItem(theme.value(), theme.key());
//    }

//    QString tn = mSettingManager->getThemeName();
//    ui->cmbThemes->setCurrentText(apm->getThemeList().value(tn));

    // load disks
    InfoManager::ins()->updateDiskInfo();
    QList<Disk*> disks = InfoManager::ins()->getDisks();

    for (const Disk *disk : disks) {
        ui->cmbDisks->addItem(QString("%1  (%2)").arg(disk->device).arg(disk->name), disk->name);
    }

    QString dk = mSettingManager->getDiskName().isEmpty() ? QStorageInfo::root().displayName() : mSettingManager->getDiskName();
    if (! dk.isEmpty()) {
        ui->cmbDisks->setCurrentIndex(ui->cmbDisks->findData(dk));
    }

    // start on boot
#ifdef Q_OS_LINUX
    mStartupAppPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation).append("/autostart");
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/stacer.desktop");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        QStringList appContent = FileUtil::readListFromFile(mStartupAppPath);
        QString isHidden = Utilities::getDesktopValue(QRegularExpression("^Hidden=.*"), appContent).toLower();
        ui->checkAutostart->setChecked(isHidden == "false");
    } else {
        ui->checkAutostart->setChecked(false);
    }
#elif defined(Q_OS_MACOS)
    mStartupAppPath = QDir::homePath() + "/Library/LaunchAgents";
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/com.stacer.app.plist");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        ui->checkAutostart->setChecked(true);
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

    // effects
    QList<QWidget*> widgets = {
        ui->cmbLanguages, ui->cmbDisks, ui->cmbStartPage, ui->cmbColorScheme, ui->btnDonate,
        ui->spinCpuPercent, ui->spinMemoryPercent, ui->spinDiskPercent
    };

    Utilities::addDropShadow(widgets, 50);

    // connects
    connect(ui->cmbLanguages, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbLanguagesChanged(int)));
//    connect(ui->cmbThemes, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbThemesChanged(int)));
    connect(ui->cmbDisks, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbDiskChanged(int)));
    connect(ui->cmbStartPage, SIGNAL(currentTextChanged(QString)), this, SLOT(cmbStartPageChanged(QString)));
    connect(ui->cmbColorScheme, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbColorSchemeChanged(int)));
}

void SettingsPage::cmbLanguagesChanged(const int &index)
{
    QString langCode = ui->cmbLanguages->itemData(index).toString();

    mSettingManager->setLanguage(langCode);
}

//void SettingsPage::cmbThemesChanged(const int &index)
//{
//    QString themeName = ui->cmbThemes->itemData(index).toString();

//    mSettingManager->setThemeName(themeName);
//    apm->updateStylesheet();
//}

void SettingsPage::cmbDiskChanged(const int &index)
{
    QString diskName = ui->cmbDisks->itemData(index).toString();

    mSettingManager->setDiskName(diskName);
}

void SettingsPage::on_checkAutostart_clicked(bool checked)
{
    if (checked) {
#ifdef Q_OS_LINUX
        QString appTemplate = QString("[Desktop Entry]\n"
                                      "Name=Stacer\n"
                                      "Comment=Linux System Optimizer and Monitoring\n"
                                      "Exec=stacer --hide \n"
                                      "Type=Application\n"
                                      "Terminal=false\n"
                                      "Hidden=false\n");
        FileUtil::writeFile(mStartupAppPath, appTemplate);
#elif defined(Q_OS_MACOS)
        QString plistTemplate = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>com.stacer.app</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "        <string>stacer</string>\n"
            "        <string>--hide</string>\n"
            "    </array>\n"
            "    <key>RunAtLoad</key>\n"
            "    <true/>\n"
            "</dict>\n"
            "</plist>\n");
        FileUtil::writeFile(mStartupAppPath, plistTemplate);
#endif
    } else {
        QFile::remove(mStartupAppPath);
    }
}

void SettingsPage::on_btnDonate_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/lsimpsonsfdc"));
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
