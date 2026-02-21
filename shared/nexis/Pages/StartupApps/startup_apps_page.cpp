#include "startup_apps_page.h"
#include "ui_startup_apps_page.h"
#include "utilities.h"
#include "Services/startup_service.h"

#ifdef Q_OS_MACOS
#include <QFileIconProvider>
#endif

StartupAppsPage::~StartupAppsPage()
{
    delete ui;
}

StartupAppsPage::StartupAppsPage(QWidget *parent, StartupService *startupService) :
    QWidget(parent),
    ui(new Ui::StartupAppsPage),
    mStartupService(startupService ? startupService : StartupService::ins())
{
    ui->setupUi(this);

    init();
}

void StartupAppsPage::init()
{
    if (mStartupService->isAutostartDisabled()) {
        ui->lblNotFound->setText(tr("Startup Apps are disabled."));
        ui->btnAddStartupApp->setEnabled(false);
    } else {
        loadApps();

        connect(mStartupService, &StartupService::appsChanged,
                this, &StartupAppsPage::loadApps);
    }

    connect(ui->btnAddStartupApp, &QPushButton::clicked, this, [this]() { openStartupAppEdit(); });
    connect(ui->txtSearchStartup, &QLineEdit::textChanged, this, &StartupAppsPage::filterStartupApps);

    Utilities::addDropShadow({ui->btnAddStartupApp, ui->txtSearchStartup}, 60);
}

void StartupAppsPage::loadApps()
{
    ui->txtSearchStartup->clear();
    ui->listWidgetStartup->clear();

    QList<StartupAppData> apps = mStartupService->getApps();

    for (const StartupAppData &appData : apps) {
        QListWidgetItem *item = new QListWidgetItem(ui->listWidgetStartup);

        StartupApp *app = new StartupApp(appData.name, appData.enabled,
                                          appData.filePath, appData.iconPath, this);

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

void StartupAppsPage::filterStartupApps(const QString &text)
{
    for (int i = 0; i < ui->listWidgetStartup->count(); ++i) {
        QListWidgetItem *item = ui->listWidgetStartup->item(i);
        StartupApp *app = qobject_cast<StartupApp*>(ui->listWidgetStartup->itemWidget(item));
        if (app) {
            bool matches = text.isEmpty() || app->getAppName().contains(text, Qt::CaseInsensitive);
            item->setHidden(!matches);
        }
    }
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
