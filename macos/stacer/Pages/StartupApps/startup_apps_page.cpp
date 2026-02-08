#include "startup_apps_page.h"
#include "ui_startup_apps_page.h"
#include "utilities.h"

StartupAppsPage::~StartupAppsPage()
{
    delete ui;
}

StartupAppsPage::StartupAppsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StartupAppsPage),
    mFileSystemWatcher(this)
{
    ui->setupUi(this);

    init();
}

bool StartupAppsPage::checkIfDisabled(const QString& as_path)
{
    Q_UNUSED(as_path);
    return false;
}

void StartupAppsPage::init()
{
    mAutostartPath = QDir::homePath() + "/Library/LaunchAgents";

    QFileInfo asfi(mAutostartPath);
    bool startups_disabled = false;

    if (asfi.isDir() == true) {
        mAutostartPath.append("/");
    }
    else {
        startups_disabled = checkIfDisabled(mAutostartPath);
    }

    if (!startups_disabled) {
        if (! QDir(mAutostartPath).exists()) {
            QDir().mkdir(mAutostartPath);
        }

        mFileSystemWatcher.addPath(mAutostartPath);

        loadApps();

        connect(&mFileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, &StartupAppsPage::loadApps);
    }
    else {
        ui->lblNotFound->setText(tr("Startup Apps are disabled."));
        ui->btnAddStartupApp->setEnabled(false);
    }

    connect(ui->btnAddStartupApp, SIGNAL(clicked()), this, SLOT(openStartupAppEdit()));

    Utilities::addDropShadow(ui->btnAddStartupApp, 60);
}

void StartupAppsPage::loadApps()
{
    // clear
    ui->listWidgetStartup->clear();

    // On macOS, look for .plist files in ~/Library/LaunchAgents
    QDir autostartFiles(mAutostartPath, "*.plist");

    for (const QFileInfo &f : autostartFiles.entryInfoList())
    {
        // Extract the label from the plist filename (remove .plist extension)
        QString appName = f.baseName();

        // Skip Apple internal agents
        if (appName.startsWith("com.apple."))
            continue;

        // Check if the plist has a Disabled key
        bool enabled = true;
        QStringList lines = FileUtil::readListFromFile(f.absoluteFilePath());
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).contains("Disabled") && i + 1 < lines.size()) {
                enabled = !lines.at(i + 1).contains("true");
                break;
            }
        }

        QListWidgetItem *item = new QListWidgetItem(ui->listWidgetStartup);

        StartupApp *app = new StartupApp(appName, enabled, f.absoluteFilePath(), this);

        connect(app, &StartupApp::deleteAppS, this, &StartupAppsPage::loadApps);
        connect(app, &StartupApp::editStartupAppS, this, &StartupAppsPage::openStartupAppEdit);

        item->setSizeHint(app->sizeHint());

        ui->listWidgetStartup->setItemWidget(item, app);
    }

    setAppCount();
}

void StartupAppsPage::setAppCount()
{
    int count = ui->listWidgetStartup->count();

    ui->lblStartupAppsTitle->setText(
        tr("Startup Applications (%1)")
        .arg(QString::number(count)));

    ui->notFoundWidget->setVisible(! count);
    ui->listWidgetStartup->setVisible(count);
}

void StartupAppsPage::openStartupAppEdit(const QString filePath)
{
    StartupAppEdit::selectedFilePath = filePath;
    if (mStartupAppEdit.isNull()) {
        mStartupAppEdit = QSharedPointer<StartupAppEdit>(new StartupAppEdit(this));
        connect(mStartupAppEdit.data(), &StartupAppEdit::startupAppAdded, this, &StartupAppsPage::loadApps);
    }
    mStartupAppEdit->show();
}
