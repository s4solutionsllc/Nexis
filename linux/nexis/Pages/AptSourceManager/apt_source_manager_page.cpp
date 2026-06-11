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
#include "repo_detail_panel.h"
#include <QSplitter>
#include <Tools/repo_health_checker.h>
#include <Tools/repo_knowledge_base.h>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QtConcurrent>
#include "Utils/command_util.h"

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
    mUpdatesSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    mUpdatesSection->hide();

    QVBoxLayout *updLayout = new QVBoxLayout(mUpdatesSection);
    updLayout->setContentsMargins(30, 5, 30, 10);
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

    mLblUpgradeProgress = new QLabel(QString(), mUpdatesSection);
    mLblUpgradeProgress->setObjectName("lblUpgradeProgress");
    mLblUpgradeProgress->hide();
    updHeader->addWidget(mLblUpgradeProgress);

    mBtnUpgradeAll = new QPushButton(tr("Upgrade All"), mUpdatesSection);
    mBtnUpgradeAll->setObjectName("btnUpgradeAll");
    mBtnUpgradeAll->setCursor(Qt::PointingHandCursor);
    mBtnUpgradeAll->setAccessibleName("primary");
    mBtnUpgradeAll->setFixedHeight(28);
    updHeader->addWidget(mBtnUpgradeAll);

    mBtnCheckNow = new QPushButton(tr("Check Now"), mUpdatesSection);
    mBtnCheckNow->setObjectName("btnCheckNow");
    mBtnCheckNow->setCursor(Qt::PointingHandCursor);
    mBtnCheckNow->setAccessibleName("primary");
    mBtnCheckNow->setFixedHeight(28);
    updHeader->addWidget(mBtnCheckNow);

    mBtnRefreshHealth = new QPushButton(tr("Refresh Health"), mUpdatesSection);
    mBtnRefreshHealth->setObjectName("btnRefreshHealth");
    mBtnRefreshHealth->setCursor(Qt::PointingHandCursor);
    mBtnRefreshHealth->setAccessibleName("primary");
    mBtnRefreshHealth->setFixedHeight(28);
    updHeader->addWidget(mBtnRefreshHealth);

    updLayout->addLayout(updHeader);

    // Updates tree widget: Source | Package | Version | Action
    // SSO-3741: the trailing Action column hosts a per-row Upgrade button.
    mUpdatesTree = new QTreeWidget(mUpdatesSection);
    mUpdatesTree->setObjectName("treeWidgetUpdates");
    mUpdatesTree->setHeaderHidden(false);
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

    // SSO-3741: upgrade lifecycle + Upgrade All. Upgrade All defers to the
    // first detected source — apt/dnf/pacman/zypper are mutually exclusive,
    // and snap/flatpak are addressed by their own per-row button.
    connect(mBtnUpgradeAll, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < mUpdatesTree->topLevelItemCount(); ++i) {
            QString src = mUpdatesTree->topLevelItem(i)->text(0);
            if (src == QLatin1String("apt") || src == QLatin1String("dnf")
                || src == QLatin1String("pacman") || src == QLatin1String("zypper")) {
                mRefresh->runUpgradeAll(src);
                return;
            }
        }
        // Fallback: pick the first source seen.
        if (mUpdatesTree->topLevelItemCount() > 0)
            mRefresh->runUpgradeAll(mUpdatesTree->topLevelItem(0)->text(0));
    });
    connect(mRefresh, &DataRefreshService::upgradeStarted,
            this, &APTSourceManagerPage::onUpgradeStarted);
    connect(mRefresh, &DataRefreshService::upgradeFinished,
            this, &APTSourceManagerPage::onUpgradeFinished);

    // --- Health Dashboard: QSplitter + Detail Panel ---
    mSplitter = new QSplitter(Qt::Horizontal, this);
    mSplitter->setChildrenCollapsible(false);

    // Reparent the entire content widget (title + search + list + buttons) into the splitter.
    // verticalWidget contains the grid with all page content; verticalWidget_2 is just the list area inside it.
    // We must move verticalWidget so the title/search/buttons stay with the list.
    mSplitter->addWidget(ui->verticalWidget);

    mDetailPanel = new RepoDetailPanel(this);
    mDetailPanel->hide();
    mDetailPanel->setMinimumWidth(250);
    mSplitter->addWidget(mDetailPanel);

    mSplitter->setSizes({600, 400});
    mSplitter->setStretchFactor(0, 1);
    mSplitter->setStretchFactor(1, 0);

    ui->verticalLayout_2->addWidget(mSplitter);

    connect(mDetailPanel, &RepoDetailPanel::closeRequested,
            this, &APTSourceManagerPage::onDetailPanelCloseRequested);
    connect(mDetailPanel, &RepoDetailPanel::repairActionRequested,
            this, &APTSourceManagerPage::onRepairActionRequested);
    connect(mDetailPanel, &RepoDetailPanel::editRequested,
            this, [this](const APTSourcePtr &src) {
        selectedAptSource = src;
        on_btnEditAptSource_clicked();
    });
    connect(mDetailPanel, &RepoDetailPanel::disableRequested,
            this, [this](const APTSourcePtr &src) {
        mToolManager->changeAPTStatus(src, !src->isActive);
        loadAptSources();
    });

    connect(mBtnRefreshHealth, &QPushButton::clicked, this, [this]() {
        mBtnRefreshHealth->setEnabled(false);
        mBtnRefreshHealth->setText(tr("Checking..."));
        mRefresh->triggerRepoHealthCheck();
    });

    connect(mRefresh, &DataRefreshService::repoHealthChecked,
            this, &APTSourceManagerPage::onRepoHealthChecked);

    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, [this](const UpdateCheckResult &) {
        mRefresh->triggerRepoHealthCheck();
    });

    // BUG-110: this page is constructed lazily (FR-97) after DataRefreshService
    // has already fired its startup systemUpdatesChecked and, downstream of it,
    // repoHealthChecked. Backfill from the cached results so the updates table
    // and health dashboard populate immediately instead of waiting for the next
    // hourly tick or a manual Check Now.
    if (mRefresh->hasLastUpdateCheckResult())
        onSystemUpdatesChecked(mRefresh->lastUpdateCheckResult());
    if (mRefresh->hasLastRepoHealthCache())
        onRepoHealthChecked(mRefresh->lastRepoHealthCache());

    if (mToolManager->packageTool()->currentPackageTool == APT_RPM) {
        ui->txtAptSource->setPlaceholderText(tr("example %1")
            .arg("'rpm [p10] http://mirror.yandex.ru/altlinux/ p10/branch/x86_64-i586 classic'"));
    } else {
        ui->txtAptSource->setPlaceholderText(tr("example %1")
            .arg("'ppa:deadsnakes/ppa'"));
    }

    loadAptSources();

    on_btnCancel_clicked();

    QList<QWidget*> widgets = {
        ui->btnAddAPTSourceRepository, ui->btnCancel, ui->btnDeleteAptSource,
        ui->txtSearchAptSource, ui->txtSearchAptSource, ui->btnEditAptSource
    };
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

    ui->lblAptSourceTitle->setText(tr("APT Repositories (%1)")
                                   .arg(aptSourceList.count()));

    // Refresh health after any source list change
    mRefresh->triggerRepoHealthCheck();
}

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

            mToolManager->addRepository(aptSourceRepository, ui->checkEnableSource->isChecked());

            ui->txtAptSource->clear();
            ui->checkEnableSource->setChecked(false);
            ui->btnAddAPTSourceRepository->setEnabled(true);
            on_btnCancel_clicked();
            selectedAptSource.clear();
            loadAptSources();
            on_txtSearchAptSource_textChanged(ui->txtSearchAptSource->text());
        }
    }
}

