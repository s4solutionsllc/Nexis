#include "homebrew_page.h"

#include "Managers/tool_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"
#include "dpi.h"
#include "utilities.h"
#include <Info/update_info.h>
#include <Tools/package_tool_shared.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QMessageBox>
#include <QProgressBar>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QUrl>
#include <QtConcurrent>
#include <Utils/brew_util.h>
#include <Utils/command_util.h>

HomebrewPage::HomebrewPage(QWidget *parent,
                           ToolManager *toolManager,
                           SignalMapper *signalMapper,
                           DataRefreshService *refreshService)
    : QWidget(parent)
    , mToolManager(toolManager ? toolManager : ToolManager::ins())
    , mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
    , mRefresh(refreshService ? refreshService : DataRefreshService::ins())
{
    buildUI();

    connect(this, &HomebrewPage::packagesLoaded,
            this, &HomebrewPage::onPackagesLoaded);
    connect(mTreeWidget, &QTreeWidget::itemChanged,
            this, &HomebrewPage::onTreeItemChanged);
    connect(mTxtSearch, &QLineEdit::textChanged,
            this, &HomebrewPage::onSearchTextChanged);
    connect(mBtnInstall, &QPushButton::clicked,
            this, &HomebrewPage::onInstallClicked);
    connect(mBtnCancel, &QPushButton::clicked,
            this, &HomebrewPage::onCancelClicked);
    connect(mBtnUninstall, &QPushButton::clicked,
            this, &HomebrewPage::onUninstallClicked);
    connect(mBtnCheckNow, &QPushButton::clicked, this, [this]() {
        mBtnCheckNow->setEnabled(false);
        mBtnCheckNow->setText(tr("Checking..."));
        mRefresh->triggerUpdateCheck();
    });
    connect(mUpdatesTree, &QTreeWidget::itemChanged,
            this, &HomebrewPage::onUpdatesTreeItemChanged);
    connect(mChkSelectAll, &QCheckBox::toggled,
            this, &HomebrewPage::onSelectAllToggled);
    connect(mBtnUpdateSelected, &QPushButton::clicked,
            this, &HomebrewPage::onUpdateSelectedClicked);
    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, &HomebrewPage::onSystemUpdatesChecked);
    connect(mRefresh, &DataRefreshService::repoHealthChecked,
            this, &HomebrewPage::onRepoHealthChecked);
    connect(mSparkleTree, &QTreeWidget::itemChanged,
            this, &HomebrewPage::onSparkleUpdateItemChanged);
    connect(mBtnSparkleUpdateSelected, &QPushButton::clicked,
            this, &HomebrewPage::onSparkleUpdateSelectedClicked);
    connect(mSignalMapper, &SignalMapper::sigUninstallFinished, this, [this]() {
        (void)QtConcurrent::run([this]() { fetchPackages(); });
    });

    onCancelClicked();

    (void)QtConcurrent::run([this]() { fetchPackages(); });

    // BUG-110 / FR-97 parity: lazily-constructed page backfills from cached
    // results so the updates/health surface doesn't wait for the next tick.
    if (mRefresh->hasLastUpdateCheckResult())
        onSystemUpdatesChecked(mRefresh->lastUpdateCheckResult());
    if (mRefresh->hasLastRepoHealthCache())
        onRepoHealthChecked(mRefresh->lastRepoHealthCache());
}

