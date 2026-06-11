#include "homebrew_page.h"

#include "Managers/tool_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"
#include "dpi.h"
#include "utilities.h"
#include <Tools/package_tool_shared.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QMessageBox>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QToolButton>

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
    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, &HomebrewPage::onSystemUpdatesChecked);

    // SSO-3741: Upgrade All defers to brew if any brew updates are present,
    // otherwise falls back to system (softwareupdate). The brew-vs-system split
    // exists because brew cannot run as root, so the elevated softwareupdate
    // path is different — Upgrade All scopes to one at a time.
    connect(mBtnUpgradeAll, &QPushButton::clicked, this, [this]() {
        bool hasBrew = false, hasSystem = false;
        for (int i = 0; i < mUpdatesTree->topLevelItemCount(); ++i) {
            const QString src = mUpdatesTree->topLevelItem(i)->text(0);
            if (src == QLatin1String("brew"))   hasBrew = true;
            if (src == QLatin1String("system")) hasSystem = true;
        }
        if (hasBrew)        mRefresh->runUpgradeAll("brew");
        else if (hasSystem) mRefresh->runUpgradeAll("system");
    });
    connect(mRefresh, &DataRefreshService::upgradeStarted,
            this, &HomebrewPage::onUpgradeStarted);
    connect(mRefresh, &DataRefreshService::upgradeFinished,
            this, &HomebrewPage::onUpgradeFinished);
    connect(mRefresh, &DataRefreshService::repoHealthChecked,
            this, &HomebrewPage::onRepoHealthChecked);
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

    // Updates section — hidden until results arrive
    mUpdatesSection = new QWidget(this);
    mUpdatesSection->setObjectName("updatesSection");
    mUpdatesSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    mUpdatesSection->hide();

    auto *updLayout = new QVBoxLayout(mUpdatesSection);
    updLayout->setContentsMargins(30, 5, 30, 10);
    updLayout->setSpacing(5);

    auto *updHeader = new QHBoxLayout();
    mLblUpdatesTitle = new QLabel(tr("Available Updates"), mUpdatesSection);
    mLblUpdatesTitle->setObjectName("lblUpdatesTitle");
    QFont updFont = mLblUpdatesTitle->font();
    updFont.setPointSize(11);
    mLblUpdatesTitle->setFont(updFont);
    updHeader->addWidget(mLblUpdatesTitle);
    updHeader->addStretch();

    mLblUpgradeProgress = new QLabel(QString(), mUpdatesSection);
    mLblUpgradeProgress->setObjectName("lblUpgradeProgress");
    mLblUpgradeProgress->hide();
    updHeader->addWidget(mLblUpgradeProgress);

    mBtnUpgradeAll = new QPushButton(tr("Upgrade All"), mUpdatesSection);
    mBtnUpgradeAll->setObjectName("btnUpgradeAll");
    mBtnUpgradeAll->setCursor(Qt::PointingHandCursor);
    mBtnUpgradeAll->setFocusPolicy(Qt::NoFocus);
    mBtnUpgradeAll->setAccessibleName("primary");
    mBtnUpgradeAll->setFixedHeight(28);
    updHeader->addWidget(mBtnUpgradeAll);

    mBtnCheckNow = new QPushButton(tr("Check Now"), mUpdatesSection);
    mBtnCheckNow->setObjectName("btnCheckNow");
    mBtnCheckNow->setCursor(Qt::PointingHandCursor);
    mBtnCheckNow->setFocusPolicy(Qt::NoFocus);
    mBtnCheckNow->setAccessibleName("primary");
    mBtnCheckNow->setFixedHeight(28);
    updHeader->addWidget(mBtnCheckNow);
    updLayout->addLayout(updHeader);

    // SSO-3741: trailing Action column hosts a per-row Upgrade button.
    mUpdatesTree = new QTreeWidget(mUpdatesSection);
    mUpdatesTree->setObjectName("treeWidgetUpdates");
    mUpdatesTree->setHeaderLabels({ tr("Source"), tr("Package"), tr("Version"), tr("Action") });
    mUpdatesTree->header()->setFixedHeight(Dpi::scale(30));
    mUpdatesTree->setColumnCount(4);
    mUpdatesTree->setRootIsDecorated(false);
    mUpdatesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mUpdatesTree->setSelectionMode(QAbstractItemView::NoSelection);
    mUpdatesTree->header()->setStretchLastSection(false);
    mUpdatesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mUpdatesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mUpdatesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mUpdatesTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mUpdatesTree->setMaximumHeight(200);
    updLayout->addWidget(mUpdatesTree);

    pageLayout->addWidget(mUpdatesSection);

    // Main content widget
    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(30, 5, 30, 20);
    contentLayout->setSpacing(8);

    // Title row + search
    auto *titleRow = new QHBoxLayout();
    mLblTitle = new QLabel(tr("Homebrew Packages"), contentWidget);
    mLblTitle->setObjectName("lblAptSourceTitle");
    QFont titleFont = mLblTitle->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    mLblTitle->setFont(titleFont);
    titleRow->addWidget(mLblTitle);
    titleRow->addStretch();

    mTxtSearch = new QLineEdit(contentWidget);
    mTxtSearch->setObjectName("txtSearchAptSource");
    mTxtSearch->setPlaceholderText(tr("Search packages"));
    mTxtSearch->setClearButtonEnabled(true);
    mTxtSearch->setFixedWidth(220);
    titleRow->addWidget(mTxtSearch);

    contentLayout->addLayout(titleRow);

    // Package tree
    mTreeWidget = new QTreeWidget(contentWidget);
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
    contentLayout->addWidget(mTreeWidget, 1);

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

    Utilities::addDropShadow({mBtnInstall, mBtnCancel, mBtnUninstall, mTxtSearch}, 40);
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

    if (!result.success || result.totalCount == 0) {
        mUpdatesSection->hide();
        return;
    }

    mLblUpdatesTitle->setText(tr("Available Updates (%1)").arg(result.totalCount));
    mUpdatesTree->clear();

    for (const UpdateEntry &entry : result.entries) {
        auto *item = new QTreeWidgetItem(mUpdatesTree);
        item->setText(0, entry.source);
        item->setText(1, entry.name);
        item->setText(2, entry.version);

        // BUG-52: prefer QToolButton over QPushButton for compact action
        // buttons on macOS Qt6 — keeps icon/text rendering consistent.
        auto *btn = new QToolButton(mUpdatesTree);
        btn->setObjectName("btnUpgrade");
        btn->setText(tr("Upgrade"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setAutoRaise(true);
        UpdateEntry captured = entry;
        connect(btn, &QToolButton::clicked, this, [this, captured]() {
            mRefresh->runUpgrade(captured);
        });
        mUpdatesTree->setItemWidget(item, 3, btn);
    }

    mUpdatesSection->show();
}

void HomebrewPage::onUpgradeStarted(const QString &label)
{
    mBtnUpgradeAll->setEnabled(false);
    mBtnCheckNow->setEnabled(false);
    for (int i = 0; i < mUpdatesTree->topLevelItemCount(); ++i) {
        if (auto *w = mUpdatesTree->itemWidget(mUpdatesTree->topLevelItem(i), 3))
            w->setEnabled(false);
    }
    mLblUpgradeProgress->setText(tr("Upgrading %1…").arg(label));
    mLblUpgradeProgress->show();
}

void HomebrewPage::onUpgradeFinished(const QString &label, bool ok, const QString &error)
{
    mLblUpgradeProgress->hide();
    mBtnUpgradeAll->setEnabled(true);
    mBtnCheckNow->setEnabled(true);
    if (!ok && !error.isEmpty()) {
        QMessageBox::warning(this, tr("Upgrade Failed"),
            tr("%1 did not complete:\n\n%2").arg(label, error));
    }
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
