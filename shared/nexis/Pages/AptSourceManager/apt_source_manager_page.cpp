#include "apt_source_manager_page.h"
#include "ui_apt_source_manager_page.h"
#include <QDebug>
#include <QMessageBox>
#include "utilities.h"
#include "Managers/tool_manager.h"
#include "signal_mapper.h"
#include "dpi.h"
#include <Tools/package_tool_shared.h>
#include "Managers/data_refresh_service.h"
#include <Info/update_info.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>

#ifdef Q_OS_MAC
#include <QFont>
#include <QHeaderView>
#include "Managers/app_manager.h"
#endif

APTSourceManagerPage::~APTSourceManagerPage()
{
    delete ui;
}

APTSourcePtr APTSourceManagerPage::selectedAptSource = nullptr;

APTSourceManagerPage::APTSourceManagerPage(QWidget *parent, ToolManager *toolManager, SignalMapper *signalMapper, DataRefreshService *refreshService) :
    QWidget(parent),
    ui(new Ui::APTSourceManagerPage),
    mToolManager(toolManager ? toolManager : ToolManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mRefresh(refreshService ? refreshService : DataRefreshService::ins())
{
    ui->setupUi(this);

    init();
}

void APTSourceManagerPage::init()
{
    // --- Available Updates section ---
    mUpdatesSection = new QWidget(this);
    mUpdatesSection->setObjectName("updatesSection");
    mUpdatesSection->hide();

    QVBoxLayout *updLayout = new QVBoxLayout(mUpdatesSection);
    updLayout->setContentsMargins(0, 0, 0, 10);
    updLayout->setSpacing(5);

    // Header row: title + check now button
    QHBoxLayout *updHeader = new QHBoxLayout();
    mLblUpdatesTitle = new QLabel(tr("Available Updates"), mUpdatesSection);
    mLblUpdatesTitle->setObjectName("lblUpdatesTitle");
    QFont updFont = mLblUpdatesTitle->font();
    updFont.setPointSize(11);
    mLblUpdatesTitle->setFont(updFont);
    updHeader->addWidget(mLblUpdatesTitle);

    updHeader->addStretch();

    mBtnCheckNow = new QPushButton(tr("Check Now"), mUpdatesSection);
    mBtnCheckNow->setObjectName("btnCheckNow");
    mBtnCheckNow->setCursor(Qt::PointingHandCursor);
    mBtnCheckNow->setFocusPolicy(Qt::NoFocus);
    mBtnCheckNow->setAccessibleName("primary");
    mBtnCheckNow->setFixedHeight(28);
    updHeader->addWidget(mBtnCheckNow);
    updLayout->addLayout(updHeader);

    // Updates tree widget: Source | Package | Version
    mUpdatesTree = new QTreeWidget(mUpdatesSection);
    mUpdatesTree->setObjectName("treeWidgetUpdates");
    mUpdatesTree->setHeaderLabels({ tr("Source"), tr("Package"), tr("Version") });
    mUpdatesTree->setColumnCount(3);
    mUpdatesTree->setRootIsDecorated(false);
    mUpdatesTree->setFocusPolicy(Qt::NoFocus);
    mUpdatesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mUpdatesTree->setSelectionMode(QAbstractItemView::NoSelection);
    mUpdatesTree->header()->setStretchLastSection(true);
    mUpdatesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mUpdatesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mUpdatesTree->setMaximumHeight(200);
    updLayout->addWidget(mUpdatesTree);

    // Insert updates section into page layout BEFORE the main content
    ui->verticalLayout_2->insertWidget(0, mUpdatesSection);

    // Wire signal and button
    connect(mBtnCheckNow, &QPushButton::clicked, this, [this]() {
        mBtnCheckNow->setEnabled(false);
        mBtnCheckNow->setText(tr("Checking..."));
        mRefresh->triggerUpdateCheck();
    });
    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, &APTSourceManagerPage::onSystemUpdatesChecked);

#ifdef Q_OS_MAC
    // macOS: Homebrew packages with tree widget layout (like Uninstaller page)

    ui->txtAptSource->setPlaceholderText(tr("example %1")
                                         .arg("'package-name'"));
    // Hide controls that don't apply to Homebrew packages
    ui->btnEditAptSource->hide();
    ui->checkEnableSource->hide();
    ui->lblAptSourceSelectInfo->hide();

    // Hide the flat QListWidget — we use a tree widget instead
    ui->listWidgetAptSources->hide();
    ui->notFoundWidget->hide();

    // Create tree widget programmatically (matches System Cleaner table style)
    mTreeWidget = new QTreeWidget(ui->verticalWidget_2);
    mTreeWidget->setObjectName("treeWidgetPackages");
    mTreeWidget->setHeaderHidden(false);
    mTreeWidget->setHeaderLabels({ tr("Package") });
    mTreeWidget->header()->setFixedHeight(Dpi::scale(30));
    mTreeWidget->header()->setStretchLastSection(true);
    mTreeWidget->setColumnCount(1);
    mTreeWidget->setFocusPolicy(Qt::NoFocus);
    mTreeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTreeWidget->setSelectionMode(QAbstractItemView::NoSelection);
    mTreeWidget->setIconSize(Dpi::scale(20, 20));
    mTreeWidget->setIndentation(Dpi::scale(20));

    // Insert into the existing vertical layout
    ui->verticalLayout->addWidget(mTreeWidget);

    // Connect tree checkbox changes
    connect(mTreeWidget, &QTreeWidget::itemChanged,
            this, &APTSourceManagerPage::onTreeItemChanged);
    connect(this, &APTSourceManagerPage::brewPackagesLoaded,
            this, &APTSourceManagerPage::onBrewPackagesLoaded);

    on_btnCancel_clicked();

    // Fetch packages on background thread
    (void)QtConcurrent::run([this]() { fetchBrewPackages(); });

    // Re-fetch after uninstall finishes
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        (void)QtConcurrent::run([this]() { fetchBrewPackages(); });
    });