void HomebrewPage::buildUI()
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    // Cask Updates section — hidden until results arrive
    mUpdatesSection = new QWidget(this);
    mUpdatesSection->setObjectName("updatesSection");
    mUpdatesSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    mUpdatesSection->hide();

    auto *updLayout = new QVBoxLayout(mUpdatesSection);
    updLayout->setContentsMargins(0, 5, 0, 10);
    updLayout->setSpacing(0);

    // Design Anchor: thin (5px) progress bar anchored to the top of the
    // section header, full-width, hidden when not running.
    mUpdatesProgress = new QProgressBar(mUpdatesSection);
    mUpdatesProgress->setObjectName("homebrewUpdatesProgress");
    mUpdatesProgress->setProperty("progressRole", "thin");
    mUpdatesProgress->setTextVisible(false);
    mUpdatesProgress->setFixedHeight(Dpi::scale(5));
    mUpdatesProgress->hide();
    updLayout->addWidget(mUpdatesProgress);

    auto *updInner = new QWidget(mUpdatesSection);
    auto *updInnerLayout = new QVBoxLayout(updInner);
    updInnerLayout->setContentsMargins(30, 0, 30, 0);
    updInnerLayout->setSpacing(8);

    // DS §3 header anatomy: accent bar + title row (title, Check Now) / source line
    auto *updHeaderWidget = new QWidget(updInner);
    updHeaderWidget->setObjectName("sectionHeaderRow");
    auto *updHeaderRoot = new QVBoxLayout(updHeaderWidget);
    updHeaderRoot->setContentsMargins(0, 0, 0, 0);
    updHeaderRoot->setSpacing(2);

    auto *updHeaderRow = new QHBoxLayout();
    updHeaderRow->setContentsMargins(0, 0, 0, 0);
    updHeaderRow->setSpacing(8);

    auto *updAccentBar = new QFrame(updHeaderWidget);
    updAccentBar->setObjectName("sectionHeaderAccent");
    updAccentBar->setProperty("accentToken", "success");
    updAccentBar->setFixedWidth(3);
    updAccentBar->setMinimumHeight(26);
    updAccentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    updHeaderRow->addWidget(updAccentBar);

    auto *updTextCol = new QVBoxLayout();
    updTextCol->setContentsMargins(0, 0, 0, 0);
    updTextCol->setSpacing(2);

    auto *updTitleRow = new QHBoxLayout();
    updTitleRow->setContentsMargins(0, 0, 0, 0);
    updTitleRow->setSpacing(8);

    mLblUpdatesTitle = new QLabel(tr("Available Cask Updates"), updHeaderWidget);
    mLblUpdatesTitle->setObjectName("sectionHeaderTitle");
    updTitleRow->addWidget(mLblUpdatesTitle);
    updTitleRow->addStretch();

    mBtnCheckNow = new QPushButton(tr("Check Now"), updHeaderWidget);
    mBtnCheckNow->setObjectName("btnCheckNow");
    mBtnCheckNow->setCursor(Qt::PointingHandCursor);
    mBtnCheckNow->setFocusPolicy(Qt::NoFocus);
    mBtnCheckNow->setAccessibleName("primary");
    mBtnCheckNow->setFixedHeight(28);
    updTitleRow->addWidget(mBtnCheckNow);

    updTextCol->addLayout(updTitleRow);

    auto *lblUpdatesSource = new QLabel(tr("Outdated Homebrew casks"), updHeaderWidget);
    lblUpdatesSource->setObjectName("sectionHeaderSource");
    updTextCol->addWidget(lblUpdatesSource);

    updHeaderRow->addLayout(updTextCol, 1);
    updHeaderRoot->addLayout(updHeaderRow);

    updInnerLayout->addWidget(updHeaderWidget);

    // DS §2 elevated container — single container-level shadow; flat rows inside.
    auto *updContainer = new QWidget(updInner);
    updContainer->setObjectName("homebrewUpdatesContainer");
    updContainer->setAttribute(Qt::WA_StyledBackground, true);
    updContainer->setProperty("cardRole", "elevated");
    auto *updContainerLayout = new QVBoxLayout(updContainer);
    updContainerLayout->setContentsMargins(0, 0, 0, 0);
    updContainerLayout->setSpacing(0);

    // Columns: App (with checkbox), Current Version, Available Version
    mUpdatesTree = new QTreeWidget(updContainer);
    mUpdatesTree->setObjectName("treeWidgetUpdates");
    mUpdatesTree->setHeaderLabels({ tr("App"), tr("Current"), tr("Available") });
    mUpdatesTree->header()->setFixedHeight(Dpi::scale(30));
    mUpdatesTree->setColumnCount(3);
    mUpdatesTree->setRootIsDecorated(false);
    mUpdatesTree->setFocusPolicy(Qt::NoFocus);
    mUpdatesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mUpdatesTree->setSelectionMode(QAbstractItemView::NoSelection);
    mUpdatesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mUpdatesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mUpdatesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mUpdatesTree->headerItem()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    mUpdatesTree->headerItem()->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    mUpdatesTree->setMaximumHeight(200);
    updContainerLayout->addWidget(mUpdatesTree);

    updInnerLayout->addWidget(updContainer, 1);

    // Bottom row: Select All + Update Selected (disabled until ≥1 checked)
    auto *updBtnRow = new QHBoxLayout();
    updBtnRow->setContentsMargins(0, 0, 0, 0);
    updBtnRow->setSpacing(8);

    mChkSelectAll = new QCheckBox(tr("Select All"), updInner);
    mChkSelectAll->setFocusPolicy(Qt::NoFocus);
    updBtnRow->addWidget(mChkSelectAll);
    updBtnRow->addStretch();

    mBtnUpdateSelected = new QPushButton(tr("Update Selected"), updInner);
    mBtnUpdateSelected->setObjectName("btnUpdateSelected");
    mBtnUpdateSelected->setCursor(Qt::PointingHandCursor);
    mBtnUpdateSelected->setFocusPolicy(Qt::NoFocus);
    mBtnUpdateSelected->setAccessibleName("primary");
    mBtnUpdateSelected->setEnabled(false);
    updBtnRow->addWidget(mBtnUpdateSelected);

    updInnerLayout->addLayout(updBtnRow);

    updLayout->addWidget(updInner);

    pageLayout->addWidget(mUpdatesSection);

    // Sparkle updates section — hidden until results arrive
    buildSparkleSection(pageLayout);

    // Main content widget
    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(30, 5, 30, 20);
    contentLayout->setSpacing(8);

    // DS §3 header anatomy (NEX F2 shared recipe): accent bar + title row
    // (title, Search packages) — no source line in the approved capture.
    auto *pkgHeaderWidget = new QWidget(contentWidget);
    pkgHeaderWidget->setObjectName("sectionHeaderRow");
    auto *pkgHeaderRow = new QHBoxLayout(pkgHeaderWidget);
    pkgHeaderRow->setContentsMargins(0, 0, 0, 0);
    pkgHeaderRow->setSpacing(8);

    auto *pkgAccentBar = new QFrame(pkgHeaderWidget);
    pkgAccentBar->setObjectName("sectionHeaderAccent");
    pkgAccentBar->setProperty("accentToken", "accent");
    pkgAccentBar->setFixedWidth(3);
    pkgAccentBar->setMinimumHeight(26);
    pkgAccentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    pkgHeaderRow->addWidget(pkgAccentBar);

    // Title row + search
    auto *titleRow = new QHBoxLayout();
    mLblTitle = new QLabel(tr("Homebrew Packages"), pkgHeaderWidget);
    mLblTitle->setObjectName("sectionHeaderTitle");
    titleRow->addWidget(mLblTitle);
    titleRow->addStretch();

    mTxtSearch = new QLineEdit(pkgHeaderWidget);
    mTxtSearch->setObjectName("txtSearchAptSource");
    mTxtSearch->setPlaceholderText(tr("Search packages"));
    mTxtSearch->setClearButtonEnabled(true);
    mTxtSearch->setFixedWidth(220);
    titleRow->addWidget(mTxtSearch);

    pkgHeaderRow->addLayout(titleRow, 1);
    contentLayout->addWidget(pkgHeaderWidget);

    // DS §2 elevated container (NEX F1 shared recipe) — single
    // container-level shadow (DS §7); the collapsible tree-group rows stay
    // flat inside it.
    auto *pkgContainer = new QWidget(contentWidget);
    pkgContainer->setObjectName("homebrewPackagesContainer");
    pkgContainer->setAttribute(Qt::WA_StyledBackground, true);
    pkgContainer->setProperty("cardRole", "elevated");
    auto *pkgContainerLayout = new QVBoxLayout(pkgContainer);
    pkgContainerLayout->setContentsMargins(0, 0, 0, 0);
    pkgContainerLayout->setSpacing(0);

    // Package tree
    mTreeWidget = new QTreeWidget(pkgContainer);
    mTreeWidget->setObjectName("treeWidgetPackages");
    mTreeWidget->setHeaderLabels({ tr("Package") });
    mTreeWidget->header()->setFixedHeight(Dpi::scale(30));
    mTreeWidget->header()->setStretchLastSection(true);
    mTreeWidget->setColumnCount(1);
    mTreeWidget->setFocusPolicy(Qt::NoFocus);
    mTreeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTreeWidget->setSelectionMode(QAbstractItemView::NoSelection);
    mTreeWidget->setIconSize(Dpi::scale(20, 20));
    mTreeWidget->setIndentation(Dpi::scale(20));
    pkgContainerLayout->addWidget(mTreeWidget);

    contentLayout->addWidget(pkgContainer, 1);

    // Bottom row: install field + uninstall button
    auto *bottomRow = new QHBoxLayout();

    mTxtInstall = new QLineEdit(contentWidget);
    mTxtInstall->setObjectName("txtAptSource");
    mTxtInstall->setPlaceholderText(tr("example %1").arg("'package-name'"));
    mTxtInstall->hide();
    bottomRow->addWidget(mTxtInstall, 1);

    mBtnInstall = new QPushButton(tr("Install"), contentWidget);
    mBtnInstall->setObjectName("btnAddAPTSourceRepository");
    mBtnInstall->setCheckable(true);
    mBtnInstall->setCursor(Qt::PointingHandCursor);
    mBtnInstall->setFocusPolicy(Qt::NoFocus);
    bottomRow->addWidget(mBtnInstall);

    mBtnCancel = new QPushButton(tr("Cancel"), contentWidget);
    mBtnCancel->setObjectName("btnCancel");
    mBtnCancel->setCursor(Qt::PointingHandCursor);
    mBtnCancel->setFocusPolicy(Qt::NoFocus);
    mBtnCancel->hide();
    bottomRow->addWidget(mBtnCancel);

    bottomRow->addStretch();

    mBtnUninstall = new QPushButton(tr("Uninstall"), contentWidget);
    mBtnUninstall->setObjectName("btnDeleteAptSource");
    mBtnUninstall->setCursor(Qt::PointingHandCursor);
    mBtnUninstall->setFocusPolicy(Qt::NoFocus);
    bottomRow->addWidget(mBtnUninstall);

    contentLayout->addLayout(bottomRow);

    pageLayout->addWidget(contentWidget, 1);

    // DS §2/§7: one shadow per elevated container, never per row.
    Utilities::addDropShadow(updContainer, 90, 26);
    Utilities::addDropShadow(pkgContainer, 90, 26);

    Utilities::addDropShadow({mBtnInstall, mBtnCancel, mBtnUninstall, mBtnUpdateSelected, mTxtSearch}, 40);
}