void APTSourceManagerPage::on_btnCancel_clicked()
{
    ui->btnAddAPTSourceRepository->setChecked(false);
    changeElementsVisible(false);
    ui->btnAddAPTSourceRepository->setText(tr("Add Repository"));
}

void APTSourceManagerPage::changeElementsVisible(const bool checked)
{
    ui->txtAptSource->setVisible(checked);
    ui->btnCancel->setVisible(checked);
    ui->btnDeleteAptSource->setVisible(!checked);
    ui->bottomSectionHorizontalSpacer->changeSize(0, 0, checked ? QSizePolicy::Minimum : QSizePolicy::Expanding);
    ui->checkEnableSource->setVisible(checked);
    ui->btnEditAptSource->setVisible(!checked);
}

void APTSourceManagerPage::on_listWidgetAptSources_itemClicked(QListWidgetItem *item)
{
    QWidget *widget = ui->listWidgetAptSources->itemWidget(item);
    if (!widget) {
        selectedAptSource.clear();
        mDetailPanel->clear();
        return;
    }

    APTSourceRepositoryItem *aptSourceItem = dynamic_cast<APTSourceRepositoryItem*>(widget);
    if (!aptSourceItem) {
        selectedAptSource.clear();
        mDetailPanel->clear();
        return;
    }

    APTSourcePtr clickedSource = aptSourceItem->aptSource();

    // Toggle: clicking the same repo again closes the panel
    if (selectedAptSource == clickedSource && mDetailPanel->isVisible()) {
        mDetailPanel->clear();
        selectedAptSource.clear();
        return;
    }

    selectedAptSource = clickedSource;
    QString key = RepoHealthChecker::cacheKey(selectedAptSource);
    if (mHealthCache.contains(key)) {
        mDetailPanel->showRepo(selectedAptSource, mHealthCache[key]);
    } else {
        RepoHealthResult placeholder;
        placeholder.status = RepoHealthResult::Unknown;
        RepoKnownInfo known = RepoKnowledgeBase::lookup(selectedAptSource->uri);
        placeholder.name = known.name.isEmpty() ? RepoKnowledgeBase::domainFromUri(selectedAptSource->uri) : known.name;
        placeholder.description = known.description;
        mDetailPanel->showRepo(selectedAptSource, placeholder);
    }
}