#else
    if (mToolManager->packageTool()->currentPackageTool == APT_RPM) {
        ui->txtAptSource->setPlaceholderText(tr("example %1")
            .arg("'rpm [p10] http://mirror.yandex.ru/altlinux/ p10/branch/x86_64-i586 classic'"));
    } else {
        ui->txtAptSource->setPlaceholderText(tr("example %1")
            .arg("'ppa:deadsnakes/ppa'"));
    }

    loadAptSources();

    on_btnCancel_clicked();
#endif

    QList<QWidget*> widgets = {
        ui->btnAddAPTSourceRepository, ui->btnCancel, ui->btnDeleteAptSource,
        ui->txtSearchAptSource, ui->txtSearchAptSource
    };
#ifndef Q_OS_MAC
    widgets.append(ui->btnEditAptSource);
#endif
    Utilities::addDropShadow(widgets, 40);
}

void APTSourceManagerPage::loadAptSources()
{
    ui->listWidgetAptSources->clear();

    QList<APTSourcePtr> aptSourceList = mToolManager->getSourceList();

    for (APTSourcePtr &aptSource: aptSourceList) {

        QListWidgetItem *listItem = new QListWidgetItem(ui->listWidgetAptSources);
        // Store searchable text: name + uri + description
        QString searchData = aptSource->source + " " + aptSource->uri + " " + aptSource->components;
        listItem->setData(5, searchData);

        APTSourceRepositoryItem *aptSourceItem = new APTSourceRepositoryItem(aptSource, ui->listWidgetAptSources);

        listItem->setSizeHint(aptSourceItem->sizeHint() + QSize(0, 1));

        ui->listWidgetAptSources->setItemWidget(listItem, aptSourceItem);
    }

    ui->notFoundWidget->setVisible(aptSourceList.isEmpty());

#ifdef Q_OS_MAC
    ui->lblAptSourceTitle->setText(tr("Homebrew Packages (%1)")
                                   .arg(aptSourceList.count()));
    if (aptSourceList.isEmpty())
        ui->lblNotFound->setText(tr("No Homebrew Packages Found"));
#else
    ui->lblAptSourceTitle->setText(tr("APT Repositories (%1)")
                                   .arg(aptSourceList.count()));
#endif
}

#ifdef Q_OS_MAC
void APTSourceManagerPage::fetchBrewPackages()
{
    // Worker thread: I/O only, no UI access
    mBrewPackages = mToolManager->getPackages();
    emit brewPackagesLoaded();
}