void HomebrewPage::setInstallFieldsVisible(bool visible)
{
    mTxtInstall->setVisible(visible);
    mBtnCancel->setVisible(visible);
    mBtnUninstall->setVisible(!visible);
    if (visible) {
        mBtnInstall->setText(tr("Save"));
    } else {
        mBtnInstall->setText(tr("Install"));
        updateUninstallButton();
    }
}

void HomebrewPage::fetchPackages()
{
    // Worker thread: I/O only, no UI access
    mPackages = mToolManager->getPackages();
    emit packagesLoaded();
}

void HomebrewPage::onPackagesLoaded()
{
    mTreeWidget->clear();
    mTreeWidget->blockSignals(true);

    QMap<QString, QList<Package>> grouped;
    for (const Package &pkg : mPackages)
        grouped[pkg.section].append(pkg);

    QIcon fallbackIcon(":/static/themes/common/img/package.png");
    QStringList sections = grouped.keys();
    sections.sort();

    for (const QString &section : sections) {
        const QList<Package> &pkgs = grouped[section];
        QString friendlyName = PackageTool::friendlySectionName(section);

        auto *sectionItem = new QTreeWidgetItem(mTreeWidget);
        sectionItem->setText(0, QString("%1 (%2)").arg(friendlyName).arg(pkgs.size()));
        sectionItem->setFlags(Qt::ItemIsEnabled);

        QFont sectionFont = sectionItem->font(0);
        sectionFont.setBold(true);
        sectionItem->setFont(0, sectionFont);

        for (const Package &pkg : pkgs) {
            auto *item = new QTreeWidgetItem(sectionItem);
            QString displayText = pkg.description.isEmpty()
                ? pkg.name
                : QString("%1 (%2)").arg(pkg.description, pkg.name);
            item->setText(0, displayText);
            item->setIcon(0, fallbackIcon);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, pkg.name);
            item->setData(0, Qt::UserRole + 1, pkg.section);
        }
    }

    mTreeWidget->blockSignals(false);

    int count = 0;
    for (int i = 0; i < mTreeWidget->topLevelItemCount(); ++i)
        count += mTreeWidget->topLevelItem(i)->childCount();

    mLblTitle->setText(tr("Homebrew Packages (%1)").arg(count));
    mTreeWidget->setVisible(count > 0);
    updateUninstallButton();

    mTreeWidget->setEnabled(true);
    mTxtSearch->setEnabled(true);
    mTxtSearch->clear();
}

