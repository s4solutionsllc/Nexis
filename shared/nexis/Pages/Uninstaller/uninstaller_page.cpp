#include "uninstaller_page.h"
#include "ui_uninstallerpage.h"
#include "crumbs_review_dialog.h"
#include <QHeaderView>
#include <QMovie>
#include <QMessageBox>
#include <QMap>
#include "utilities.h"
#include "dpi.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "Services/package_service.h"
#include <Utils/command_util.h>
#include <Utils/format_util.h>

UninstallerPage::~UninstallerPage()
{
    delete ui;
}

UninstallerPage::UninstallerPage(QWidget *parent, PackageService *packageService,
                                 AppManager *appManager, SignalMapper *signalMapper) :
    QWidget(parent),
    ui(new Ui::UninstallerPage),
    mPackageService(packageService ? packageService : PackageService::ins()),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
{
    ui->setupUi(this);

    init();
}

void UninstallerPage::init()
{
    ui->treeWidgetPackages->header()->setFixedHeight(Dpi::scale(30));
    ui->treeWidgetPackages->setHeaderLabels({ tr("Application") });
    ui->treeWidgetPackages->header()->setStretchLastSection(true);

    QString iconLoading = QString(":/static/themes/%1/img/loading.gif").arg(mAppManager->resolveThemeName());
    QMovie *loadingMovie = new QMovie(iconLoading, QByteArray(), this);
    ui->lblLoadingUninstaller->setMovie(loadingMovie);
    loadingMovie->start();
    ui->lblLoadingUninstaller->hide();

    ui->stackedWidget->setCurrentIndex(0);

#ifdef Q_OS_MACOS
    ui->chkPurge->hide();
    ui->btnSnapPackages->hide();
#endif

    QList<QWidget*> widgets = { ui->txtPackageSearch, ui->btnUninstall, ui->btnSystemPackages,
                                ui->btnSnapPackages, ui->btnOrphanPackages };
    Utilities::addDropShadow(widgets, 40);

    connect(mPackageService, &PackageService::packagesFetched,
            this, &UninstallerPage::onPackagesLoaded);
    connect(mPackageService, &PackageService::snapPackagesFetched,
            this, &UninstallerPage::onSnapPackagesLoaded);
    connect(mPackageService, &PackageService::orphanPackagesFetched,
            this, &UninstallerPage::onOrphanPackagesLoaded);
    connect(ui->treeWidgetPackages, &QTreeWidget::itemChanged, this, &UninstallerPage::onTreeItemChanged);

    mPackageService->fetchPackages();
    mPackageService->fetchSnapPackages();
    mPackageService->fetchOrphanPackages();

    connect(mSignalMapper, &SignalMapper::sigUninstallStarted, this, &UninstallerPage::uninstallStarted);
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        mPackageService->fetchPackages();
    });
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        mPackageService->fetchSnapPackages();
    });
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        mPackageService->fetchOrphanPackages();
    });

#ifdef Q_OS_MAC
    // FR-123: after a macOS uninstall, scan for residual files in ~/Library
    // that match the uninstalled apps' bundle identifiers and offer review.
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        if (mPendingCrumbBundleIds.isEmpty())
            return;
        const QStringList ids = mPendingCrumbBundleIds;
        mPendingCrumbBundleIds.clear();
        auto *dlg = new CrumbsReviewDialog(ids, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->open();
    });
#endif
}

void UninstallerPage::onPackagesLoaded(QList<Package> packages)
{
    emit uninstallStarted();

    ui->treeWidgetPackages->clear();
    ui->treeWidgetPackages->blockSignals(true);

    QMap<QString, QList<Package>> grouped;
    for (const Package &pkg : packages) {
        QString section = pkg.section.isEmpty() ? "other" : pkg.section;
        section = section.section('/', -1);
        grouped[section].append(pkg);
    }

    QIcon fallbackIcon(":/static/themes/common/img/package.png");

    QStringList sections = grouped.keys();
    sections.sort();

    for (const QString &section : sections) {
        const QList<Package> &pkgs = grouped[section];
        QString friendlyName = PackageTool::friendlySectionName(section);

        QTreeWidgetItem *sectionItem = new QTreeWidgetItem(ui->treeWidgetPackages);
        sectionItem->setText(0, QString("%1 (%2)").arg(friendlyName).arg(pkgs.size()));
        sectionItem->setFlags(Qt::ItemIsEnabled);

        QFont sectionFont = sectionItem->font(0);
        sectionFont.setBold(true);
        sectionItem->setFont(0, sectionFont);

        for (const Package &pkg : pkgs) {
            QTreeWidgetItem *item = new QTreeWidgetItem(sectionItem);
#ifdef Q_OS_MAC
            QString displayText = pkg.description.isEmpty()
                ? pkg.name
                : QString("%1 %2").arg(pkg.name, pkg.description);
            item->setData(0, Qt::UserRole + 1, pkg.path);
            item->setData(0, Qt::UserRole + 2, pkg.bundleId);   // FR-123
#else
            QString displayText = pkg.description.isEmpty()
                ? pkg.name
                : QString("%1 (%2)").arg(pkg.description, pkg.name);
#endif
            item->setText(0, displayText);
            item->setIcon(0, fallbackIcon);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, pkg.name);
        }
    }

    ui->treeWidgetPackages->blockSignals(false);
    setAppCount();

    ui->treeWidgetPackages->setEnabled(true);
    ui->txtPackageSearch->setEnabled(true);
    ui->txtPackageSearch->clear();

    ui->lblLoadingUninstaller->hide();
}

