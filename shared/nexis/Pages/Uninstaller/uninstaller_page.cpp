#include "uninstaller_page.h"
#include "ui_uninstallerpage.h"
#ifdef Q_OS_MAC
#include "crumbs_review_dialog.h"
#include "running_app_warning_dialog.h"
#include <Tools/running_app_gate.h>
#endif
#ifdef Q_OS_LINUX
#include "leftover_review_hook.h"
#endif
#include <QHeaderView>
#include <QMovie>
#include <QMessageBox>
#include <QMap>
#include <QSettings>
#include <QTableWidgetItem>
#include "utilities.h"
#include "dpi.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "Services/package_service.h"
#include <Utils/command_util.h>
#include <Utils/format_util.h>

// Safety levels stored in Qt::UserRole + 1 on the Reverse Deps cell
// so colors can be re-applied after a theme change without re-fetching.
enum class OrphanSafety { Safe = 0, Manual = 1, Dangerous = 2, Unknown = 3 };

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

    // DS §2/§7: single elevated container around the application list;
    // rows inside stay flat (no per-row shadow).
    ui->uninstallerContainer->setAttribute(Qt::WA_StyledBackground, true);
    ui->uninstallerContainer->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(ui->uninstallerContainer, 90, 26);

#ifdef Q_OS_MACOS
    ui->chkPurge->hide();
    ui->btnSnapPackages->hide();
    ui->btnFlatpakPackages->hide();
#endif

    QList<QWidget*> widgets = { ui->txtPackageSearch, ui->btnUninstall, ui->btnSystemPackages,
                                ui->btnSnapPackages, ui->btnFlatpakPackages, ui->btnOrphanPackages,
                                ui->btnAptHistory };
    Utilities::addDropShadow(widgets, 40);

    connect(mPackageService, &PackageService::packagesFetched,
            this, &UninstallerPage::onPackagesLoaded);
    connect(mPackageService, &PackageService::snapPackagesFetched,
            this, &UninstallerPage::onSnapPackagesLoaded);
    connect(mPackageService, &PackageService::flatpakPackagesFetched,
            this, &UninstallerPage::onFlatpakPackagesLoaded);
    connect(mPackageService, &PackageService::orphanPackagesFetched,
            this, &UninstallerPage::onOrphanPackagesLoaded);
    connect(ui->treeWidgetPackages, &QTreeWidget::itemChanged, this, &UninstallerPage::onTreeItemChanged);

    mPackageService->fetchPackages();
    mPackageService->fetchSnapPackages();
    mPackageService->fetchFlatpakPackages();
    mPackageService->fetchOrphanPackages();

    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &UninstallerPage::refreshOrphanThemeColors);
    connect(mSignalMapper, &SignalMapper::sigUninstallStarted, this, &UninstallerPage::uninstallStarted);
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        mPackageService->fetchPackages();
    });
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        mPackageService->fetchSnapPackages();
    });
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        mPackageService->fetchFlatpakPackages();
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
#ifdef Q_OS_LINUX
    // SSO-15385: after a Linux package uninstall, scan XDG user dirs for
    // leftover files matching the removed packages and offer move-to-trash review.
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        if (mPendingUninstallPackageNames.isEmpty())
            return;
        const QStringList names = mPendingUninstallPackageNames;
        mPendingUninstallPackageNames.clear();
        LeftoverReviewHook::maybeShowReviewDialog(names, this);
    });
#endif

#ifndef Q_OS_MACOS
    // FW-07 (SSO-3735): APT 3.1 transaction history.
    // The nav button is hidden until we confirm the local apt understands
    // history-list. The feature degrades to "invisible" rather than to a
    // "not supported" empty panel so older Debian/Ubuntu users don't see a
    // grey tab they can't use.
    const bool aptHistory = mPackageService->isAptHistorySupported();
    ui->btnAptHistory->setVisible(aptHistory);
    if (aptHistory) {
        QTableWidget *htbl = ui->tableWidgetAptHistory;
        htbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        htbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        htbl->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        htbl->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        htbl->verticalHeader()->setDefaultSectionSize(Dpi::scale(26));
        ui->notFoundWidget_4->hide();

        connect(mPackageService, &PackageService::aptHistoryFetched,
                this, &UninstallerPage::onAptHistoryFetched);
        connect(mPackageService, &PackageService::aptWhyFetched,
                this, &UninstallerPage::onAptWhyFetched);
        connect(htbl, &QTableWidget::itemSelectionChanged,
                this, &UninstallerPage::onAptHistorySelectionChanged);

        // Reload the history whenever an install/uninstall completes so the
        // panel reflects the new HEAD transaction.
        connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
            mPackageService->fetchAptHistory();
        });
        mPackageService->fetchAptHistory();
    } else {
        ui->notFoundWidget_4->show();
        ui->tableWidgetAptHistory->setEnabled(false);
        ui->grpAptWhy->setEnabled(false);
        ui->btnAptHistoryUndoLast->setEnabled(false);
    }