void APTSourceManagerPage::onBrewPackagesLoaded()
{
    // Main thread: populate tree widget
    mTreeWidget->clear();
    mTreeWidget->blockSignals(true);

    // Group packages by section (formula / cask)
    QMap<QString, QList<Package>> grouped;
    for (const Package &pkg : mBrewPackages) {
        grouped[pkg.section].append(pkg);
    }

    QIcon fallbackIcon(":/static/themes/common/img/package.png");
    QStringList sections = grouped.keys();
    sections.sort();

    for (const QString &section : sections) {
        const QList<Package> &pkgs = grouped[section];
        QString friendlyName = PackageTool::friendlySectionName(section);

        QTreeWidgetItem *sectionItem = new QTreeWidgetItem(mTreeWidget);
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
            item->setIcon(0, fallbackIcon);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, pkg.name);
            // Store section (formula/cask) for potential future use
            item->setData(0, Qt::UserRole + 1, pkg.section);
        }
    }

    mTreeWidget->blockSignals(false);

    // Update title with count
    int count = 0;
    for (int i = 0; i < mTreeWidget->topLevelItemCount(); ++i)
        count += mTreeWidget->topLevelItem(i)->childCount();

    ui->lblAptSourceTitle->setText(tr("Homebrew Packages (%1)").arg(count));
    mTreeWidget->setVisible(count > 0);
    updateBrewUninstallButton();

    mTreeWidget->setEnabled(true);
    ui->txtSearchAptSource->setEnabled(true);
    ui->txtSearchAptSource->clear();
}

void APTSourceManagerPage::onTreeItemChanged(QTreeWidgetItem *, int)
{
    updateBrewUninstallButton();
}

QStringList APTSourceManagerPage::getSelectedBrewPackages()
{
    QStringList selected;
    if (!mTreeWidget)
        return selected;
    for (int i = 0; i < mTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = mTreeWidget->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                selected << item->data(0, Qt::UserRole).toString();
        }
    }
    return selected;
}

void APTSourceManagerPage::updateBrewUninstallButton()
{
    int count = getSelectedBrewPackages().count();
    if (count > 0)
        ui->btnDeleteAptSource->setText(tr("Uninstall Selected (%1)").arg(count));
    else
        ui->btnDeleteAptSource->setText(tr("Uninstall"));
}
#endif

void APTSourceManagerPage::on_btnAddAPTSourceRepository_clicked(bool checked)
{
    if (checked) {
        ui->btnAddAPTSourceRepository->setText(tr("Save"));
        changeElementsVisible(checked);
    } else {
        QString aptSourceRepository = ui->txtAptSource->text().trimmed();

        if (! aptSourceRepository.isEmpty()) {
            ui->btnAddAPTSourceRepository->setText(tr("Adding..."));
            ui->btnAddAPTSourceRepository->setEnabled(false);

            mToolManager->addAPTRepository(aptSourceRepository, ui->checkEnableSource->isChecked());

            ui->txtAptSource->clear();
            ui->checkEnableSource->setChecked(false);
            ui->btnAddAPTSourceRepository->setEnabled(true);
            on_btnCancel_clicked();
            selectedAptSource.clear();
#ifdef Q_OS_MAC
            (void)QtConcurrent::run([this]() { fetchBrewPackages(); });
#else
            loadAptSources();
            on_txtSearchAptSource_textChanged(ui->txtSearchAptSource->text());
#endif
        }
    }
}

void APTSourceManagerPage::on_btnCancel_clicked()
{
    ui->btnAddAPTSourceRepository->setChecked(false);
    changeElementsVisible(false);
#ifdef Q_OS_MAC
    ui->btnAddAPTSourceRepository->setText(tr("Install"));
#else
    ui->btnAddAPTSourceRepository->setText(tr("Add Repository"));
#endif
}

void APTSourceManagerPage::changeElementsVisible(const bool checked)
{
    ui->txtAptSource->setVisible(checked);
    ui->btnCancel->setVisible(checked);
    ui->btnDeleteAptSource->setVisible(!checked);
    ui->bottomSectionHorizontalSpacer->changeSize(0, 0, checked ? QSizePolicy::Minimum : QSizePolicy::Expanding);
#ifdef Q_OS_MAC
    // Homebrew packages can't be edited or toggled — keep these hidden
    ui->checkEnableSource->setVisible(false);
    ui->btnEditAptSource->setVisible(false);
    if (!checked)
        updateBrewUninstallButton();
#else
    ui->checkEnableSource->setVisible(checked);
    ui->btnEditAptSource->setVisible(!checked);
#endif
}

