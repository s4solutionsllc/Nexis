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
#include <QFrame>
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
    updLayout->setSpacing(8);

    // DS §3 header anatomy (NEX F2 shared recipe): accent bar + title row
    // (title, Check Now, Refresh Health) / source line — mirrors
    // HomebrewPage::buildUI() (homebrew_page.cpp:83-134).
    QWidget *updHeaderWidget = new QWidget(mUpdatesSection);
    updHeaderWidget->setObjectName("sectionHeaderRow");
    QVBoxLayout *updHeaderRoot = new QVBoxLayout(updHeaderWidget);
    updHeaderRoot->setContentsMargins(0, 0, 0, 0);
    updHeaderRoot->setSpacing(2);

    QHBoxLayout *updHeaderRow = new QHBoxLayout();
    updHeaderRow->setContentsMargins(0, 0, 0, 0);
    updHeaderRow->setSpacing(8);

    QFrame *updAccentBar = new QFrame(updHeaderWidget);
    updAccentBar->setObjectName("sectionHeaderAccent");
    updAccentBar->setProperty("accentToken", "accent");
    updAccentBar->setFixedWidth(3);
    updAccentBar->setMinimumHeight(26);
    updAccentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    updHeaderRow->addWidget(updAccentBar);

    QVBoxLayout *updTextCol = new QVBoxLayout();
    updTextCol->setContentsMargins(0, 0, 0, 0);
    updTextCol->setSpacing(2);

    QHBoxLayout *updTitleRow = new QHBoxLayout();
    updTitleRow->setContentsMargins(0, 0, 0, 0);
    updTitleRow->setSpacing(8);

    mLblUpdatesTitle = new QLabel(tr("Available Updates"), updHeaderWidget);
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

    mBtnRefreshHealth = new QPushButton(tr("Refresh Health"), updHeaderWidget);
    mBtnRefreshHealth->setObjectName("btnRefreshHealth");
    mBtnRefreshHealth->setCursor(Qt::PointingHandCursor);
    mBtnRefreshHealth->setFocusPolicy(Qt::NoFocus);
    mBtnRefreshHealth->setAccessibleName("primary");
    mBtnRefreshHealth->setFixedHeight(28);
    updTitleRow->addWidget(mBtnRefreshHealth);

    updTextCol->addLayout(updTitleRow);

    QLabel *lblUpdatesSource = new QLabel(tr("apt update / apt list --upgradable, on demand or hourly"), updHeaderWidget);
    lblUpdatesSource->setObjectName("sectionHeaderSource");
    updTextCol->addWidget(lblUpdatesSource);

    updHeaderRow->addLayout(updTextCol, 1);
    updHeaderRoot->addLayout(updHeaderRow);

    updLayout->addWidget(updHeaderWidget);

    // DS §2 elevated container (NEX F1 shared recipe) — single
    // container-level shadow (DS §7); the tree rows stay flat inside it.
    QWidget *updContainer = new QWidget(mUpdatesSection);
    updContainer->setObjectName("aptUpdatesContainer");
    updContainer->setAttribute(Qt::WA_StyledBackground, true);
    updContainer->setProperty("cardRole", "elevated");
    QVBoxLayout *updContainerLayout = new QVBoxLayout(updContainer);
    updContainerLayout->setContentsMargins(0, 0, 0, 0);
    updContainerLayout->setSpacing(0);

    // Updates tree widget: Source | Package | Version
    mUpdatesTree = new QTreeWidget(updContainer);
    mUpdatesTree->setObjectName("treeWidgetUpdates");
    mUpdatesTree->setHeaderHidden(false);
    mUpdatesTree->setHeaderLabels({ tr("Source"), tr("Package"), tr("Version") });
    mUpdatesTree->header()->setFixedHeight(Dpi::scale(30));
    mUpdatesTree->setColumnCount(3);
    mUpdatesTree->setRootIsDecorated(false);
    // SSO-3502: read-only data display (NoSelection + NoEditTriggers); skipped
    // by tab order because nothing focusable lives inside.
    mUpdatesTree->setFocusPolicy(Qt::NoFocus);
    mUpdatesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mUpdatesTree->setSelectionMode(QAbstractItemView::NoSelection);
    mUpdatesTree->header()->setStretchLastSection(true);
    mUpdatesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mUpdatesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mUpdatesTree->setMaximumHeight(200);
    updContainerLayout->addWidget(mUpdatesTree);

    updLayout->addWidget(updContainer, 1);

    // Insert updates section into page layout BEFORE the main content
    ui->verticalLayout_2->insertWidget(0, mUpdatesSection);

    // DS §2/§7: one shadow per elevated container, never per row.
    Utilities::addDropShadow(updContainer, 90, 26);

    // Wire signal and button
    connect(mBtnCheckNow, &QPushButton::clicked, this, [this]() {
        mBtnCheckNow->setEnabled(false);
        mBtnCheckNow->setText(tr("Checking..."));
        mRefresh->triggerUpdateCheck();
    });
    connect(mRefresh, &DataRefreshService::systemUpdatesChecked,
            this, &APTSourceManagerPage::onSystemUpdatesChecked);

    // --- Health Dashboard: QSplitter + Detail Panel ---
    mSplitter = new QSplitter(Qt::Horizontal, this);
    mSplitter->setChildrenCollapsible(false);

    // Reparent the entire content widget (title + search + list + buttons) into the splitter.
    // verticalWidget contains the grid with all page content; aptSourcesContainer is just the list area inside it.
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
        ui->txtSearchAptSource, ui->btnEditAptSource
    };
    Utilities::addDropShadow(widgets, 40);

    // DS §2/§7 (SSO-15096): repository list rendered as a GNOME-style boxed
    // list (flat @cardBg rows, apt_source_repository_item.cpp) inside a
    // single elevated container — shadow lives here, never per row.
    ui->aptSourcesContainer->setAttribute(Qt::WA_StyledBackground, true);
    Utilities::addDropShadow(ui->aptSourcesContainer, 90, 26);
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

    ui->sectionHeaderTitle->setText(tr("APT Repositories (%1)")
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
    }

    mUpdatesSection->show();
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