#else
    ui->btnAptHistory->hide();
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
            item->setData(0, Qt::UserRole + 3, static_cast<qlonglong>(pkg.size));   // SSO-15384
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
    // SSO-15384: start with button disabled — no items checked yet.
    ui->btnUninstall->setEnabled(false);

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

void UninstallerPage::onFlatpakPackagesLoaded(QStringList packages)
{
    ui->listWidgetFlatpakPackages->clear();

    QIcon icon(":/static/themes/common/img/package.png");
    for (const QString &package : packages) {
        QListWidgetItem *item = new QListWidgetItem(icon, QString("  %1").arg(package));
        item->setCheckState(Qt::Unchecked);
        ui->listWidgetFlatpakPackages->addItem(item);
    }
    setAppCount();

    ui->listWidgetFlatpakPackages->setEnabled(true);
    ui->txtPackageSearch->setEnabled(true);
    ui->txtPackageSearch->clear();

    ui->lblLoadingUninstaller->hide();
}

void UninstallerPage::onOrphanPackagesLoaded(QList<OrphanPackage> packages)
{
    QTableWidget *tbl = ui->tableWidgetOrphanPackages;
    tbl->setSortingEnabled(false);
    tbl->setRowCount(0);

    tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tbl->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tbl->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tbl->verticalHeader()->setDefaultSectionSize(Dpi::scale(28));

    QSettings *sv = mAppManager->getStyleValues();
    QString successColor  = sv ? sv->value("@successColor").toString()  : "#46A758";
    QString warningColor  = sv ? sv->value("@warningColor").toString()  : "#FFB347";
    QString dangerColor   = sv ? sv->value("@destructiveColor").toString() : "#E05454";
    QString mutedColor    = sv ? sv->value("@tertiaryText").toString()  : "#888888";

    for (const OrphanPackage &pkg : packages) {
        int row = tbl->rowCount();
        tbl->insertRow(row);

        // Col 0 — Package name + description as tooltip
        auto *nameItem = new QTableWidgetItem(pkg.name);
        nameItem->setData(Qt::UserRole, pkg.name);
        if (!pkg.description.isEmpty())
            nameItem->setToolTip(pkg.description);
        tbl->setItem(row, 0, nameItem);

        // Col 1 — Size
        QString sizeStr = pkg.size > 0 ? FormatUtil::formatBytes(pkg.size) : QString("—");
        auto *sizeItem = new QTableWidgetItem(sizeStr);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tbl->setItem(row, 1, sizeItem);

        // Col 2 — Install type
        QString installStr = (pkg.reverseDepsCount == -1)
            ? QString("—")
            : (pkg.autoInstalled ? tr("Auto") : tr("Manual"));
        auto *installItem = new QTableWidgetItem(installStr);
        installItem->setTextAlignment(Qt::AlignCenter);
        if (!pkg.autoInstalled && pkg.reverseDepsCount != -1) {
            QFont f = installItem->font();
            f.setBold(true);
            installItem->setFont(f);
        }
        tbl->setItem(row, 2, installItem);

        // Col 3 — Reverse deps count + color
        OrphanSafety safety;
        QString rdStr;
        if (pkg.reverseDepsCount == -1) {
            safety = OrphanSafety::Unknown;
            rdStr = QString("—");
        } else if (pkg.reverseDepsCount > 0) {
            safety = OrphanSafety::Dangerous;
            rdStr = tr("%n dependent(s)", "", pkg.reverseDepsCount);
        } else if (!pkg.autoInstalled) {
            safety = OrphanSafety::Manual;
            rdStr = tr("None");
        } else {
            safety = OrphanSafety::Safe;
            rdStr = tr("None");
        }
        auto *rdItem = new QTableWidgetItem(rdStr);
        rdItem->setTextAlignment(Qt::AlignCenter);
        rdItem->setData(Qt::UserRole + 1, static_cast<int>(safety));
        switch (safety) {
        case OrphanSafety::Safe:      rdItem->setForeground(QColor(successColor)); break;
        case OrphanSafety::Manual:    rdItem->setForeground(QColor(warningColor)); break;
        case OrphanSafety::Dangerous: rdItem->setForeground(QColor(dangerColor));  break;
        default:        rdItem->setForeground(QColor(mutedColor));   break;
        }
        tbl->setItem(row, 3, rdItem);
    }

    tbl->setSortingEnabled(true);
    setAppCount();

    tbl->setEnabled(true);
    ui->txtPackageSearch->setEnabled(true);
    ui->lblLoadingUninstaller->hide();
}