void HomebrewPage::onTreeItemChanged(QTreeWidgetItem *, int)
{
    updateUninstallButton();
}

QStringList HomebrewPage::getSelectedPackages() const
{
    QStringList selected;
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

void HomebrewPage::updateUninstallButton()
{
    int count = getSelectedPackages().count();
    if (count > 0)
        mBtnUninstall->setText(tr("Uninstall Selected (%1)").arg(count));
    else
        mBtnUninstall->setText(tr("Uninstall"));
}

void HomebrewPage::onSearchTextChanged(const QString &val)
{
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
}

void HomebrewPage::onInstallClicked()
{
    if (mBtnInstall->isChecked()) {
        setInstallFieldsVisible(true);
        return;
    }

    QString spec = mTxtInstall->text().trimmed();
    if (spec.isEmpty()) {
        setInstallFieldsVisible(false);
        return;
    }

    mBtnInstall->setText(tr("Installing..."));
    mBtnInstall->setEnabled(false);

    mToolManager->addRepository(spec, /*isSource=*/false);

    mTxtInstall->clear();
    mBtnInstall->setEnabled(true);
    onCancelClicked();
    (void)QtConcurrent::run([this]() { fetchPackages(); });
}

void HomebrewPage::onCancelClicked()
{
    mBtnInstall->setChecked(false);
    setInstallFieldsVisible(false);
}

void HomebrewPage::onUninstallClicked()
{
    QStringList selected = getSelectedPackages();
    if (selected.isEmpty())
        return;

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
        this, tr("Confirm Uninstall"), message,
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);

    if (reply != QMessageBox::Ok)
        return;

    QStringList packagesToRemove = selected;
    (void)QtConcurrent::run([this, packagesToRemove]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->uninstallPackages(packagesToRemove);
        emit mSignalMapper->sigUninstallFinished();
    });
}