void APTSourceManagerPage::on_listWidgetAptSources_itemDoubleClicked(QListWidgetItem *item)
{
    on_listWidgetAptSources_itemClicked(item);
    on_btnEditAptSource_clicked();
}

void APTSourceManagerPage::on_btnDeleteAptSource_clicked()
{
    if (! selectedAptSource.isNull()) {
        mToolManager->removeAPTSource(selectedAptSource);
        selectedAptSource.clear();
        loadAptSources();
        on_txtSearchAptSource_textChanged(ui->txtSearchAptSource->text());
    }
}

void APTSourceManagerPage::on_txtSearchAptSource_textChanged(const QString &val)
{
    for (int i = 0; i < ui->listWidgetAptSources->count(); ++i) {
        QListWidgetItem *item = ui->listWidgetAptSources->item(i);
        if (item) {
            bool isContain = item->data(5).toString().contains(val, Qt::CaseInsensitive);
            item->setHidden(! isContain);
        }
    }
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

        // SSO-3741: per-row Upgrade button. Captured by value so the lambda
        // outlives the QTreeWidgetItem if the user clicks during a re-render.
        auto *btn = new QPushButton(tr("Upgrade"), mUpdatesTree);
        btn->setObjectName("btnUpgrade");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(24);
        UpdateEntry captured = entry;
        connect(btn, &QPushButton::clicked, this, [this, captured]() {
            mRefresh->runUpgrade(captured);
        });
        mUpdatesTree->setItemWidget(item, 3, btn);
    }

    mUpdatesSection->show();
}

void APTSourceManagerPage::onUpgradeStarted(const QString &label)
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

void APTSourceManagerPage::onUpgradeFinished(const QString &label, bool ok, const QString &error)
{
    mLblUpgradeProgress->hide();
    mBtnUpgradeAll->setEnabled(true);
    mBtnCheckNow->setEnabled(true);
    if (!ok && !error.isEmpty()) {
        QMessageBox::warning(this, tr("Upgrade Failed"),
            tr("%1 did not complete:\n\n%2").arg(label, error));
    }
    // The subsequent systemUpdatesChecked emit (queued by runUpgrade
    // completion) will refresh the tree, including re-enabling row buttons.
}

void APTSourceManagerPage::onRepoHealthChecked(const RepoHealthCache &cache)
{
    mHealthCache = cache;

    mBtnRefreshHealth->setEnabled(true);
    mBtnRefreshHealth->setText(tr("Refresh Health"));

    for (int i = 0; i < ui->listWidgetAptSources->count(); ++i) {
        QListWidgetItem *listItem = ui->listWidgetAptSources->item(i);
        QWidget *widget = ui->listWidgetAptSources->itemWidget(listItem);
        if (!widget) continue;

        APTSourceRepositoryItem *cardItem = dynamic_cast<APTSourceRepositoryItem*>(widget);
        if (!cardItem) continue;

        QString key = RepoHealthChecker::cacheKey(cardItem->aptSource());
        if (cache.contains(key))
            cardItem->setHealthResult(cache[key]);
    }

    if (mDetailPanel->isVisible() && !selectedAptSource.isNull()) {
        QString key = RepoHealthChecker::cacheKey(selectedAptSource);
        if (cache.contains(key))
            mDetailPanel->showRepo(selectedAptSource, cache[key]);
    }
}

void APTSourceManagerPage::onDetailPanelCloseRequested()
{
    mDetailPanel->hide();
    selectedAptSource.clear();
}