void UninstallerPage::refreshOrphanThemeColors()
{
    QTableWidget *tbl = ui->tableWidgetOrphanPackages;
    QSettings *sv = mAppManager->getStyleValues();
    if (!sv) return;

    QString successColor = sv->value("@successColor").toString();
    QString warningColor = sv->value("@warningColor").toString();
    QString dangerColor  = sv->value("@destructiveColor").toString();
    QString mutedColor   = sv->value("@tertiaryText").toString();

    for (int row = 0; row < tbl->rowCount(); ++row) {
        QTableWidgetItem *rdItem = tbl->item(row, 3);
        if (!rdItem) continue;
        OrphanSafety safety = static_cast<OrphanSafety>(rdItem->data(Qt::UserRole + 1).toInt());
        switch (safety) {
        case OrphanSafety::Safe:      rdItem->setForeground(QColor(successColor)); break;
        case OrphanSafety::Manual:    rdItem->setForeground(QColor(warningColor)); break;
        case OrphanSafety::Dangerous: rdItem->setForeground(QColor(dangerColor));  break;
        default:        rdItem->setForeground(QColor(mutedColor));   break;
        }
    }
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

    int flatpakCount = ui->listWidgetFlatpakPackages->count();
    ui->btnFlatpakPackages->setText(tr("Flatpak Packages (%1)").arg(flatpakCount));
    ui->notFoundWidget_5->setVisible(! flatpakCount);
    ui->listWidgetFlatpakPackages->setVisible(flatpakCount);

#ifndef Q_OS_MAC
    ui->btnFlatpakPackages->setVisible(CommandUtil::isExecutable("flatpak"));
#endif

    int orphanCount = ui->tableWidgetOrphanPackages->rowCount();
    ui->btnOrphanPackages->setText(tr("Orphan Packages (%1)").arg(orphanCount));
    ui->notFoundWidget_3->setVisible(! orphanCount);
    ui->tableWidgetOrphanPackages->setVisible(orphanCount);

#ifndef Q_OS_MAC
    int historyCount = ui->tableWidgetAptHistory->rowCount();
    if (ui->btnAptHistory->isVisible())
        ui->btnAptHistory->setText(tr("APT History (%1)").arg(historyCount));
#endif

    // btnUninstall drives the System/Snap/Flatpak/Orphan tabs only; the APT
    // History tab has its own action bar (Undo Last / Undo Selected /
    // Rollback) and doesn't use the primary uninstall button.
    ui->btnUninstall->setVisible(count || snapCount || flatpakCount || orphanCount);
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

QStringList UninstallerPage::getSelectedFlatpakPackages()
{
    QStringList selectedPackages = {};

    for (int i = 0; i < ui->listWidgetFlatpakPackages->count(); ++i)
    {
        QListWidgetItem *item = ui->listWidgetFlatpakPackages->item(i);

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
    if (ui->stackedWidget->currentWidget() == ui->pageOrphanPackages) {
        QTableWidget *tbl = ui->tableWidgetOrphanPackages;
        int orphanCount = tbl->rowCount();
        if (orphanCount == 0)
            return;

        QStringList names;
        for (int i = 0; i < tbl->rowCount(); ++i) {
            QTableWidgetItem *item = tbl->item(i, 0);
            if (item) names << item->data(Qt::UserRole).toString();
        }

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

    // SSO-15384 / Design Anchor: confirmation dialog is one sentence max and
    // shows a "what will be deleted" size + item count summary.
    quint64 totalSize = 0;
    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetPackages->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                totalSize += static_cast<quint64>(item->data(0, Qt::UserRole + 3).toLongLong());
        }
    }
    const QString sizeStr = FormatUtil::formatBytes(totalSize);
    const QString message = tr("Move %1 application(s) (%2) to Trash?")
                                .arg(selectedPaths.count()).arg(sizeStr);

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Move to Trash"),
        message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (reply != QMessageBox::Ok)
        return;

    // SSO-15566 / CISO §4: block on any selected app that's currently running
    // before any deletion begins. Walk the checked items directly (rather
    // than the getSelected*() helpers, whose bundle-id list is filtered and
    // therefore not index-aligned with selectedPaths) so path/name/bundle id
    // stay one-to-one per app. Cancelling the warn/quit dialog for one item
    // just drops that item from the batch — the rest proceed.
    QMap<QString, QString> pathToName;
    QMap<QString, QString> pathToBundleId;
    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetPackages->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) != Qt::Checked)
                continue;
            const QString path = item->data(0, Qt::UserRole + 1).toString();
            pathToName[path] = item->data(0, Qt::UserRole).toString();
            pathToBundleId[path] = item->data(0, Qt::UserRole + 2).toString();
        }
    }

    const QStringList finalPaths = RunningAppGate::filterRunnable(
        selectedPaths,
        [this](const QString &path) { return mPackageService->isAppRunning(path); },
        [this, &pathToName](const QString &path) {
            RunningAppWarningDialog dlg(pathToName.value(path), path, mPackageService, this);
            return dlg.exec() == QDialog::Accepted;
        });

    if (finalPaths.isEmpty())
        return;

    // FR-123: capture bundle ids now — brew cask uninstalls would remove the
    // .app before we could read its Info.plist, and the review dialog needs
    // them to scan residual files once the uninstall finishes.
    QStringList finalBundleIds;
    for (const QString &path : finalPaths) {
        const QString bid = pathToBundleId.value(path);
        if (!bid.isEmpty())
            finalBundleIds << bid;
    }
    mPendingCrumbBundleIds = finalBundleIds;

    mPackageService->trashApps(finalPaths);