void HomebrewPage::onSystemUpdatesChecked(const UpdateCheckResult &result)
{
    mBtnCheckNow->setEnabled(true);
    mBtnCheckNow->setText(tr("Check Now"));

    // Split entries: Sparkle ones go to the dedicated section; brew/system
    // entries stay in the existing updates tree (cask-only).
    QList<UpdateEntry> caskUpdates;
    QList<UpdateEntry> sparkleEntries;
    for (const UpdateEntry &e : result.entries) {
        if (e.source == "sparkle")
            sparkleEntries.append(e);
        else if (e.isCask)
            caskUpdates.append(e);
    }

    // Homebrew cask section
    if (!result.success || caskUpdates.isEmpty()) {
        mUpdatesSection->hide();
    } else {
        mLblUpdatesTitle->setText(tr("Available Cask Updates (%1)").arg(caskUpdates.size()));
        mUpdatesTree->blockSignals(true);
        mUpdatesTree->clear();
        for (const UpdateEntry &entry : caskUpdates) {
            auto *item = new QTreeWidgetItem(mUpdatesTree);
            item->setText(0, entry.name);
            item->setText(1, entry.installedVersion);
            item->setText(2, entry.version);
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, entry.name); // cask token for brew upgrade
        }
        mUpdatesTree->blockSignals(false);
        mChkSelectAll->setChecked(false);
        updateUpdateButton();
        mUpdatesSection->show();
    }

    // Sparkle section
    mSparkleEntries = sparkleEntries;
    mSparkleTree->blockSignals(true);
    mSparkleTree->clear();
    for (const UpdateEntry &entry : sparkleEntries) {
        auto *item = new QTreeWidgetItem(mSparkleTree);
        item->setCheckState(0, Qt::Unchecked);
        item->setText(0, entry.name);
        item->setText(1, entry.version);
        if (!entry.signatureMetadataPresent) {
            item->setText(2, tr("No Signature"));
            item->setForeground(2, QColor(0xD9, 0x53, 0x4F)); // error red
            item->setCheckState(0, Qt::Unchecked);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        } else {
            item->setText(2, QString());
        }
    }
    mSparkleTree->blockSignals(false);

    if (sparkleEntries.isEmpty()) {
        mSparkleSection->hide();
    } else {
        mLblSparkleTitle->setText(tr("App Updates — Sparkle (%1)").arg(sparkleEntries.size()));
        updateSparkleUpdateButton();
        mSparkleSection->show();
    }
}

