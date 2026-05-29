#include "uninstaller_page.h"
#include "ui_uninstallerpage.h"
#ifdef Q_OS_MAC
#include "crumbs_review_dialog.h"
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

    int orphanCount = ui->tableWidgetOrphanPackages->rowCount();
    ui->btnOrphanPackages->setText(tr("Orphan Packages (%1)").arg(orphanCount));
    ui->notFoundWidget_3->setVisible(! orphanCount);
    ui->tableWidgetOrphanPackages->setVisible(orphanCount);

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
    ui->tableWidgetOrphanPackages->setEnabled(false);
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
        QTableWidget *tbl = ui->tableWidgetOrphanPackages;
        for (int i = 0; i < tbl->rowCount(); ++i) {
            QTableWidgetItem *nameItem = tbl->item(i, 0);
            bool matches = val.isEmpty() || (nameItem && nameItem->text().contains(val, Qt::CaseInsensitive));
            tbl->setRowHidden(i, !matches);
        }
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

void UninstallerPage::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    ui->btnUninstall->setText(tr("Uninstall Selected (%1)")
                              .arg(getSelectedSnapPackages().count() + getSelectedPackages().count()));
}