#else
    QStringList selectedPackages = getSelectedPackages();
    QStringList selectedSnapPackages = getSelectedSnapPackages();
    QStringList selectedFlatpakPackages = getSelectedFlatpakPackages();

    if (selectedPackages.isEmpty() && selectedSnapPackages.isEmpty() && selectedFlatpakPackages.isEmpty())
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
    if (!selectedFlatpakPackages.isEmpty()) {
        message += "\n\n" + tr("Flatpak packages:\n");
        message += selectedFlatpakPackages.join(", ");
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

#ifdef Q_OS_LINUX
    // SSO-15385: capture names before uninstall so the leftover-scan dialog
    // can search for residual files once the package removal finishes.
    mPendingUninstallPackageNames = selectedPackages + selectedSnapPackages + selectedFlatpakPackages;
#endif

    mPackageService->uninstallPackages(selectedPackages, purge);
    mPackageService->uninstallSnapPackages(selectedSnapPackages);
    mPackageService->uninstallFlatpakPackages(selectedFlatpakPackages);
#endif
}

void UninstallerPage::uninstallStarted()
{
    ui->treeWidgetPackages->setEnabled(false);
    ui->listWidgetSnapPackages->setEnabled(false);
    ui->listWidgetFlatpakPackages->setEnabled(false);
    ui->tableWidgetOrphanPackages->setEnabled(false);
    ui->txtPackageSearch->setEnabled(false);
    ui->btnUninstall->hide();
    ui->lblLoadingUninstaller->show();
}