void HomebrewPage::buildSparkleSection(QVBoxLayout *pageLayout)
{
    mSparkleSection = new QWidget(this);
    mSparkleSection->setObjectName("sparkleUpdatesSection");
    mSparkleSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    mSparkleSection->hide();

    auto *layout = new QVBoxLayout(mSparkleSection);
    layout->setContentsMargins(30, 5, 30, 10);
    layout->setSpacing(8);

    auto *headerWidget = new QWidget(mSparkleSection);
    headerWidget->setObjectName("sectionHeaderRow");
    auto *headerRoot = new QVBoxLayout(headerWidget);
    headerRoot->setContentsMargins(0, 0, 0, 0);
    headerRoot->setSpacing(2);

    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);

    auto *accentBar = new QFrame(headerWidget);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("accentToken", "warning");
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(26);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    headerRow->addWidget(accentBar);

    auto *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    mLblSparkleTitle = new QLabel(tr("App Updates — Sparkle"), headerWidget);
    mLblSparkleTitle->setObjectName("sectionHeaderTitle");
    titleRow->addWidget(mLblSparkleTitle);
    titleRow->addStretch();

    mBtnSparkleUpdateSelected = new QPushButton(tr("Update Selected"), headerWidget);
    mBtnSparkleUpdateSelected->setObjectName("btnUpdateSelected");
    mBtnSparkleUpdateSelected->setCursor(Qt::PointingHandCursor);
    mBtnSparkleUpdateSelected->setFocusPolicy(Qt::NoFocus);
    mBtnSparkleUpdateSelected->setAccessibleName("primary");
    mBtnSparkleUpdateSelected->setFixedHeight(28);
    mBtnSparkleUpdateSelected->setEnabled(false); // disabled until checkbox checked
    titleRow->addWidget(mBtnSparkleUpdateSelected);

    textCol->addLayout(titleRow);

    auto *lblSource = new QLabel(
        tr("Non-Homebrew apps with Sparkle update feeds  •  Nexis does not verify installer signatures  •  No Signature = appcast has no signature metadata"),
        headerWidget);
    lblSource->setObjectName("sectionHeaderSource");
    textCol->addWidget(lblSource);

    headerRow->addLayout(textCol, 1);
    headerRoot->addLayout(headerRow);
    layout->addWidget(headerWidget);

    auto *container = new QWidget(mSparkleSection);
    container->setObjectName("sparkleUpdatesContainer");
    container->setAttribute(Qt::WA_StyledBackground, true);
    container->setProperty("cardRole", "elevated");
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    mSparkleTree = new QTreeWidget(container);
    mSparkleTree->setObjectName("treeWidgetSparkleUpdates");
    mSparkleTree->setHeaderLabels({ tr("App"), tr("Available Version"), tr("") });
    mSparkleTree->header()->setFixedHeight(Dpi::scale(30));
    mSparkleTree->setColumnCount(3);
    mSparkleTree->setRootIsDecorated(false);
    mSparkleTree->setFocusPolicy(Qt::NoFocus);
    mSparkleTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mSparkleTree->setSelectionMode(QAbstractItemView::NoSelection);
    mSparkleTree->header()->setStretchLastSection(true);
    mSparkleTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mSparkleTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mSparkleTree->headerItem()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    mSparkleTree->setMaximumHeight(200);
    containerLayout->addWidget(mSparkleTree);

    layout->addWidget(container, 1);
    pageLayout->addWidget(mSparkleSection);
}