void UninstallerPage::onSnapPackagesLoaded(QStringList packages)
{
    ui->listWidgetSnapPackages->clear();

    QIcon icon(":/static/themes/common/img/package.png");
    for (const QString &package : packages) {
        QListWidgetItem *item = new QListWidgetItem(icon, QString("  %1").arg(package));
        item->setCheckState(Qt::Unchecked);
        ui->listWidgetSnapPackages->addItem(item);
    }
    setAppCount();

    ui->listWidgetSnapPackages->setEnabled(true);
    ui->txtPackageSearch->setEnabled(true);
    ui->txtPackageSearch->clear();

    ui->lblLoadingUninstaller->hide();
}

void UninstallerPage::onOrphanPackagesLoaded(QList<OrphanPackage> packages)
{
    ui->listWidgetOrphanPackages->clear();

    QIcon icon(":/static/themes/common/img/package.png");
    for (const OrphanPackage &pkg : packages) {
        QString displayText = pkg.description.isEmpty()
            ? QString("  %1").arg(pkg.name)
            : QString("  %1 — %2").arg(pkg.name, pkg.description);
        if (pkg.size > 0)
            displayText += QString(" (%1)").arg(FormatUtil::formatBytes(pkg.size));

        QListWidgetItem *item = new QListWidgetItem(icon, displayText);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, pkg.name);
        ui->listWidgetOrphanPackages->addItem(item);
    }
    setAppCount();

    ui->listWidgetOrphanPackages->setEnabled(true);
    ui->txtPackageSearch->setEnabled(true);

    ui->lblLoadingUninstaller->hide();
}

void UninstallerPage::setAppCount()
{
    int count = 0;
    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i)
        count += ui->treeWidgetPackages->topLevelItem(i)->childCount();

#ifdef Q_OS_MAC
    ui->btnSystemPackages->setText(tr("Applications (%1)").arg(count));
#else
    ui->btnSystemPackages->setText(tr("Packages (%1)").arg(count));
#endif
    ui->notFoundWidget->setVisible(! count);
    ui->treeWidgetPackages->setVisible(count);

    int snapCount = ui->listWidgetSnapPackages->count();
    ui->btnSnapPackages->setText(tr("Snap Packages (%1)").arg(snapCount));
    ui->notFoundWidget_2->setVisible(! snapCount);
    ui->listWidgetSnapPackages->setVisible(snapCount);

#ifndef Q_OS_MAC
    ui->btnSnapPackages->setVisible(CommandUtil::isExecutable("snap"));
#endif

    int orphanCount = ui->listWidgetOrphanPackages->count();
    ui->btnOrphanPackages->setText(tr("Orphan Packages (%1)").arg(orphanCount));
    ui->notFoundWidget_3->setVisible(! orphanCount);
    ui->listWidgetOrphanPackages->setVisible(orphanCount);

    ui->btnUninstall->setVisible(count || snapCount || orphanCount);
}

QStringList UninstallerPage::getSelectedPackages()
{
    QStringList selectedPackages;

    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetPackages->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                selectedPackages << item->data(0, Qt::UserRole).toString();
        }
    }

    return selectedPackages;
}

QStringList UninstallerPage::getSelectedSnapPackages()
{
    QStringList selectedPackages = {};

    for (int i = 0; i < ui->listWidgetSnapPackages->count(); ++i)
    {
        QListWidgetItem *item = ui->listWidgetSnapPackages->item(i);

        if(item->checkState() == Qt::Checked)
            selectedPackages << item->text().trimmed();
    }

    return selectedPackages;
}

#ifdef Q_OS_MAC
QStringList UninstallerPage::getSelectedAppPaths()
{
    QStringList paths;
    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetPackages->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                paths << item->data(0, Qt::UserRole + 1).toString();
        }
    }
    return paths;
}

QStringList UninstallerPage::getSelectedAppBundleIds()
{
    QStringList ids;
    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetPackages->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) != Qt::Checked)
                continue;
            const QString bid = item->data(0, Qt::UserRole + 2).toString();
            if (!bid.isEmpty())
                ids << bid;
        }
    }
    return ids;
}
#endif

void UninstallerPage::on_btnUninstall_clicked()
{
    // Orphan packages tab — all-or-nothing autoremove
    if (ui->stackedWidget->currentIndex() == 2) {
        int orphanCount = ui->listWidgetOrphanPackages->count();
        if (orphanCount == 0)
            return;

        QStringList names;
        for (int i = 0; i < ui->listWidgetOrphanPackages->count(); ++i)
            names << ui->listWidgetOrphanPackages->item(i)->data(Qt::UserRole).toString();

        QString message = tr("The following orphan packages will be removed via autoremove:\n\n");
        message += names.join(", ");

        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            tr("Confirm Remove Orphan Packages"),
            message,
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel);

        if (reply != QMessageBox::Ok)
            return;

        mPackageService->removeOrphanPackages();
        return;
    }