void UninstallerPage::on_txtPackageSearch_textChanged(const QString &val)
{
    QWidget *current = ui->stackedWidget->currentWidget();
    if (current == ui->pageSystemPackages) {
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
    } else if (current == ui->pageSnapPackages) {
        QList<QListWidgetItem*> matches = ui->listWidgetSnapPackages->findItems(val, Qt::MatchFlag::MatchContains);
        for (int i = 0; i < ui->listWidgetSnapPackages->count(); ++i)
            ui->listWidgetSnapPackages->item(i)->setHidden(true);
        for (QListWidgetItem* item : matches)
            item->setHidden(false);
    } else if (current == ui->pageFlatpakPackages) {
        QList<QListWidgetItem*> matches = ui->listWidgetFlatpakPackages->findItems(val, Qt::MatchFlag::MatchContains);
        for (int i = 0; i < ui->listWidgetFlatpakPackages->count(); ++i)
            ui->listWidgetFlatpakPackages->item(i)->setHidden(true);
        for (QListWidgetItem* item : matches)
            item->setHidden(false);
    } else if (current == ui->pageOrphanPackages) {
        QTableWidget *tbl = ui->tableWidgetOrphanPackages;
        for (int i = 0; i < tbl->rowCount(); ++i) {
            QTableWidgetItem *nameItem = tbl->item(i, 0);
            bool matches = val.isEmpty() || (nameItem && nameItem->text().contains(val, Qt::CaseInsensitive));
            tbl->setRowHidden(i, !matches);
        }
    } else if (current == ui->pageAptHistory) {
        QTableWidget *tbl = ui->tableWidgetAptHistory;
        for (int i = 0; i < tbl->rowCount(); ++i) {
            bool matches = val.isEmpty();
            for (int c = 0; c < tbl->columnCount() && !matches; ++c) {
                QTableWidgetItem *it = tbl->item(i, c);
                if (it && it->text().contains(val, Qt::CaseInsensitive))
                    matches = true;
            }
            tbl->setRowHidden(i, !matches);
        }
    }
}

void UninstallerPage::on_btnSystemPackages_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageSystemPackages);
    setAppCount();  // restores btnUninstall visibility based on contents
#ifndef Q_OS_MAC
    ui->chkPurge->show();
#endif
}

void UninstallerPage::on_btnSnapPackages_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageSnapPackages);
    setAppCount();
#ifndef Q_OS_MAC
    ui->chkPurge->show();
#endif
}

void UninstallerPage::on_btnFlatpakPackages_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageFlatpakPackages);
    setAppCount();
#ifndef Q_OS_MAC
    ui->chkPurge->show();
#endif
}

void UninstallerPage::on_btnOrphanPackages_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageOrphanPackages);
    setAppCount();
#ifndef Q_OS_MAC
    ui->chkPurge->show();
#endif
}

void UninstallerPage::on_btnAptHistory_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageAptHistory);
    // APT History has its own per-row action bar — the page-global Uninstall
    // button doesn't apply on this tab.
    ui->btnUninstall->hide();
    ui->chkPurge->hide();
}

void UninstallerPage::on_listWidgetSnapPackages_itemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)")
                              .arg(getSelectedSnapPackages().count() + getSelectedFlatpakPackages().count()
                                   + getSelectedPackages().count()));
}

void UninstallerPage::on_listWidgetFlatpakPackages_itemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)")
                              .arg(getSelectedSnapPackages().count() + getSelectedFlatpakPackages().count()
                                   + getSelectedPackages().count()));
}

void UninstallerPage::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    const int count = getSelectedSnapPackages().count() + getSelectedFlatpakPackages().count()
                     + getSelectedPackages().count();
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)").arg(count));
    // SSO-15384 / Design Anchor: button stays disabled until at least one
    // item is explicitly checked — never a silent no-op.
    ui->btnUninstall->setEnabled(count > 0);
}

#ifndef Q_OS_MACOS
// FW-07 (SSO-3735): APT 3.1 transaction-history slots.