void APTSourceManagerPage::on_listWidgetAptSources_itemClicked(QListWidgetItem *item)
{
    QWidget *widget = ui->listWidgetAptSources->itemWidget(item);
    if (widget) {
        APTSourceRepositoryItem *aptSourceItem = dynamic_cast<APTSourceRepositoryItem*>(widget);
        if (aptSourceItem) {
            selectedAptSource = aptSourceItem->aptSource();
        }
    } else {
        selectedAptSource.clear();
    }
}

void APTSourceManagerPage::on_listWidgetAptSources_itemDoubleClicked(QListWidgetItem *item)
{
    on_listWidgetAptSources_itemClicked(item);
#ifndef Q_OS_MAC
    // Edit is not applicable for Homebrew packages
    on_btnEditAptSource_clicked();
#endif
}

void APTSourceManagerPage::on_btnDeleteAptSource_clicked()
{
#ifdef Q_OS_MAC
    QStringList selected = getSelectedBrewPackages();
    if (selected.isEmpty())
        return;

    // Dry-run to show dependencies
    QStringList allWouldRemove = mToolManager->dryRunRemovePackages(selected);
    QStringList additional;
    for (const QString &pkg : allWouldRemove) {
        if (!selected.contains(pkg))
            additional << pkg;
    }

    QString message = tr("The following packages will be uninstalled:\n\n") + selected.join("\n");
    if (!additional.isEmpty())
        message += "\n\n" + tr("Packages that also depend on these:\n\n") + additional.join("\n");

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Uninstall"),
        message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (reply != QMessageBox::Ok)
        return;

    QStringList packagesToRemove = selected;
    (void)QtConcurrent::run([this, packagesToRemove]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->uninstallPackages(packagesToRemove);
        emit mSignalMapper->sigUninstallFinished();
    });
#else
    if (! selectedAptSource.isNull()) {
        mToolManager->removeAPTSource(selectedAptSource);
        selectedAptSource.clear();
        loadAptSources();
        on_txtSearchAptSource_textChanged(ui->txtSearchAptSource->text());
    }
#endif
}

void APTSourceManagerPage::on_txtSearchAptSource_textChanged(const QString &val)
{
#ifdef Q_OS_MAC
    if (!mTreeWidget)
        return;
    for (int i = 0; i < mTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = mTreeWidget->topLevelItem(i);
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
#else
    for (int i = 0; i < ui->listWidgetAptSources->count(); ++i) {
        QListWidgetItem *item = ui->listWidgetAptSources->item(i);
        if (item) {
            bool isContain = item->data(5).toString().contains(val, Qt::CaseInsensitive);
            item->setHidden(! isContain);
        }
    }
#endif
}

void APTSourceManagerPage::on_btnEditAptSource_clicked()
{
    if (! selectedAptSource.isNull()) {
        if (mAptSourceEditDialog.isNull()) {
            mAptSourceEditDialog = QSharedPointer<APTSourceEdit>(new APTSourceEdit(this));
            connect(mAptSourceEditDialog.data(), &APTSourceEdit::saved, this, [this]() {
                selectedAptSource.clear();
                loadAptSources();
                on_txtSearchAptSource_textChanged(ui->txtSearchAptSource->text());
            });
        }
        APTSourceEdit::selectedAptSource = selectedAptSource;
        mAptSourceEditDialog->show();
    }
}


void APTSourceManagerPage::onSystemUpdatesChecked(const UpdateCheckResult &result)
{
    mBtnCheckNow->setEnabled(true);
    mBtnCheckNow->setText(tr("Check Now"));

    if (!result.success || result.totalCount == 0) {
        mUpdatesSection->hide();
        return;
    }

    mLblUpdatesTitle->setText(tr("Available Updates (%1)").arg(result.totalCount));
    mUpdatesTree->clear();

    for (const UpdateEntry &entry : result.entries) {
        QTreeWidgetItem *item = new QTreeWidgetItem(mUpdatesTree);
        item->setText(0, entry.source);
        item->setText(1, entry.name);
        item->setText(2, entry.version);
    }

    mUpdatesSection->show();
}