#ifdef Q_OS_MAC
    QStringList selectedNames = getSelectedPackages();
    QStringList selectedPaths = getSelectedAppPaths();

    if (selectedPaths.isEmpty())
        return;

    QString message = tr("The following applications will be moved to Trash:\n\n");
    message += selectedNames.join("\n");

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Move to Trash"),
        message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (reply != QMessageBox::Ok)
        return;

    // FR-123: capture bundle ids now — brew cask uninstalls would remove the
    // .app before we could read its Info.plist, and the review dialog needs
    // them to scan residual files once the uninstall finishes.
    mPendingCrumbBundleIds = getSelectedAppBundleIds();

    mPackageService->trashApps(selectedPaths);
#else
    QStringList selectedPackages = getSelectedPackages();
    QStringList selectedSnapPackages = getSelectedSnapPackages();

    if (selectedPackages.isEmpty() && selectedSnapPackages.isEmpty())
        return;

    QStringList allWouldRemove;
    if (!selectedPackages.isEmpty())
        allWouldRemove = mPackageService->dryRunRemovePackages(selectedPackages);

    QStringList additionalPackages;
    for (const QString &pkg : allWouldRemove) {
        if (!selectedPackages.contains(pkg))
            additionalPackages << pkg;
    }

    QString message = tr("The following packages will be removed:\n\n");
    message += selectedPackages.join(", ");
    if (!selectedSnapPackages.isEmpty()) {
        message += "\n\n" + tr("Snap packages:\n");
        message += selectedSnapPackages.join(", ");
    }
    if (!additionalPackages.isEmpty()) {
        message += "\n\n" + tr("The following additional packages will also be removed:\n\n");
        message += additionalPackages.join(", ");
    }

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Uninstall"),
        message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (reply != QMessageBox::Ok)
        return;

    bool purge = ui->chkPurge->isChecked();

    mPackageService->uninstallPackages(selectedPackages, purge);
    mPackageService->uninstallSnapPackages(selectedSnapPackages);
#endif
}

void UninstallerPage::uninstallStarted()
{
    ui->treeWidgetPackages->setEnabled(false);
    ui->listWidgetSnapPackages->setEnabled(false);
    ui->listWidgetOrphanPackages->setEnabled(false);
    ui->txtPackageSearch->setEnabled(false);
    ui->btnUninstall->hide();
    ui->lblLoadingUninstaller->show();
}

void UninstallerPage::on_txtPackageSearch_textChanged(const QString &val)
{
    if (ui->stackedWidget->currentIndex() == 0) {
        for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i) {
            QTreeWidgetItem *section = ui->treeWidgetPackages->topLevelItem(i);
            int visibleChildren = 0;
            for (int j = 0; j < section->childCount(); ++j) {
                QTreeWidgetItem *item = section->child(j);
                bool matches = val.isEmpty()
                    || item->text(0).contains(val, Qt::CaseInsensitive)
                    || item->data(0, Qt::UserRole).toString().contains(val, Qt::CaseInsensitive);
                item->setHidden(!matches);
                if (matches)
                    visibleChildren++;
            }
            section->setHidden(visibleChildren == 0);
            if (visibleChildren > 0 && !val.isEmpty())
                section->setExpanded(true);
        }
    } else if (ui->stackedWidget->currentIndex() == 1) {
        QList<QListWidgetItem*> matches = ui->listWidgetSnapPackages->findItems(val, Qt::MatchFlag::MatchContains);
        for (int i = 0; i < ui->listWidgetSnapPackages->count(); ++i)
            ui->listWidgetSnapPackages->item(i)->setHidden(true);
        for (QListWidgetItem* item : matches)
            item->setHidden(false);
    } else if (ui->stackedWidget->currentIndex() == 2) {
        QList<QListWidgetItem*> matches = ui->listWidgetOrphanPackages->findItems(val, Qt::MatchFlag::MatchContains);
        for (int i = 0; i < ui->listWidgetOrphanPackages->count(); ++i)
            ui->listWidgetOrphanPackages->item(i)->setHidden(true);
        for (QListWidgetItem* item : matches)
            item->setHidden(false);
    }
}

void UninstallerPage::on_btnSystemPackages_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void UninstallerPage::on_btnSnapPackages_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void UninstallerPage::on_btnOrphanPackages_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
}

void UninstallerPage::on_listWidgetSnapPackages_itemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)")
                              .arg(getSelectedSnapPackages().count() + getSelectedPackages().count()));
}

void UninstallerPage::on_listWidgetOrphanPackages_itemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    int count = 0;
    for (int i = 0; i < ui->listWidgetOrphanPackages->count(); ++i)
        if (ui->listWidgetOrphanPackages->item(i)->checkState() == Qt::Checked)
            ++count;
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)").arg(count));
}

void UninstallerPage::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)")
                              .arg(getSelectedSnapPackages().count() + getSelectedPackages().count()));
}