void UninstallerPage::onAptHistoryFetched(QList<AptHistoryEntry> entries)
{
    QTableWidget *tbl = ui->tableWidgetAptHistory;
    tbl->setSortingEnabled(false);
    tbl->setRowCount(0);

    for (const AptHistoryEntry &e : entries) {
        int row = tbl->rowCount();
        tbl->insertRow(row);

        auto *idItem = new QTableWidgetItem();
        idItem->setData(Qt::DisplayRole, e.id);
        idItem->setData(Qt::UserRole, e.id);
        idItem->setTextAlignment(Qt::AlignCenter);
        tbl->setItem(row, 0, idItem);

        tbl->setItem(row, 1, new QTableWidgetItem(e.dateTime));
        tbl->setItem(row, 2, new QTableWidgetItem(e.operation));

        auto *cmdItem = new QTableWidgetItem(e.commandLine);
        cmdItem->setToolTip(e.commandLine);
        tbl->setItem(row, 3, cmdItem);
    }

    tbl->setSortingEnabled(true);
    tbl->sortItems(0, Qt::DescendingOrder);

    const bool any = tbl->rowCount() > 0;
    ui->btnAptHistoryUndoLast->setEnabled(any);
    ui->notFoundWidget_4->setVisible(!any);
    if (!any) {
        ui->lblNotFoundHistory->setText(tr("No apt transactions recorded yet."));
    }

    setAppCount();
}

void UninstallerPage::onAptHistorySelectionChanged()
{
    const bool hasSel = !ui->tableWidgetAptHistory->selectedItems().isEmpty();
    ui->btnAptHistoryUndoSelected->setEnabled(hasSel);
    ui->btnAptHistoryRollback->setEnabled(hasSel);
}

static int aptHistorySelectedId(QTableWidget *tbl)
{
    const QList<QTableWidgetItem*> sel = tbl->selectedItems();
    if (sel.isEmpty())
        return 0;
    // The id column lives at column 0; selectedItems returns the whole row.
    for (QTableWidgetItem *it : sel) {
        if (it->column() == 0)
            return it->data(Qt::UserRole).toInt();
    }
    return 0;
}

void UninstallerPage::on_btnAptHistoryUndoLast_clicked()
{
    QTableWidget *tbl = ui->tableWidgetAptHistory;
    if (tbl->rowCount() == 0)
        return;

    // The table is sorted descending by id; row 0 holds the most recent
    // transaction. Reading by row instead of by sort order keeps the user's
    // current sort selection from changing the meaning of "last".
    int maxId = 0;
    for (int r = 0; r < tbl->rowCount(); ++r) {
        QTableWidgetItem *it = tbl->item(r, 0);
        if (it)
            maxId = std::max(maxId, it->data(Qt::UserRole).toInt());
    }
    if (maxId <= 0)
        return;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Undo APT Transaction"),
        tr("This will reverse apt transaction #%1.\n\nContinue?").arg(maxId),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Ok)
        return;

    mPackageService->aptHistoryUndo(maxId);
}

void UninstallerPage::on_btnAptHistoryUndoSelected_clicked()
{
    const int id = aptHistorySelectedId(ui->tableWidgetAptHistory);
    if (id <= 0)
        return;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Undo APT Transaction"),
        tr("This will reverse apt transaction #%1.\n\nContinue?").arg(id),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Ok)
        return;

    mPackageService->aptHistoryUndo(id);
}

void UninstallerPage::on_btnAptHistoryRollback_clicked()
{
    const int id = aptHistorySelectedId(ui->tableWidgetAptHistory);
    if (id <= 0)
        return;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Rollback"),
        tr("This will reverse every apt transaction newer than #%1.\n\nContinue?").arg(id),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Ok)
        return;

    mPackageService->aptHistoryRollback(id);
}

void UninstallerPage::on_btnAptWhy_clicked()
{
    const QString pkg = ui->txtAptWhyPackage->text().trimmed();
    if (pkg.isEmpty())
        return;
    ui->txtAptWhyOutput->setPlainText(tr("Looking up…"));
    mPackageService->fetchAptWhy(pkg, /*whyNot=*/false);
}

void UninstallerPage::on_btnAptWhyNot_clicked()
{
    const QString pkg = ui->txtAptWhyPackage->text().trimmed();
    if (pkg.isEmpty())
        return;
    ui->txtAptWhyOutput->setPlainText(tr("Looking up…"));
    mPackageService->fetchAptWhy(pkg, /*whyNot=*/true);
}

void UninstallerPage::onAptWhyFetched(QString package, bool whyNot, QStringList reasons)
{
    QString header = whyNot
        ? tr("apt why-not %1").arg(package)
        : tr("apt why %1").arg(package);
    QString body = reasons.isEmpty()
        ? tr("(no answer)")
        : reasons.join('\n');
    ui->txtAptWhyOutput->setPlainText(header + "\n\n" + body);
}
#endif // !Q_OS_MACOS
