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

void StartupAppsPage::addSectionHeader(const QString &title)
{
    auto *label = new QLabel(title, this);
    label->setObjectName(QStringLiteral("startupSectionHeader"));

    auto *item = new QListWidgetItem(ui->listWidgetStartup);
    item->setFlags(Qt::NoItemFlags);
    item->setSizeHint(label->sizeHint());
    ui->listWidgetStartup->setItemWidget(item, label);

    SectionGroup group;
    group.headerItem = item;
    mSectionGroups.append(group);
}

void StartupAppsPage::loadApps()
{
    ui->txtSearchStartup->clear();
    ui->listWidgetStartup->clear();
    mSectionGroups.clear();

#ifdef Q_OS_MACOS
    QList<StartupAppData> all = mStartupService->getAllLoginItems();
#else
    QList<StartupAppData> all = mStartupService->getApps();
#endif

    // Partition into buckets
    QList<StartupAppData> userAgents, systemAgents, systemDaemons;
    for (const StartupAppData &d : all) {
        switch (d.category) {
        case LoginItemCategory::UserAgent:   userAgents   << d; break;
        case LoginItemCategory::SystemAgent:  systemAgents  << d; break;
        case LoginItemCategory::SystemDaemon: systemDaemons << d; break;
        }
    }

    auto addGroup = [&](const QString &title, const QList<StartupAppData> &items) {
        if (items.isEmpty())
            return;

        addSectionHeader(title);
        SectionGroup &group = mSectionGroups.last();

        for (const StartupAppData &appData : items) {
            auto *item = new QListWidgetItem(ui->listWidgetStartup);

            auto *app = new StartupApp(appData.name, appData.enabled, appData.filePath,
                                       appData.iconPath, appData.readOnly, this);

            if (!appData.readOnly) {
                connect(app, &StartupApp::deleteAppS, this, &StartupAppsPage::loadApps);
                connect(app, &StartupApp::editStartupAppS, this, &StartupAppsPage::openStartupAppEdit);
            }

            QSize hint = app->sizeHint();
            hint.setHeight(qMax(hint.height(), app->minimumHeight()));
            item->setSizeHint(hint);
            ui->listWidgetStartup->setItemWidget(item, app);
            group.appItems.append(item);
        }
    };

    addGroup(tr("User Agents"), userAgents);
    addGroup(tr("System Agents"), systemAgents);
    addGroup(tr("System Daemons"), systemDaemons);

    setAppCount();
}

void StartupAppsPage::setAppCount()
{
    int count = 0;
    for (const SectionGroup &g : mSectionGroups)
        count += g.appItems.size();

    ui->lblStartupAppsTitle->setText(
        tr("Startup Applications (%1)")
        .arg(QString::number(count)));

    ui->notFoundWidget->setVisible(!count);
    ui->listWidgetStartup->setVisible(count);
}

void StartupAppsPage::filterStartupApps(const QString &text)
{
    for (const SectionGroup &group : mSectionGroups) {
        bool anyVisible = false;

        for (QListWidgetItem *item : group.appItems) {
            StartupApp *app = qobject_cast<StartupApp*>(ui->listWidgetStartup->itemWidget(item));
            bool matches = text.isEmpty() || (app && app->getAppName().contains(text, Qt::CaseInsensitive));
            item->setHidden(!matches);
            if (matches)
                anyVisible = true;
        }

        if (group.headerItem)
            group.headerItem->setHidden(!anyVisible);
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
