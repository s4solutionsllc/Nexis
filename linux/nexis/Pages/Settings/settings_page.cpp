#include "settings_page.h"
#include "ui_settings_page.h"
#include "Managers/info_manager.h"
#include "utilities.h"
#include <QApplication>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QUrl>
#include <QLineEdit>

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

    // Set version label dynamically from cmake-derived APP_VERSION
    ui->lblCreatedBy->setText(
        QString("<html><head/><body><p>Nexis v%1 "
                "<a href=\"https://github.com/lsimpsonsfdc\">"
                "<span style=\" text-decoration: underline; color:#007af4;\">"
                "Luke Simpson</span></a></p></body></html>")
            .arg(qApp->applicationVersion()));

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

    // load disks
    InfoManager::ins()->updateDiskInfo();
    const QList<Disk> disks = InfoManager::ins()->getDisks();

    for (const Disk &disk : disks) {
        ui->cmbDisks->addItem(QString("%1  (%2)").arg(disk.device).arg(disk.name), disk.name);
    }

    QString dk = mSettingManager->getDiskName().isEmpty() ? QStorageInfo::root().displayName() : mSettingManager->getDiskName();
    if (! dk.isEmpty()) {
        ui->cmbDisks->setCurrentIndex(ui->cmbDisks->findData(dk));
    }

    // start on boot (autostart .desktop)
    mStartupAppPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation).append("/autostart");
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/nexis.desktop");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        QStringList appContent = FileUtil::readListFromFile(mStartupAppPath);
        QString isHidden = Utilities::getDesktopValue(QRegularExpression("^Hidden=.*"), appContent).toLower();
        ui->checkAutostart->setChecked(isHidden == "false");
    } else {
        ui->checkAutostart->setChecked(false);
    }

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

    // disk analyzer preference
    initDiskAnalyzerCombo();

    // effects
    QList<QWidget*> widgets = {
        ui->cmbLanguages, ui->cmbDisks, ui->cmbStartPage, ui->cmbColorScheme, ui->btnDonate,
        ui->spinCpuPercent, ui->spinMemoryPercent, ui->spinDiskPercent, ui->cmbDiskAnalyzer
    };

    Utilities::addDropShadow(widgets, 50);

    // connects
    connect(ui->cmbLanguages, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbLanguagesChanged(int)));
    connect(ui->cmbDisks, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbDiskChanged(int)));
    connect(ui->cmbStartPage, SIGNAL(currentTextChanged(QString)), this, SLOT(cmbStartPageChanged(QString)));
    connect(ui->cmbColorScheme, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbColorSchemeChanged(int)));
    connect(ui->cmbDiskAnalyzer, SIGNAL(currentIndexChanged(int)), this, SLOT(cmbDiskAnalyzerChanged(int)));
}

void SettingsPage::cmbLanguagesChanged(const int &index)
{
    QString langCode = ui->cmbLanguages->itemData(index).toString();

    mSettingManager->setLanguage(langCode);
}

void SettingsPage::cmbDiskChanged(const int &index)
{
    QString diskName = ui->cmbDisks->itemData(index).toString();

    mSettingManager->setDiskName(diskName);
}

void SettingsPage::on_checkAutostart_clicked(bool checked)
{
    if (checked) {
        QString appTemplate = QString("[Desktop Entry]\n"
                                      "Name=Nexis\n"
                                      "Comment=Linux System Optimizer and Monitoring\n"
                                      "Exec=nexis --hide \n"
                                      "Type=Application\n"
                                      "Terminal=false\n"
                                      "Hidden=false\n");
        FileUtil::writeFile(mStartupAppPath, appTemplate);
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

void SettingsPage::initDiskAnalyzerCombo()
{
    // Platform-specific tool list for Linux
    ui->cmbDiskAnalyzer->addItem(tr("Auto (Detect)"), "auto");
    ui->cmbDiskAnalyzer->addItem(tr("Baobab (GNOME Disk Usage Analyzer)"), "baobab");
    ui->cmbDiskAnalyzer->addItem(tr("Filelight (KDE)"), "filelight");
    ui->cmbDiskAnalyzer->addItem(tr("QDirStat"), "qdirstat");
    ui->cmbDiskAnalyzer->addItem(tr("ncdu (Terminal)"), "ncdu");
    ui->cmbDiskAnalyzer->addItem(tr("Custom..."), "custom");

    // Restore saved preference
    QString saved = mSettingManager->getDiskAnalyzerTool();
    int idx = ui->cmbDiskAnalyzer->findData(saved);
    if (idx >= 0)
        ui->cmbDiskAnalyzer->setCurrentIndex(idx);
    else
        ui->cmbDiskAnalyzer->setCurrentIndex(0); // fallback to Auto

    // Restore custom path
    ui->txtDiskAnalyzerCustomPath->setText(mSettingManager->getDiskAnalyzerCustomPath());

    updateCustomPathVisibility();
}

void SettingsPage::updateCustomPathVisibility()
{
    bool isCustom = (ui->cmbDiskAnalyzer->currentData().toString() == "custom");
    ui->lblDiskAnalyzerCustomPath->setVisible(isCustom);
    ui->txtDiskAnalyzerCustomPath->setVisible(isCustom);
}

void SettingsPage::cmbDiskAnalyzerChanged(int index)
{
    QString tool = ui->cmbDiskAnalyzer->itemData(index).toString();
    mSettingManager->setDiskAnalyzerTool(tool);
    updateCustomPathVisibility();
}

void SettingsPage::on_txtDiskAnalyzerCustomPath_editingFinished()
{
    mSettingManager->setDiskAnalyzerCustomPath(ui->txtDiskAnalyzerCustomPath->text().trimmed());
}