void HomebrewPage::updateSparkleUpdateButton()
{
    int checked = 0;
    for (int i = 0; i < mSparkleTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = mSparkleTree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            ++checked;
    }
    mBtnSparkleUpdateSelected->setEnabled(checked > 0);
    if (checked > 0)
        mBtnSparkleUpdateSelected->setText(tr("Update Selected (%1)").arg(checked));
    else
        mBtnSparkleUpdateSelected->setText(tr("Update Selected"));
}

void HomebrewPage::onSparkleUpdateItemChanged(QTreeWidgetItem *, int column)
{
    if (column == 0)
        updateSparkleUpdateButton();
}

void HomebrewPage::onSparkleUpdateSelectedClicked()
{
    // Collect checked entries with signature metadata present and open the
    // enclosure URL in the browser. Nexis does not download the installer or
    // cryptographically verify it — this only opens the publisher's download
    // page. SparkleSignatureVerifier will be invoked against the downloaded
    // bytes once a download agent exists (see SSO-15431); until then, this
    // gate on signatureMetadataPresent is not a security control, only a
    // signal to steer users away from feeds that don't even carry a signature.
    for (int i = 0; i < mSparkleTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = mSparkleTree->topLevelItem(i);
        if (item->checkState(0) != Qt::Checked)
            continue;
        if (i >= mSparkleEntries.size())
            continue;
        const UpdateEntry &entry = mSparkleEntries[i];
        if (!entry.signatureMetadataPresent || entry.enclosureUrl.isEmpty())
            continue;
        QDesktopServices::openUrl(QUrl(entry.enclosureUrl));
    }
}

QStringList HomebrewPage::getSelectedCaskUpdates() const
{
    QStringList selected;
    for (int i = 0; i < mUpdatesTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = mUpdatesTree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            selected << item->data(0, Qt::UserRole).toString();
    }
    return selected;
}

void HomebrewPage::updateUpdateButton()
{
    int count = getSelectedCaskUpdates().count();
    if (count > 0)
        mBtnUpdateSelected->setText(tr("Update Selected (%1)").arg(count));
    else
        mBtnUpdateSelected->setText(tr("Update Selected"));
    mBtnUpdateSelected->setEnabled(count > 0 && !mUpdatesRunning);
}

