#include "uninstaller_page.h"
#include "ui_uninstallerpage.h"
#include <QMovie>
#include <QMessageBox>
#include <QMap>
#include "utilities.h"

UninstallerPage::~UninstallerPage()
{
    delete ui;
}

UninstallerPage::UninstallerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UninstallerPage),
    tm(ToolManager::ins())
{
    ui->setupUi(this);

    init();
}

void UninstallerPage::init()
{
    QString iconLoading = QString(":/static/themes/%1/img/loading.gif").arg(AppManager::ins()->resolveThemeName());
    QMovie *loadingMovie = new QMovie(iconLoading, QByteArray(), this);
    ui->lblLoadingUninstaller->setMovie(loadingMovie);
    loadingMovie->start();
    ui->lblLoadingUninstaller->hide();

    ui->stackedWidget->setCurrentIndex(0);

#ifdef Q_OS_MACOS
    ui->chkPurge->hide();
#endif

    QList<QWidget*> widgets = { ui->txtPackageSearch, ui->btnUninstall, ui->btnSystemPackages, ui->btnSnapPackages };
    Utilities::addDropShadow(widgets, 40);

    connect(this, &UninstallerPage::packagesLoadedS, this, &UninstallerPage::onPackagesLoaded);
    connect(this, &UninstallerPage::snapPackagesLoadedS, this, &UninstallerPage::onSnapPackagesLoaded);
    connect(ui->treeWidgetPackages, &QTreeWidget::itemChanged, this, &UninstallerPage::onTreeItemChanged);

    // Initial load via worker threads
    (void)QtConcurrent::run([this]() { fetchPackages(); });
    (void)QtConcurrent::run([this]() { fetchSnapPackages(); });

    connect(SignalMapper::ins(), &SignalMapper::sigUninstallStarted, this, &UninstallerPage::uninstallStarted);
    // After uninstall finishes, re-fetch packages on worker threads
    connect(SignalMapper::ins(), &SignalMapper::sigUninstallFinished, this, [this]() {
        (void)QtConcurrent::run([this]() { fetchPackages(); });
    });
    connect(SignalMapper::ins(), &SignalMapper::sigUninstallFinished, this, [this]() {
        (void)QtConcurrent::run([this]() { fetchSnapPackages(); });
    });
}

void UninstallerPage::fetchPackages()
{
    // Worker thread: I/O only, no UI access
    mPackages = tm->getPackages();
    emit packagesLoadedS();
}

void UninstallerPage::fetchSnapPackages()
{
    // Worker thread: I/O only, no UI access
    mSnapPackages = tm->getSnapPackages();
    emit snapPackagesLoadedS();
}

void UninstallerPage::onPackagesLoaded()
{
    // Main thread: all UI updates
    emit uninstallStarted();

    ui->treeWidgetPackages->clear();
    ui->treeWidgetPackages->blockSignals(true);

    // Group packages by section
    QMap<QString, QList<Package>> grouped;
    for (const Package &pkg : mPackages) {
        QString section = pkg.section.isEmpty() ? "other" : pkg.section;
        // Strip repository prefix (e.g. "universe/libs" → "libs")
        section = section.section('/', -1);
        grouped[section].append(pkg);
    }

    QIcon fallbackIcon(":/static/themes/common/img/package.png");

    // Create tree: section headers with package children
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
            QString displayText = pkg.description.isEmpty()
                ? pkg.name
                : QString("%1 (%2)").arg(pkg.description, pkg.name);
            item->setText(0, displayText);
            item->setIcon(0, QIcon::fromTheme(pkg.name, fallbackIcon));
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

void UninstallerPage::onSnapPackagesLoaded()
{
    // Main thread: all UI updates
    ui->listWidgetSnapPackages->clear();

    QIcon icon(":/static/themes/common/img/package.png");
    for (const QString &package : mSnapPackages) {
        QListWidgetItem *item = new QListWidgetItem(QIcon::fromTheme(package, icon), QString("  %1").arg(package));
        item->setCheckState(Qt::Unchecked);
        ui->listWidgetSnapPackages->addItem(item);
    }
    setAppCount();

    ui->listWidgetSnapPackages->setEnabled(true);
    ui->txtPackageSearch->setEnabled(true);
    ui->txtPackageSearch->clear();

    ui->lblLoadingUninstaller->hide();
}

void UninstallerPage::setAppCount()
{
    // Count all child items across all sections in the tree
    int count = 0;
    for (int i = 0; i < ui->treeWidgetPackages->topLevelItemCount(); ++i)
        count += ui->treeWidgetPackages->topLevelItem(i)->childCount();

    ui->btnSystemPackages->setText(tr("Packages (%1)").arg(count));
    ui->notFoundWidget->setVisible(! count);
    ui->treeWidgetPackages->setVisible(count);

    int snapCount = ui->listWidgetSnapPackages->count();
    ui->btnSnapPackages->setText(tr("Snap Packages (%1)").arg(snapCount));
    ui->notFoundWidget_2->setVisible(! snapCount);
    ui->listWidgetSnapPackages->setVisible(snapCount);

    ui->btnSnapPackages->setVisible(CommandUtil::isExecutable("snap"));

    ui->btnUninstall->setVisible(count || snapCount);
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

void UninstallerPage::on_btnUninstall_clicked()
{
    QStringList selectedPackages = getSelectedPackages();
    QStringList selectedSnapPackages = getSelectedSnapPackages();

    if (selectedPackages.isEmpty() && selectedSnapPackages.isEmpty())
        return;

    // Run dry-run to discover dependency removals
    QStringList allWouldRemove;
    if (!selectedPackages.isEmpty())
        allWouldRemove = tm->dryRunRemovePackages(selectedPackages);

    // Build confirmation message
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

    (void)QtConcurrent::run([=]
    {
        emit SignalMapper::ins()->sigUninstallStarted();

        ToolManager::ins()->uninstallPackages(selectedPackages, purge);
        ToolManager::ins()->uninstallSnapPackages(selectedSnapPackages);

        emit SignalMapper::ins()->sigUninstallFinished();
    });
}

void UninstallerPage::uninstallStarted()
{
    ui->treeWidgetPackages->setEnabled(false);
    ui->listWidgetSnapPackages->setEnabled(false);
    ui->txtPackageSearch->setEnabled(false);
    ui->btnUninstall->hide();
    ui->lblLoadingUninstaller->show();
}

void UninstallerPage::on_txtPackageSearch_textChanged(const QString &val)
{
    if (ui->stackedWidget->currentIndex() == 0) {
        // Tree widget search for system packages
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
    } else {
        // Flat list search for snap packages
        QList<QListWidgetItem*> matches = ui->listWidgetSnapPackages->findItems(val, Qt::MatchFlag::MatchContains);
        for (int i = 0; i < ui->listWidgetSnapPackages->count(); ++i)
            ui->listWidgetSnapPackages->item(i)->setHidden(true);
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

void UninstallerPage::on_listWidgetSnapPackages_itemClicked(QListWidgetItem *item)
{
    //item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
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
