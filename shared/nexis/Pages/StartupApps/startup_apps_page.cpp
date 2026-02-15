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
#ifdef Q_OS_MACOS
    // macOS LaunchAgents directory is never globally disabled
    Q_UNUSED(as_path);
    return false;
#else
    // Linux: check if X-GNOME-Autostart-enabled=false is present
    const QByteArray disabled_str("X-GNOME-Autostart-enabled=false");
    QFile autostart_file(as_path);

    autostart_file.open(QIODevice::ReadOnly | QIODevice::Text);

    return autostart_file.readAll().indexOf(disabled_str, 0) != -1;
#endif
}

void StartupAppsPage::init()
{
#ifdef Q_OS_MACOS
    mAutostartPath = QDir::homePath() + "/Library/LaunchAgents";
#else
    mAutostartPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation).append("/autostart");
#endif

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

#ifdef Q_OS_MACOS
    // macOS: load .plist files from ~/Library/LaunchAgents
    QDir autostartFiles(mAutostartPath, "*.plist");

    for (const QFileInfo &f : autostartFiles.entryInfoList())
    {
        // completeBaseName() gives the full name minus the last extension,
        // e.g. "com.jetbrains.toolbox.plist" → "com.jetbrains.toolbox"
        // (baseName() only returns the part before the *first* dot, i.e. "com")
        QString identifier = f.completeBaseName();

        // Skip Apple internal agents
        if (identifier.startsWith("com.apple."))
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

        // Derive a human-friendly display name from the reverse-DNS identifier.
        // e.g. "com.jetbrains.toolbox" → "Jetbrains Toolbox"
        //      "com.Tautulli.Tautulli" → "Tautulli"
        //      "homebrew.mxcl.redis"   → "Redis"
        QString appName = identifier;
        QStringList parts = identifier.split('.');
        if (parts.size() >= 3) {
            // Drop the first component (com/org/io/homebrew/etc.) and deduplicate
            QStringList nameParts;
            for (int i = 1; i < parts.size(); ++i) {
                // Skip parts that duplicate the previous one (case-insensitive),
                // e.g. "com.Tautulli.Tautulli" → just "Tautulli"
                if (!nameParts.isEmpty()
                    && nameParts.last().compare(parts.at(i), Qt::CaseInsensitive) == 0)
                    continue;
                QString p = parts.at(i);
                if (!p.isEmpty())
                    p[0] = p[0].toUpper();
                nameParts << p;
            }
            appName = nameParts.join(' ');
        } else if (parts.size() == 2) {
            // Two-part identifier — use the second component
            appName = parts.last();
            if (!appName.isEmpty())
                appName[0] = appName[0].toUpper();
        }

        QListWidgetItem *item = new QListWidgetItem(ui->listWidgetStartup);

        StartupApp *app = new StartupApp(appName, enabled, f.absoluteFilePath(), this);

        connect(app, &StartupApp::deleteAppS, this, &StartupAppsPage::loadApps);
        connect(app, &StartupApp::editStartupAppS, this, &StartupAppsPage::openStartupAppEdit);

        item->setSizeHint(app->sizeHint());

        ui->listWidgetStartup->setItemWidget(item, app);
    }
#else
    // Linux: load .desktop files from XDG autostart directory
    QDir autostartFiles(mAutostartPath, "*.desktop");

    QLatin1String enabledStr("true");
    for (const QFileInfo &f : autostartFiles.entryInfoList())
    {
        QStringList lines = FileUtil::readListFromFile(f.absoluteFilePath());

        QString appName = Utilities::getDesktopValue(NAME_REG, lines); // get name

        if(! appName.isEmpty()) // has a name
        {
            bool enabled = false;

            // Hidden=[true|false]
            QString hidden = Utilities::getDesktopValue(HIDDEN_REG, lines).toLower();

            // X-GNOME-Autostart-enabled=[true|false]
            QString gnomeEnabled = Utilities::getDesktopValue(GNOME_ENABLED_REG, lines).toLower();

            if (! hidden.isEmpty()) {
                enabled = (hidden != enabledStr);
            } else {
                enabled = (gnomeEnabled == enabledStr);
            }

            QListWidgetItem *item = new QListWidgetItem(ui->listWidgetStartup);

            // new app
            StartupApp *app = new StartupApp(appName, enabled, f.absoluteFilePath(), this);

            connect(app, &StartupApp::deleteAppS, this, &StartupAppsPage::loadApps);
            connect(app, &StartupApp::editStartupAppS, this, &StartupAppsPage::openStartupAppEdit);

            item->setSizeHint(app->sizeHint());

            ui->listWidgetStartup->setItemWidget(item, app);
        }
    }
#endif

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