void HomebrewPage::onUpdatesTreeItemChanged(QTreeWidgetItem *, int)
{
    // Sync "Select All" state without triggering its toggled signal
    int total = mUpdatesTree->topLevelItemCount();
    int checked = getSelectedCaskUpdates().count();
    mChkSelectAll->blockSignals(true);
    mChkSelectAll->setCheckState(
        checked == 0 ? Qt::Unchecked
        : checked == total ? Qt::Checked
        : Qt::PartiallyChecked);
    mChkSelectAll->blockSignals(false);
    updateUpdateButton();
}

void HomebrewPage::onSelectAllToggled(bool checked)
{
    mUpdatesTree->blockSignals(true);
    for (int i = 0; i < mUpdatesTree->topLevelItemCount(); ++i)
        mUpdatesTree->topLevelItem(i)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    mUpdatesTree->blockSignals(false);
    updateUpdateButton();
}

void HomebrewPage::onUpdateSelectedClicked()
{
    QStringList casks = getSelectedCaskUpdates();
    if (casks.isEmpty() || mUpdatesRunning)
        return;

    mUpdatesRunning = true;
    mBtnUpdateSelected->setEnabled(false);
    mBtnCheckNow->setEnabled(false);
    mChkSelectAll->setEnabled(false);
    mUpdatesProgress->setRange(0, casks.size());
    mUpdatesProgress->setValue(0);
    mUpdatesProgress->show();

    // Capture tree items by cask token for per-item status updates
    QMap<QString, QTreeWidgetItem *> itemMap;
    for (int i = 0; i < mUpdatesTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = mUpdatesTree->topLevelItem(i);
        itemMap[it->data(0, Qt::UserRole).toString()] = it;
    }

    (void)QtConcurrent::run([this, casks, itemMap]() mutable {
        QString brewPath = findBrew();
        int done = 0;
        for (const QString &cask : casks) {
            ExecResult res = CommandUtil::execWithStatus(
                brewPath, {"upgrade", "--cask", cask}, 300000);
            done++;
            // Update UI on the main thread
            QMetaObject::invokeMethod(this, [this, cask, res, done, itemMap]() {
                if (itemMap.contains(cask)) {
                    QTreeWidgetItem *it = itemMap[cask];
                    if (res.ok()) {
                        // Grey out successfully updated row
                        for (int col = 0; col < mUpdatesTree->columnCount(); ++col)
                            it->setForeground(col, QColor(128, 128, 128));
                        it->setText(0, it->text(0) + tr(" ✓"));
                        it->setCheckState(0, Qt::Unchecked);
                        it->setFlags(it->flags() & ~Qt::ItemIsUserCheckable);
                    } else {
                        it->setText(0, it->text(0) + tr(" ✗ (%1)").arg(res.error.left(80)));
                        it->setForeground(0, QColor(200, 50, 50));
                        it->setFlags(it->flags() & ~Qt::ItemIsUserCheckable);
                    }
                }
                mUpdatesProgress->setValue(done);
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(this, [this]() {
            mUpdatesRunning = false;
            mUpdatesProgress->hide();
            mBtnCheckNow->setEnabled(true);
            mChkSelectAll->setEnabled(true);
            updateUpdateButton();
            // Refresh package list since versions may have changed
            (void)QtConcurrent::run([this]() { fetchPackages(); });
        }, Qt::QueuedConnection);
    });
}

void HomebrewPage::onRepoHealthChecked(const RepoHealthCache &cache)
{
    for (int i = 0; i < mTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = mTreeWidget->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            QString pkgName = item->data(0, Qt::UserRole).toString();
            if (!cache.contains(pkgName))
                continue;

            const RepoHealthResult &result = cache[pkgName];
            QString prefix;
            switch (result.status) {
            case RepoHealthResult::Warning: prefix = "⚠ "; break;
            case RepoHealthResult::Error:   prefix = "✗ "; break;
            default: break;
            }
            QString currentText = item->text(0);
            currentText.remove(QRegularExpression("^[⚠✗] "));
            item->setText(0, prefix + currentText);
        }
    }
}