void APTSourceManagerPage::onRepairActionRequested(const RepoRepairAction &action, const APTSourcePtr &source)
{
    RepoRepairEngine *engine = mToolManager->repoRepairEngine();

    if (action.type == RepoRepairAction::DiagnoseConnection) {
        if (mDiagnoseRunning) return;
        mDiagnoseRunning = true;
        connect(engine, &RepoRepairEngine::diagnoseFinished,
                this, &APTSourceManagerPage::onDiagnoseFinished, Qt::UniqueConnection);
        QtConcurrent::run([engine, source]() {
            engine->diagnoseConnection(source);
        });
        return;
    }

    // "Ask Claude.ai" — copy prompt to clipboard and show modal
    if (action.type == RepoRepairAction::AskClaude) {
        QString prompt = action.context.value("prompt").toString();

        // Append health issues from the current detail panel result
        if (mDetailPanel && mDetailPanel->isVisible()) {
            RepoHealthResult healthResult = mDetailPanel->currentResult();
            if (!healthResult.issues.isEmpty()) {
                prompt += "\n\nHealth check issues:\n";
                for (const RepoHealthIssue &issue : healthResult.issues)
                    prompt += QString("- %1: %2\n").arg(issue.summary, issue.detail);
            }
        }

        QGuiApplication::clipboard()->setText(prompt);

        QDialog dlg(this);
        dlg.setWindowTitle(tr("Ask Claude.ai"));
        dlg.setMinimumWidth(420);
        auto *layout = new QVBoxLayout(&dlg);
        layout->setSpacing(12);
        layout->setContentsMargins(20, 20, 20, 20);

        auto *lblMessage = new QLabel(
            tr("A question about this repository issue has been copied "
               "to your clipboard.\n\n"
               "Paste it into Claude to get personalized help."));
        lblMessage->setWordWrap(true);
        layout->addWidget(lblMessage);

        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(8);

        auto *btnOpen = new QPushButton(tr("Open Claude.ai"));
        btnOpen->setCursor(Qt::PointingHandCursor);
        connect(btnOpen, &QPushButton::clicked, &dlg, [&dlg]() {
            QDesktopServices::openUrl(QUrl("https://claude.ai/new"));
            dlg.accept();
        });
        btnRow->addWidget(btnOpen);

        auto *btnClose = new QPushButton(tr("Close"));
        btnClose->setCursor(Qt::PointingHandCursor);
        connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::reject);
        btnRow->addWidget(btnClose);

        btnRow->addStretch();
        layout->addLayout(btnRow);

        dlg.exec();
        return;
    }

    // "Open in Browser" — no pkexec needed
    if (action.type == RepoRepairAction::RunCommand) {
        if (action.command.startsWith("xdg-open") || action.command.startsWith("open")) {
            QStringList args = action.command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (args.size() >= 2)
                QDesktopServices::openUrl(QUrl(args.mid(1).join(' ')));
            return;
        }
    }

    // Confirmation dialog
    QString message;
    if (action.type == RepoRepairAction::RemoveSource) {
        message = tr("This will permanently delete this repository entry.\n\n"
                     "This action cannot be undone.\n\nProceed?");
    } else {
        message = tr("This will modify your system's repository configuration.\n\n"
                     "Action: %1\n\nThis requires administrator privileges. Proceed?")
            .arg(action.label);
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm: %1").arg(action.label),
        message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    RepoRepairEngine::RepairResult result;
    switch (action.type) {
    case RepoRepairAction::RunCommand:
        result = engine->runCommand(action.command);
        break;
    case RepoRepairAction::ConvertToDeb822:
        result = engine->convertToDeb822(source);
        break;
    case RepoRepairAction::RemoveDuplicate:
        result = engine->removeDuplicate(source);
        break;
    case RepoRepairAction::DisableSource:
        result = engine->disableSource(source);
        break;
    case RepoRepairAction::EnableSource:
        result = engine->enableSource(source);
        break;
    case RepoRepairAction::RemoveSource:
        result = engine->removeSource(source);
        break;
    default:
        return;
    }

    if (result.success) {
        loadAptSources();
        mRefresh->triggerRepoHealthCheck();
    } else if (!result.errorDetail.isEmpty()) {
        QMessageBox::warning(this, tr("Action Failed"),
            tr("%1\n\n%2").arg(result.message, result.errorDetail));
    }
}

void APTSourceManagerPage::onDiagnoseFinished(const DiagnoseResult &result)
{
    mDiagnoseRunning = false;
    mLastDiagnoseResult = result;
    mHasDiagnoseResult = true;
    if (mDetailPanel->isVisible() && selectedAptSource) {
        QString key = RepoHealthChecker::cacheKey(selectedAptSource);
        if (mHealthCache.contains(key))
            mDetailPanel->showRepo(selectedAptSource, mHealthCache[key], &result);
    }
}
