#include "startup_apps_page.h"
#include "ui_startup_apps_page.h"
#include "utilities.h"
#include "Services/startup_service.h"

#ifdef Q_OS_MACOS
#include "btm_reset_dialog.h"
#include "btm_row.h"
#include <QFileIconProvider>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
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

#ifdef Q_OS_MACOS
    // FW-10: add a "Repair BTM…" button alongside Add Startup App. The
    // parent layout of btnAddStartupApp is the page's grid; we insert the
    // new button into the same column right next to it.
    mBtnRepairBtm = new QPushButton(tr("Repair BTM…"), this);
    mBtnRepairBtm->setObjectName(QStringLiteral("btnRepairBtm"));
    mBtnRepairBtm->setAccessibleName(QStringLiteral("destructive"));
    mBtnRepairBtm->setCursor(Qt::PointingHandCursor);
    mBtnRepairBtm->setFocusPolicy(Qt::NoFocus);
    mBtnRepairBtm->setToolTip(tr(
        "Run sudo sfltool resetbtm. Wipes the Background Task Management "
        "database; every Login Item re-prompts on next login."));
    connect(mBtnRepairBtm, &QPushButton::clicked,
            this, &StartupAppsPage::onRepairBtmClicked);

    // The button row is row 0 of gridLayout_2. Add the new button to the
    // far right (column 4); extend the row 1 list's colspan so it stays
    // aligned with the wider header.
    if (auto *grid = qobject_cast<QGridLayout *>(ui->widgetStartupApps->layout())) {
        grid->addWidget(mBtnRepairBtm, 0, 4);
        const int vIdx = grid->indexOf(ui->verticalWidget);
        if (vIdx >= 0) {
            int r = 0, c = 0, rs = 0, cs = 0;
            grid->getItemPosition(vIdx, &r, &c, &rs, &cs);
            grid->removeWidget(ui->verticalWidget);
            grid->addWidget(ui->verticalWidget, r, c, rs, 5);
        }
    }
    Utilities::addDropShadow(mBtnRepairBtm, 60);
#endif
}

void StartupAppsPage::addSectionHeader(const QString &title, bool isBtmGroup)
{
    auto *label = new QLabel(title, this);
    label->setObjectName(QStringLiteral("startupSectionHeader"));

    auto *item = new QListWidgetItem(ui->listWidgetStartup);
    item->setFlags(Qt::NoItemFlags);
    item->setSizeHint(label->sizeHint());
    ui->listWidgetStartup->setItemWidget(item, label);

    SectionGroup group;
    group.headerItem = item;
    group.isBtmGroup = isBtmGroup;
    mSectionGroups.append(group);
}

void StartupAppsPage::loadApps()
{
    ui->txtSearchStartup->clear();
    ui->listWidgetStartup->clear();
    mSectionGroups.clear();
#ifdef Q_OS_MACOS
    mBtmRowCount = 0;
#endif

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

#ifdef Q_OS_MACOS
    // FW-10: append the BTM section after the launchd-derived sections so
    // users can spot orphan / duplicate badges in one scroll.
    QString btmError;
    const QList<BtmRecord> btmRecords = mStartupService->getBtmRecords(&btmError);

    if (!btmRecords.isEmpty()) {
        addSectionHeader(tr("BTM Records (%1)").arg(btmRecords.size()), true);
        SectionGroup &group = mSectionGroups.last();

        for (const BtmRecord &record : btmRecords) {
            auto *item = new QListWidgetItem(ui->listWidgetStartup);
            auto *row = new BtmRow(record, this);

            QSize hint = row->sizeHint();
            hint.setHeight(qMax(hint.height(), row->minimumHeight()));
            item->setSizeHint(hint);
            ui->listWidgetStartup->setItemWidget(item, row);
            group.appItems.append(item);
        }
        mBtmRowCount = btmRecords.size();
    } else if (!btmError.isEmpty()) {
        addSectionHeader(tr("BTM Records — %1").arg(btmError), true);
    }
#endif

    setAppCount();
}

void StartupAppsPage::setAppCount()
{
    int count = 0;
    for (const SectionGroup &g : mSectionGroups) {
        if (g.isBtmGroup)
            continue;
        count += g.appItems.size();
    }

    ui->lblStartupAppsTitle->setText(
        tr("Startup Applications (%1)")
        .arg(QString::number(count)));

    int totalRows = count;
#ifdef Q_OS_MACOS
    totalRows += mBtmRowCount;
#endif
    ui->notFoundWidget->setVisible(!totalRows);
    ui->listWidgetStartup->setVisible(totalRows);
}

void StartupAppsPage::filterStartupApps(const QString &text)
{
    for (const SectionGroup &group : mSectionGroups) {
        bool anyVisible = false;

        for (QListWidgetItem *item : group.appItems) {
            QWidget *w = ui->listWidgetStartup->itemWidget(item);
            bool matches = text.isEmpty();
            if (!matches && w) {
                if (auto *app = qobject_cast<StartupApp*>(w))
                    matches = app->getAppName().contains(text, Qt::CaseInsensitive);
#ifdef Q_OS_MACOS
                else if (auto *btm = qobject_cast<BtmRow*>(w))
                    matches = btm->matches(text);
#endif
            }
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

#ifdef Q_OS_MACOS
void StartupAppsPage::onRepairBtmClicked()
{
    BtmResetDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString error;
    if (mStartupService->resetBtm(&error)) {
        QMessageBox::information(this, tr("BTM Repair Complete"),
            tr("sfltool resetbtm completed. Background items will re-prompt on "
               "next login."));
        loadApps();
    } else {
        QMessageBox::warning(this, tr("BTM Repair Failed"),
            error.isEmpty()
                ? tr("sfltool resetbtm failed for an unknown reason.")
                : error);
    }
}
#endif
