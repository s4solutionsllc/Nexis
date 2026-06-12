#include "updates_page.h"
#include "ui_updatespage.h"

#include <QHeaderView>
#include <QMovie>
#include <QMessageBox>
#include <QTreeWidgetItem>

#include "Managers/app_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"
#include "utilities.h"
#include "dpi.h"

UpdatesPage::UpdatesPage(QWidget *parent,
                         DataRefreshService *dataRefreshService,
                         AppManager *appManager,
                         SignalMapper *signalMapper)
    : QWidget(parent)
    , ui(new Ui::UpdatesPage)
    , mDrs(dataRefreshService ? dataRefreshService : DataRefreshService::ins())
    , mAppManager(appManager ? appManager : AppManager::ins())
    , mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
{
    ui->setupUi(this);
    init();
}

UpdatesPage::~UpdatesPage()
{
    delete ui;
}

void UpdatesPage::init()
{
    ui->treeUpdates->header()->setStretchLastSection(true);
    ui->treeUpdates->header()->setFixedHeight(Dpi::scale(30));
    ui->treeUpdates->setColumnWidth(0, Dpi::scale(90));
    ui->treeUpdates->setColumnWidth(1, Dpi::scale(300));

    QString iconLoading = QString(":/static/themes/%1/img/loading.gif")
                              .arg(mAppManager->resolveThemeName());
    QMovie *loadingMovie = new QMovie(iconLoading, QByteArray(), this);
    ui->lblLoading->setMovie(loadingMovie);
    loadingMovie->start();
    ui->lblLoading->hide();

    QList<QWidget *> widgets = { ui->btnCheckNow, ui->btnUpgradeSelected, ui->btnUpgradeAll, ui->txtSearch };
    Utilities::addDropShadow(widgets, 40);

    connect(mDrs, &DataRefreshService::systemUpdatesChecked,
            this, &UpdatesPage::onUpdatesChecked);
    connect(mDrs, &DataRefreshService::upgradeStarted,
            this, &UpdatesPage::onUpgradeStarted);
    connect(mDrs, &DataRefreshService::upgradeFinished,
            this, &UpdatesPage::onUpgradeFinished);

    // Backfill if DataRefreshService already has a result (lazy-constructed page).
    if (mDrs->hasLastUpdateCheckResult())
        onUpdatesChecked(mDrs->lastUpdateCheckResult());
    else
        setLoading(true);
}

void UpdatesPage::onUpdatesChecked(const UpdateCheckResult &result)
{
    mLastResult = result;
    mHasResult = true;
    setLoading(false);
    populate(result);
}

void UpdatesPage::populate(const UpdateCheckResult &result)
{
    const QString filter = ui->txtSearch->text().trimmed().toLower();

    ui->treeUpdates->clear();
    ui->btnUpgradeAll->setEnabled(!result.entries.isEmpty() && !mDrs->isUpgradeRunning());

    if (!result.success) {
        ui->lblStatus->setText(tr("Update check failed: %1").arg(result.errorMessage));
        return;
    }

    if (result.entries.isEmpty()) {
        ui->lblStatus->setText(tr("Everything is up to date."));
        return;
    }

    for (const UpdateEntry &e : result.entries) {
        if (!filter.isEmpty()) {
            if (!e.name.toLower().contains(filter) && !e.source.toLower().contains(filter))
                continue;
        }
        auto *item = new QTreeWidgetItem(ui->treeUpdates);
        item->setText(0, e.source);
        item->setText(1, e.name);
        item->setText(2, e.version);
        item->setData(0, Qt::UserRole, e.source);
        item->setData(1, Qt::UserRole, e.name);
    }

    int shown = ui->treeUpdates->topLevelItemCount();
    ui->lblStatus->setText(
        shown == result.totalCount
            ? tr("%n update(s) available", nullptr, result.totalCount)
            : tr("%1 of %2 update(s) shown").arg(shown).arg(result.totalCount));
}

void UpdatesPage::setLoading(bool loading)
{
    ui->lblLoading->setVisible(loading);
    ui->treeUpdates->setVisible(!loading);
    if (loading)
        ui->lblStatus->setText(tr("Checking for updates…"));
}

void UpdatesPage::setUpgrading(bool upgrading, const QString &label)
{
    ui->btnCheckNow->setEnabled(!upgrading);
    ui->btnUpgradeAll->setEnabled(!upgrading && mHasResult && !mLastResult.entries.isEmpty());
    ui->btnUpgradeSelected->setEnabled(!upgrading && !ui->treeUpdates->selectedItems().isEmpty());
    if (upgrading)
        ui->lblStatus->setText(tr("Upgrading: %1…").arg(label));
}

void UpdatesPage::onUpgradeStarted(const QString &label)
{
    setUpgrading(true, label);
}

void UpdatesPage::onUpgradeFinished(const QString &label, bool ok, const QString &error)
{
    setUpgrading(false);
    if (!ok) {
        QMessageBox::warning(this, tr("Upgrade Failed"),
                             tr("Failed to upgrade %1:\n%2").arg(label, error));
    }
    // DataRefreshService re-triggers an update check after upgradeFinished,
    // so no explicit triggerUpdateCheck() needed here.
}

void UpdatesPage::on_btnCheckNow_clicked()
{
    setLoading(true);
    mDrs->triggerUpdateCheck();
}

void UpdatesPage::on_btnUpgradeSelected_clicked()
{
    const auto selected = ui->treeUpdates->selectedItems();
    if (selected.isEmpty())
        return;

    if (selected.size() == 1) {
        UpdateEntry entry;
        entry.source = selected.first()->data(0, Qt::UserRole).toString();
        entry.name   = selected.first()->data(1, Qt::UserRole).toString();
        mDrs->runUpgrade(entry);
    } else {
        // Group by source and run one "upgrade all" per source present in selection.
        QSet<QString> sources;
        for (auto *item : selected)
            sources.insert(item->data(0, Qt::UserRole).toString());
        for (const QString &src : sources)
            mDrs->runUpgradeAll(src);
    }
}

void UpdatesPage::on_btnUpgradeAll_clicked()
{
    if (!mHasResult)
        return;

    QSet<QString> sources;
    for (const UpdateEntry &e : mLastResult.entries)
        sources.insert(e.source);
    for (const QString &src : sources)
        mDrs->runUpgradeAll(src);
}

void UpdatesPage::on_treeUpdates_itemSelectionChanged()
{
    ui->btnUpgradeSelected->setEnabled(!ui->treeUpdates->selectedItems().isEmpty()
                                       && !mDrs->isUpgradeRunning());
}

void UpdatesPage::on_txtSearch_textChanged(const QString &)
{
    if (mHasResult)
        populate(mLastResult);
}
