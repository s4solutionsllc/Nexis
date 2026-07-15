#include "system_cleaner_page.h"
#include "ui_system_cleaner_page.h"
#include "byte_tree_widget.h"

#include <cstdlib>
#include "nexis_roles.h"
#include "dpi.h"
#include <Managers/schedule_manager.h>
#include <Managers/tool_manager.h>
#include "signal_mapper.h"
#include <Utils/format_util.h>
#include "exclusion_manager_dialog.h"
#include "schedule_editor_dialog.h"
#include "utilities.h"
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QToolButton>
#include <QPushButton>
#include <QScrollArea>
#include <QProgressBar>
#include <QMenu>
#include <QHeaderView>
#include <QScrollBar>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>

SystemCleanerPage::~SystemCleanerPage()
{
    mWorkerFuture.waitForFinished();
    delete ui;
}

SystemCleanerPage::SystemCleanerPage(QWidget *parent, AppManager *appManager,
                                     SignalMapper *signalMapper, CleanerService *cleanerService,
                                     ScheduleManager *scheduleManager) :
    QWidget(parent),
    ui(new Ui::SystemCleanerPage),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mCleanerService(cleanerService ? cleanerService : CleanerService::ins()),
    mScheduleManager(scheduleManager ? scheduleManager : ScheduleManager::ins()),
    mDefaultIcon(QIcon(":/static/themes/common/img/package.png"))
{
    ui->setupUi(this);
    init();
    ui->stackedWidget->setCurrentIndex(0);
}

void SystemCleanerPage::init()
{
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme, this, [this] {
        QString themeName = mAppManager->resolveThemeName();
        if (mBtnExclusions) {
            mBtnExclusions->setIcon(QIcon(
                QString(":/static/themes/%1/img/sidebar-icons/settings.svg")
                    .arg(themeName)));
        }
        if (mScanProgress) {
            mScanProgress->style()->unpolish(mScanProgress);
            mScanProgress->style()->polish(mScanProgress);
        }
    });

    qRegisterMetaType<QList<QPersistentModelIndex>>();
    qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>();
    qRegisterMetaType<Qt::SortOrder>();

    connect(this, &SystemCleanerPage::scanFinishedS, this, &SystemCleanerPage::onScanFinished);
    connect(this, &SystemCleanerPage::cleanFinishedS, this, &SystemCleanerPage::onCleanFinished);
    connect(mCleanerService, &CleanerService::snapshotTaken,
            this, &SystemCleanerPage::onSnapshotTaken,
            Qt::QueuedConnection);
    // SSO-3732 / FW-05: queued so the signal — emitted from the cleaner's
    // QtConcurrent worker thread when QFile::remove hits TCC denial — is
    // marshalled back to the UI thread before the banner is shown.
    connect(mCleanerService, &CleanerService::accessNeededDetected,
            this, &SystemCleanerPage::onAccessNeededDetected,
            Qt::QueuedConnection);

    ui->treeWidgetScanResult->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeWidgetScanResult, &QTreeWidget::customContextMenuRequested,
            this, &SystemCleanerPage::onTreeContextMenu);

    ui->treeWidgetScanResult->setColumnCount(2);
    ui->treeWidgetScanResult->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeWidgetScanResult->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->treeWidgetScanResult->setColumnWidth(1, Dpi::scale(100));
    ui->treeWidgetScanResult->header()->setFixedHeight(Dpi::scale(30));
    ui->treeWidgetScanResult->setHeaderLabels({ tr("File Name"), tr("Size") });

    buildCategoryHeader();
    buildCategoryCards();
    buildInlineResults();
    buildCleanerFooter();
    initScheduleIndicator();
}

// ─── Page header: title + subtitle + Schedule… + Scan system + Exclusions ────

void SystemCleanerPage::buildCategoryHeader()
{
    QVBoxLayout *catLayout = qobject_cast<QVBoxLayout *>(ui->cleanerCategories->layout());
    Q_ASSERT(catLayout);

    // DS §3 header anatomy (NEX F2 shared recipe): accent bar + [title row
    // (title, gear/Schedule…/Scan system) / source line], mirroring
    // MetricTileBase::buildChrome() (metric_tile_base.cpp:255-307).
    QWidget *headerWidget = new QWidget(ui->cleanerCategories);
    headerWidget->setObjectName("sectionHeaderRow");
    QVBoxLayout *root = new QVBoxLayout(headerWidget);
    root->setContentsMargins(14, 12, 14, 10);
    root->setSpacing(6);

    QHBoxLayout *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);

    QFrame *accentBar = new QFrame(headerWidget);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("accentToken", "success");
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(26);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    headerRow->addWidget(accentBar);

    QVBoxLayout *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    QHBoxLayout *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    mLblCleanerTitle = new QLabel(tr("System Cleaner"), headerWidget);
    mLblCleanerTitle->setObjectName("sectionHeaderTitle");
    titleRow->addWidget(mLblCleanerTitle);
    titleRow->addStretch();

    // Exclusions gear, Schedule…, Scan system — kept as the page's existing
    // trailing controls rather than the single #sectionHeaderAction slot
    // (this page keeps three controls, per notes/system_cleaner.md).
    mBtnExclusions = new QToolButton(headerWidget);
    mBtnExclusions->setAutoRaise(true);
    mBtnExclusions->setIcon(QIcon(
        QString(":/static/themes/%1/img/sidebar-icons/settings.svg")
            .arg(mAppManager->resolveThemeName())));
    mBtnExclusions->setIconSize(Dpi::scale(16, 16));
    mBtnExclusions->setFixedSize(Dpi::scale(28, 28));
    mBtnExclusions->setToolTip(tr("Manage exclusion rules"));
    mBtnExclusions->setCursor(Qt::PointingHandCursor);
    connect(mBtnExclusions, &QToolButton::clicked, this, &SystemCleanerPage::onManageExclusions);

    mBtnSchedule = new QPushButton(tr("Schedule\u2026"), headerWidget);
    mBtnSchedule->setObjectName("btnScheduleCleaner");
    mBtnSchedule->setCursor(Qt::PointingHandCursor);
    connect(mBtnSchedule, &QPushButton::clicked, this, [this] {
        ScheduleEditorDialog *dlg = new ScheduleEditorDialog(this);
        connect(dlg, &ScheduleEditorDialog::scheduleCreated, this, [this](const ScheduleManager::CleaningSchedule &s) {
            mScheduleManager->createSchedule(s);
            updateScheduleIndicator();
        });
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    mBtnScanSystem = new QPushButton(tr("Scan system"), headerWidget);
    mBtnScanSystem->setObjectName("btnScanSystem");
    mBtnScanSystem->setCursor(Qt::PointingHandCursor);
    connect(mBtnScanSystem, &QPushButton::clicked, this, &SystemCleanerPage::onBtnScanSystemClicked);

    titleRow->addWidget(mBtnExclusions);
    titleRow->addWidget(mBtnSchedule);
    titleRow->addWidget(mBtnScanSystem);

    textCol->addLayout(titleRow);

    QLabel *lblSubtitle = new QLabel(tr("Reclaim disk space by removing caches, logs, and crash reports."), headerWidget);
    lblSubtitle->setObjectName("sectionHeaderSource");
    textCol->addWidget(lblSubtitle);

    headerRow->addLayout(textCol, 1);
    root->addLayout(headerRow);

    catLayout->addWidget(headerWidget);
}

// ─── Two-column card grid ─────────────────────────────────────────────────────

void SystemCleanerPage::buildCategoryCards()
{
    QVBoxLayout *catLayout = qobject_cast<QVBoxLayout *>(ui->cleanerCategories->layout());
    Q_ASSERT(catLayout);

    struct CatDef {
        CleanCategories cat;
        QString name;
        QString subtitle;
    };

    const QList<CatDef> defs = {
        { PACKAGE_CACHE,
#ifdef Q_OS_MACOS
          tr("Package Caches"), QStringLiteral("brew \u00b7 ~/Library/Caches (brew)")
#else
          tr("Package Caches"), QStringLiteral("apt \u00b7 dnf \u00b7 pacman \u00b7 zypper")
#endif
        },
        { CRASH_REPORTS,
#ifdef Q_OS_MACOS
          tr("Crash Reports"), QStringLiteral("~/Library/Logs/DiagnosticReports")
#else
          tr("Crash Reports"), QStringLiteral("/var/crash \u00b7 ~/.xsession-errors")
#endif
        },
        { APPLICATION_LOGS,
#ifdef Q_OS_MACOS
          tr("Application Logs"), QStringLiteral("~/Library/Logs \u00b7 /var/log")
#else
          tr("Application Logs"), QStringLiteral("journald \u00b7 ~/.cache/*.log")
#endif
        },
        { APPLICATION_CACHES,
#ifdef Q_OS_MACOS
          tr("Application Caches"), QStringLiteral("~/Library/Caches")
#else
          tr("Application Caches"), QStringLiteral("~/.cache/*")
#endif
        },
        { TRASH,
#ifdef Q_OS_MACOS
          tr("Trash"), QStringLiteral("~/.Trash")
#else
          tr("Trash"), QStringLiteral("~/.local/share/Trash")
#endif
        },
        { DEV_TOOL_CACHES,
          tr("Dev Tool Caches"), QStringLiteral("~/.electron \u00b7 ~/.cache/vscode*")
        },
        { BROKEN_SYMLINKS,
          tr("Broken Symlinks"), QStringLiteral("~/ recursive symlink scan")
        },
        { BROWSER_PRIVACY,
          tr("Browser Privacy"), QStringLiteral("Chrome \u00b7 Firefox \u00b7 Chromium \u00b7 Safari")
        },
    };

    // Pre-size mCards so enum-indexed access is safe
    mCards.resize(SNAP_FLATPAK_REVISIONS + 1);

    // GH#55 / SSO-355: cards must scroll inside their own region so the page
    // fits in windows smaller than the FR-130 design size (1025×736). Give the
    // scroll area a small minimum height — the parent layout can then shrink
    // it and the vertical scrollbar engages instead of the page overflowing.
    QScrollArea *scrollArea = new QScrollArea(ui->cleanerCategories);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
    scrollArea->setMinimumHeight(Dpi::scale(160));
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget *container = new QWidget;
    container->setStyleSheet("background-color:transparent;");
    container->setObjectName("cleanerCardsContainer");
    QGridLayout *grid = new QGridLayout(container);
    grid->setContentsMargins(0, 0, 0, 8);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    int index = 0;
    auto addCard = [&](const CatDef &def) {
        QFrame *card = new QFrame(container);
        card->setObjectName("cleanerCategoryCard");
        card->setProperty("checked", false);
        // DS §2 elevated tile (NEX F1 shared recipe) — container-level shadow
        // only (DS §7); rows/labels inside stay flat.
        card->setProperty("cardRole", "elevated");
        Utilities::addDropShadow(card, 90, 26);

        QCheckBox *check = new QCheckBox(card);
        check->setCursor(Qt::PointingHandCursor);

        QLabel *lblName = new QLabel(def.name, card);
        lblName->setObjectName("lblCatName");

        QLabel *lblSubtitle = new QLabel(def.subtitle, card);
        lblSubtitle->setObjectName("lblCatSubtitle");

        QLabel *lblSize = new QLabel(QStringLiteral("\u2014"), card);
        lblSize->setObjectName("lblCatSize");
        lblSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblSize->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        QVBoxLayout *textCol = new QVBoxLayout;
        textCol->setSpacing(1);
        textCol->addWidget(lblName);
        textCol->addWidget(lblSubtitle);

        QHBoxLayout *cardRow = new QHBoxLayout(card);
        cardRow->setContentsMargins(12, 8, 12, 8);
        cardRow->setSpacing(10);
        cardRow->addWidget(check, 0, Qt::AlignVCenter);
        cardRow->addLayout(textCol, 1);
        cardRow->addWidget(lblSize, 0, Qt::AlignVCenter);

        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        connect(check, &QCheckBox::toggled, this, [this, card, def](bool on) {
            card->setProperty("checked", on);
            card->style()->unpolish(card);
            card->style()->polish(card);
            // GH-226: skip the expensive rebuild during onSelectAllClicked(); it
            // will call these once after all checkboxes are set.
            if (mBulkCategoryUpdate) return;
            if (mHasScanned) refreshInlineTree();
            updateFooterTotal();
            updateCleanerCheckBadge();
        });

        CategoryCard cc;
        cc.frame   = card;
        cc.check   = check;
        cc.lblSize = lblSize;
        mCards[def.cat] = cc;

        int row = index / 2;
        int col = index % 2;
        grid->addWidget(card, row, col);
        ++index;
    };

    for (const CatDef &def : defs)
        addCard(def);

#ifndef Q_OS_MACOS
    // Snap/Flatpak — Linux only
    CatDef snapDef { SNAP_FLATPAK_REVISIONS,
                     tr("Snap/Flatpak Revisions"),
                     QStringLiteral("snap revisions \u00b7 flatpak unused runtimes") };
    addCard(snapDef);
    mCheckSnapFlatpak = mCards[SNAP_FLATPAK_REVISIONS].check;
#endif

    // Push cards to top — empty stretch row below the last card row absorbs extra space
    grid->setRowStretch((index + 1) / 2, 1);

    scrollArea->setWidget(container);

    // Indeterminate progress bar shown during scan (matches duplicate finder pattern)
    mScanProgress = new QProgressBar(ui->cleanerCategories);
    mScanProgress->setObjectName("cleanerScanProgress");
    mScanProgress->setTextVisible(false);
    mScanProgress->setFixedHeight(Dpi::scale(4));
    mScanProgress->hide();

    catLayout->addWidget(mScanProgress);
    // Stretch 1 so the scroll area absorbs available vertical space and
    // compresses (rather than overflowing) when the window is small.
    catLayout->addWidget(scrollArea, 1);
}

// ─── Inline results (reparented from stacked widget page 1) ──────────────────

void SystemCleanerPage::buildInlineResults()
{
    QVBoxLayout *catLayout = qobject_cast<QVBoxLayout *>(ui->cleanerCategories->layout());
    Q_ASSERT(catLayout);

    // Reparent cleanerPage out of the stacked widget so it lives inline in
    // catLayout, below the cards. The stacked widget retains only cleanerCategories.
    ui->cleanerPage->setParent(ui->cleanerCategories);
    ui->cleanerPage->hide();
    catLayout->addWidget(ui->cleanerPage, 1); // stretch 1 — fills remaining space when visible
}

// ─── Footer bar ──────────────────────────────────────────────────────────────

void SystemCleanerPage::buildCleanerFooter()
{
    QVBoxLayout *catLayout = qobject_cast<QVBoxLayout *>(ui->cleanerCategories->layout());
    Q_ASSERT(catLayout);

    mCleanerFooter = new QFrame(ui->cleanerCategories);
    mCleanerFooter->setObjectName("cleanerFooter");

    QVBoxLayout *outerLayout = new QVBoxLayout(mCleanerFooter);
    outerLayout->setContentsMargins(14, 10, 14, 10);
    outerLayout->setSpacing(8);

    // Main row: estimated size + clean button
    QHBoxLayout *footerRow = new QHBoxLayout;
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(12);

    QLabel *lblEstimatedLabel = new QLabel(tr("ESTIMATED RECOVERABLE"), mCleanerFooter);
    lblEstimatedLabel->setObjectName("lblEstimatedLabel");

    mLblEstimated = new QLabel(QStringLiteral("\u2014"), mCleanerFooter);
    mLblEstimated->setObjectName("lblEstimatedSize");

    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->setSpacing(2);
    leftCol->addWidget(lblEstimatedLabel);
    leftCol->addWidget(mLblEstimated);

    footerRow->addLayout(leftCol, 1);

    // GH#55 / SSO-355: restore the "Select All" affordance dropped in the
    // FR-130 redesign. Toggles every category card's checkbox in one click;
    // label flips to "Clear All" when every card is already checked.
    mBtnSelectAll = new QPushButton(tr("Select All"), mCleanerFooter);
    mBtnSelectAll->setObjectName("btnSelectAll");
    mBtnSelectAll->setCursor(Qt::PointingHandCursor);
    connect(mBtnSelectAll, &QPushButton::clicked,
            this, &SystemCleanerPage::onSelectAllClicked);
    footerRow->addWidget(mBtnSelectAll);

    mBtnCleanSelected = new QPushButton(tr("Clean selected"), mCleanerFooter);
    mBtnCleanSelected->setObjectName("btnCleanSelected");
    mBtnCleanSelected->setCursor(Qt::PointingHandCursor);
    mBtnCleanSelected->setEnabled(false);
    connect(mBtnCleanSelected, &QPushButton::clicked,
            this, &SystemCleanerPage::quickCleanByCategory);

    footerRow->addWidget(mBtnCleanSelected);
    outerLayout->addLayout(footerRow);

    // Toast: appears briefly after a successful pre-clean snapshot (NEX-281)
    mLblSnapshotToast = new QLabel(mCleanerFooter);
    mLblSnapshotToast->setObjectName("lblSnapshotToast");
    mLblSnapshotToast->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mLblSnapshotToast->hide();
    outerLayout->addWidget(mLblSnapshotToast);

    // SSO-3732 / FW-05: persistent banner for macOS 27 Privacy & Security
    // denial. Hidden by default; populated by onAccessNeededDetected() from
    // the CleanerService::accessNeededDetected signal. Word-wrapped + rich
    // text so the embedded link routes through QDesktopServices.
    mLblAccessNeeded = new QLabel(mCleanerFooter);
    mLblAccessNeeded->setObjectName("lblAccessNeeded");
    mLblAccessNeeded->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mLblAccessNeeded->setWordWrap(true);
    mLblAccessNeeded->setTextFormat(Qt::RichText);
    mLblAccessNeeded->setTextInteractionFlags(Qt::TextBrowserInteraction);
    mLblAccessNeeded->setOpenExternalLinks(false);
    connect(mLblAccessNeeded, &QLabel::linkActivated, this, [](const QString &url) {
        QDesktopServices::openUrl(QUrl(url));
    });
    mLblAccessNeeded->hide();
    outerLayout->addWidget(mLblAccessNeeded);

    catLayout->addWidget(mCleanerFooter);
}

// ─── Footer total calculation ─────────────────────────────────────────────────

void SystemCleanerPage::updateFooterTotal()
{
    quint64 total = 0;

    if (mHasScanned && ui->cleanerPage->isVisible()) {
        QTreeWidget *tree = ui->treeWidgetScanResult;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *root = tree->topLevelItem(i);
            CleanCategories cat = (CleanCategories) root->data(2, 0).toInt();
            if (cat == TRASH || cat == SNAP_FLATPAK_REVISIONS) {
                if (root->checkState(0) == Qt::Checked)
                    total += root->data(1, SortRole).toULongLong();
            } else {
                for (int j = 0; j < root->childCount(); ++j) {
                    if (root->child(j)->checkState(0) == Qt::Checked)
                        total += root->child(j)->data(1, SortRole).toULongLong();
                }
            }
        }
    } else {
        for (const CategoryCard &c : mCards) {
            if (c.check && c.check->isChecked())
                total += c.lastSize;
        }
    }

    if (mLblEstimated)
        mLblEstimated->setText((mHasScanned && total > 0)
            ? FormatUtil::formatBytes(total)
            : QStringLiteral("\u2014"));
    if (mBtnCleanSelected)
        mBtnCleanSelected->setEnabled(mHasScanned && total > 0);
}

// ─── Badge signal ────────────────────────────────────────────────────────────

void SystemCleanerPage::updateCleanerCheckBadge()
{
    int count = 0;
    int total = 0;
    for (const CategoryCard &c : mCards) {
        if (!c.check) continue;
        ++total;
        if (c.check->isChecked()) ++count;
    }
    emit checkedCategoryCountChanged(count);

    // GH#55 / SSO-355: when every card is already checked, the affordance
    // becomes "Clear All" so the user has a one-click escape hatch.
    if (mBtnSelectAll)
        mBtnSelectAll->setText((total > 0 && count == total) ? tr("Clear All") : tr("Select All"));
}

void SystemCleanerPage::onSelectAllClicked()
{
    bool allChecked = true;
    int total = 0;
    for (const CategoryCard &c : mCards) {
        if (!c.check) continue;
        ++total;
        if (!c.check->isChecked()) { allChecked = false; break; }
    }
    const bool target = !(total > 0 && allChecked); // false ⇒ clear, true ⇒ select

    // GH-226: guard against N concurrent refreshInlineTree() calls (one per
    // toggled signal). Each call clears and rebuilds the full tree — on systems
    // with large ~/.cache trees this causes an OOM kill. Set the flag so the
    // toggled lambda only updates card styling; do one shared rebuild after.
    mBulkCategoryUpdate = true;
    for (const CategoryCard &c : mCards) {
        if (c.check && c.check->isEnabled())
            c.check->setChecked(target);
    }
    mBulkCategoryUpdate = false;

    if (mHasScanned) refreshInlineTree();
    updateFooterTotal();
    updateCleanerCheckBadge();
}

// ─── Scan ─────────────────────────────────────────────────────────────────────

void SystemCleanerPage::onBtnScanSystemClicked()
{
    if (mScanInProgress || mCleanInProgress)
        return;

    // Always scan all categories — checkboxes control cleaning, not scanning
    mScanPackageCache   = true;
    mScanCrashReports   = true;
    mScanAppLog         = true;
    mScanAppCache       = true;
    mScanTrash          = true;
    mScanDevToolCache   = true;
    mScanBrokenSymlinks = true;
    mScanBrowserPrivacy = true;
    mScanSnapFlatpak    = (mCheckSnapFlatpak != nullptr);

    mLblPackageCacheText   = tr("Package Caches");
    mLblCrashReportsText   = tr("Crash Reports");
    mLblAppLogText         = tr("Application Logs");
    mLblAppCacheText       = tr("Application Caches");
    mLblTrashText          = tr("Trash");
    mLblDevToolCacheText   = tr("Dev Tool Caches");
    mLblBrokenSymlinksText = tr("Broken Symlinks");
    mLblBrowserPrivacyText = tr("Browser Privacy");
    mLblSnapFlatpakText    = tr("Snap/Flatpak Revisions");

    // Disable UI during scan
    mBtnScanSystem->setEnabled(false);
    mBtnSchedule->setEnabled(false);
    if (mBtnSelectAll) mBtnSelectAll->setEnabled(false);
    for (const CategoryCard &c : mCards)
        if (c.check) c.check->setEnabled(false);

    if (mScanProgress) { mScanProgress->setRange(0, 0); mScanProgress->show(); }

    mPackageCaches.clear();  mCrashReports.clear();
    mAppLogs.clear();        mAppCaches.clear();
    mDevToolCaches.clear();  mBrokenSymlinks.clear();
    mBrowserPrivacy.clear(); mSnapFlatpakRevisions.clear();

    mRetainedPackageCaches.clear();  mRetainedCrashReports.clear();
    mRetainedAppLogs.clear();        mRetainedAppCaches.clear();
    mRetainedDevToolCaches.clear();  mRetainedBrokenSymlinks.clear();
    mRetainedBrowserPrivacy.clear(); mRetainedSnapFlatpak.clear();

    mScanInProgress = true;
    mWorkerFuture = QtConcurrent::run([this]() { systemScan(); });
}

void SystemCleanerPage::startBackgroundSizeScan()
{
    mScanPackageCache   = true;
    mScanCrashReports   = true;
    mScanAppLog         = true;
    mScanAppCache       = true;
    mScanTrash          = true;
    mScanDevToolCache   = true;
    mScanBrokenSymlinks = true;
    mScanBrowserPrivacy = true;
    mScanSnapFlatpak    = (mCheckSnapFlatpak != nullptr);

    mLblPackageCacheText   = tr("Package Caches");
    mLblCrashReportsText   = tr("Crash Reports");
    mLblAppLogText         = tr("Application Logs");
    mLblAppCacheText       = tr("Application Caches");
    mLblTrashText          = tr("Trash");
    mLblDevToolCacheText   = tr("Dev Tool Caches");
    mLblBrokenSymlinksText = tr("Broken Symlinks");
    mLblBrowserPrivacyText = tr("Browser Privacy");
    mLblSnapFlatpakText    = tr("Snap/Flatpak Revisions");

    mInitialScan = true;

    // Disable action buttons but leave checkboxes interactive
    mBtnScanSystem->setEnabled(false);
    mBtnSchedule->setEnabled(false);

    if (mScanProgress) { mScanProgress->setRange(0, 0); mScanProgress->show(); }

    mPackageCaches.clear();  mCrashReports.clear();
    mAppLogs.clear();        mAppCaches.clear();
    mDevToolCaches.clear();  mBrokenSymlinks.clear();
    mBrowserPrivacy.clear(); mSnapFlatpakRevisions.clear();

    mRetainedPackageCaches.clear();  mRetainedCrashReports.clear();
    mRetainedAppLogs.clear();        mRetainedAppCaches.clear();
    mRetainedDevToolCaches.clear();  mRetainedBrokenSymlinks.clear();
    mRetainedBrowserPrivacy.clear(); mRetainedSnapFlatpak.clear();

    mScanInProgress = true;
    mWorkerFuture = QtConcurrent::run([this]() { systemScan(); });
}

void SystemCleanerPage::quickScan()
{
    if (mScanInProgress || mCleanInProgress)
        return;

    mScanPackageCache   = true;
    mScanCrashReports   = true;
    mScanAppLog         = true;
    mScanAppCache       = true;
    mScanTrash          = true;
    mScanDevToolCache   = true;
    mScanBrokenSymlinks = true;
    mScanBrowserPrivacy = true;
    mScanSnapFlatpak    = (mCheckSnapFlatpak != nullptr);

    mLblPackageCacheText   = tr("Package Caches");
    mLblCrashReportsText   = tr("Crash Reports");
    mLblAppLogText         = tr("Application Logs");
    mLblAppCacheText       = tr("Application Caches");
    mLblTrashText          = tr("Trash");
    mLblDevToolCacheText   = tr("Dev Tool Caches");
    mLblBrokenSymlinksText = tr("Broken Symlinks");
    mLblBrowserPrivacyText = tr("Browser Privacy");
    mLblSnapFlatpakText    = tr("Snap/Flatpak Revisions");

    // Check all cards
    for (const CategoryCard &c : mCards)
        if (c.check) c.check->setChecked(true);

    mBtnScanSystem->setEnabled(false);
    mBtnSchedule->setEnabled(false);
    if (mCleanerFooter) mCleanerFooter->hide();
    for (const CategoryCard &c : mCards)
        if (c.check) c.check->setEnabled(false);

    if (mScanProgress) { mScanProgress->setRange(0, 0); mScanProgress->show(); }

    mPackageCaches.clear(); mCrashReports.clear();
    mAppLogs.clear();       mAppCaches.clear();
    mDevToolCaches.clear(); mBrokenSymlinks.clear();
    mBrowserPrivacy.clear(); mSnapFlatpakRevisions.clear();

    mRetainedPackageCaches.clear(); mRetainedCrashReports.clear();
    mRetainedAppLogs.clear();       mRetainedAppCaches.clear();
    mRetainedDevToolCaches.clear(); mRetainedBrokenSymlinks.clear();
    mRetainedBrowserPrivacy.clear(); mRetainedSnapFlatpak.clear();

    mScanInProgress = true;
    mWorkerFuture = QtConcurrent::run([this]() { systemScan(); });
}

void SystemCleanerPage::systemScan()
{
    QList<CleanerService::CleanCategory> categories;
    if (mScanPackageCache)   categories << CleanerService::PACKAGE_CACHE;
    if (mScanCrashReports)   categories << CleanerService::CRASH_REPORTS;
    if (mScanAppLog)         categories << CleanerService::APPLICATION_LOGS;
    if (mScanAppCache)       categories << CleanerService::APPLICATION_CACHES;
    if (mScanDevToolCache)   categories << CleanerService::DEV_TOOL_CACHES;
    if (mScanBrokenSymlinks) categories << CleanerService::BROKEN_SYMLINKS;
    if (mScanBrowserPrivacy) categories << CleanerService::BROWSER_PRIVACY;
    if (mScanSnapFlatpak)    categories << CleanerService::SNAP_FLATPAK_REVISIONS;

    CleanerService::ScanResult result = mCleanerService->scan(categories);

    mPackageCaches        = result.categoryFiles.value(CleanerService::PACKAGE_CACHE);
    mCrashReports         = result.categoryFiles.value(CleanerService::CRASH_REPORTS);
    mAppLogs              = result.categoryFiles.value(CleanerService::APPLICATION_LOGS);
    mAppCaches            = result.categoryFiles.value(CleanerService::APPLICATION_CACHES);
    mDevToolCaches        = result.categoryFiles.value(CleanerService::DEV_TOOL_CACHES);
    mBrokenSymlinks       = result.categoryFiles.value(CleanerService::BROKEN_SYMLINKS);
    mBrowserPrivacy       = result.categoryFiles.value(CleanerService::BROWSER_PRIVACY);
    mSnapFlatpakRevisions = result.categoryFiles.value(CleanerService::SNAP_FLATPAK_REVISIONS);

    emit scanFinishedS();
}

void SystemCleanerPage::onScanFinished()
{
    if (mScanProgress) mScanProgress->hide();

    // ── Update card size labels ─────────────────────────────────────────────
    auto computeSize = [](const QFileInfoList &files) -> quint64 {
        quint64 total = 0;
        for (const QFileInfo &f : files)
            total += FileUtil::getFileSize(f.absoluteFilePath());
        return total;
    };
    auto updateCard = [this](CleanCategories cat, quint64 sz) {
        if (cat < mCards.size() && mCards[cat].lblSize) {
            mCards[cat].lblSize->setText(FormatUtil::formatBytes(sz));
            mCards[cat].lastSize = sz;
        }
    };

    if (mScanPackageCache)   updateCard(PACKAGE_CACHE,          computeSize(mPackageCaches));
    if (mScanCrashReports)   updateCard(CRASH_REPORTS,          computeSize(mCrashReports));
    if (mScanAppLog)         updateCard(APPLICATION_LOGS,       computeSize(mAppLogs));
    if (mScanAppCache)       updateCard(APPLICATION_CACHES,     computeSize(mAppCaches));
    if (mScanDevToolCache)   updateCard(DEV_TOOL_CACHES,        computeSize(mDevToolCaches));
    if (mScanBrokenSymlinks) updateCard(BROKEN_SYMLINKS,        computeSize(mBrokenSymlinks));
    if (mScanBrowserPrivacy) updateCard(BROWSER_PRIVACY,        computeSize(mBrowserPrivacy));
    if (mScanSnapFlatpak)    updateCard(SNAP_FLATPAK_REVISIONS, computeSize(mSnapFlatpakRevisions));
    if (mScanTrash) {
        quint64 trashSize = 0;
        for (const QString &trashRoot : mCleanerService->getTrashRoots())
            trashSize += FileUtil::getFileSize(trashRoot);
        updateCard(TRASH, trashSize);
    }

    // Retain scan results for inline tree and "Clean selected"
    mRetainedPackageCaches  = mPackageCaches;
    mRetainedCrashReports   = mCrashReports;
    mRetainedAppLogs        = mAppLogs;
    mRetainedAppCaches      = mAppCaches;
    mRetainedDevToolCaches  = mDevToolCaches;
    mRetainedBrokenSymlinks = mBrokenSymlinks;
    mRetainedBrowserPrivacy = mBrowserPrivacy;
    mRetainedSnapFlatpak    = mSnapFlatpakRevisions;

    // Clear worker copies (BUG-10)
    mPackageCaches.clear(); mCrashReports.clear();
    mAppLogs.clear();       mAppCaches.clear();
    mDevToolCaches.clear(); mBrokenSymlinks.clear();
    mBrowserPrivacy.clear(); mSnapFlatpakRevisions.clear();

    mHasScanned = true;
    mScanInProgress = false;
    mInitialScan = false;

    // Re-enable UI
    for (const CategoryCard &c : mCards)
        if (c.check) c.check->setEnabled(true);
    mBtnScanSystem->setEnabled(true);
    mBtnSchedule->setEnabled(true);
    if (mBtnSelectAll) mBtnSelectAll->setEnabled(true);

    // Show details only for currently checked categories
    refreshInlineTree();
    updateFooterTotal();
}

void SystemCleanerPage::refreshInlineTree()
{
    ui->treeWidgetScanResult->setSortingEnabled(false);
    ui->treeWidgetScanResult->clear();

    auto catChecked = [this](CleanCategories cat) -> bool {
        return cat < (int)mCards.size() && mCards[cat].check && mCards[cat].check->isChecked();
    };

    bool anyAdded = false;
    auto addIfChecked = [&](CleanCategories cat, const QString &label, const QFileInfoList &files) {
        if (!catChecked(cat)) return;
        addTreeRoot(cat, label, files);
        anyAdded = true;
    };

    addIfChecked(PACKAGE_CACHE,        mLblPackageCacheText,   mRetainedPackageCaches);
    addIfChecked(CRASH_REPORTS,        mLblCrashReportsText,   mRetainedCrashReports);
    addIfChecked(APPLICATION_LOGS,     mLblAppLogText,         mRetainedAppLogs);
    addIfChecked(APPLICATION_CACHES,   mLblAppCacheText,       mRetainedAppCaches);
    addIfChecked(DEV_TOOL_CACHES,      mLblDevToolCacheText,   mRetainedDevToolCaches);
    addIfChecked(BROKEN_SYMLINKS,      mLblBrokenSymlinksText, mRetainedBrokenSymlinks);
    addIfChecked(BROWSER_PRIVACY,      mLblBrowserPrivacyText, mRetainedBrowserPrivacy);
    addIfChecked(SNAP_FLATPAK_REVISIONS, mLblSnapFlatpakText,  mRetainedSnapFlatpak);

    if (catChecked(TRASH)) {
        // GH#182: display each trash root (home + mounted FSes) separately
        int trashCount = 0;
        for (const QString &trashRoot : mCleanerService->getTrashRoots()) {
            QString displayName = mLblTrashText;
            if (trashCount > 0) {
                // For mounted FS trash, show the mount point path
                QFileInfo fi(trashRoot);
                displayName = QString("%1 (%2)").arg(mLblTrashText, fi.absolutePath());
            }
            addTreeRoot(TRASH, displayName, { QFileInfo(trashRoot) }, true);
            trashCount++;
        }
        anyAdded = true;
    }

    // Dev tool cache: prefix ambiguous child names with their parent app folder
    if (catChecked(DEV_TOOL_CACHES)) {
        for (int i = 0; i < ui->treeWidgetScanResult->topLevelItemCount(); ++i) {
            QTreeWidgetItem *root = ui->treeWidgetScanResult->topLevelItem(i);
            if (root->data(2, 0).toInt() == DEV_TOOL_CACHES) {
                for (int j = 0; j < root->childCount(); ++j) {
                    QTreeWidgetItem *child = root->child(j);
                    QString name = child->text(0);
                    if (name == "Cache" || name == "GPUCache") {
                        QDir dir(child->data(2, 0).toString());
                        dir.cdUp();
                        child->setText(0, dir.dirName() + "/" + name);
                    }
                }
                break;
            }
        }
    }

    ui->treeWidgetScanResult->setSortingEnabled(true);
    on_cbSortBy_currentIndexChanged(ui->cbSortBy->currentIndex());

    if (anyAdded) {
        ui->cleanerPage->show();
    } else {
        ui->cleanerPage->hide();
    }
    updateScheduleIndicator();
}

// ─── Clean selected (from page 0 footer) ─────────────────────────────────────

void SystemCleanerPage::quickCleanByCategory()
{
    if (mScanInProgress || mCleanInProgress || !mHasScanned) return;

    mFilesToDelete.clear();
    mChildrenToRemove.clear();
    mCleanTrash = false;
    mCleanSnapFlatpak = false;
    mCleaningFromCard = true;

    if (ui->cleanerPage->isVisible()) {
        // Tree is visible — honor per-item checkbox selections
        QTreeWidget *tree = ui->treeWidgetScanResult;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *it = tree->topLevelItem(i);
            CleanCategories cat = (CleanCategories) it->data(2, 0).toInt();
            if (cat == TRASH) {
                if (it->checkState(0) == Qt::Checked) mCleanTrash = true;
            } else if (cat == SNAP_FLATPAK_REVISIONS) {
                if (it->checkState(0) == Qt::Checked) mCleanSnapFlatpak = true;
            } else {
                for (int j = 0; j < it->childCount(); ++j) {
                    if (it->child(j)->checkState(0) == Qt::Checked)
                        mFilesToDelete << it->child(j)->data(2, 0).toString();
                }
            }
        }
    } else {
        // No tree yet — category-level fallback
        auto collectFiles = [this](CleanCategories cat, const QFileInfoList &files) {
            if (cat < mCards.size() && mCards[cat].check && mCards[cat].check->isChecked()) {
                for (const QFileInfo &fi : files)
                    mFilesToDelete << fi.absoluteFilePath();
            }
        };
        collectFiles(PACKAGE_CACHE,      mRetainedPackageCaches);
        collectFiles(CRASH_REPORTS,      mRetainedCrashReports);
        collectFiles(APPLICATION_LOGS,   mRetainedAppLogs);
        collectFiles(APPLICATION_CACHES, mRetainedAppCaches);
        collectFiles(DEV_TOOL_CACHES,    mRetainedDevToolCaches);
        collectFiles(BROKEN_SYMLINKS,    mRetainedBrokenSymlinks);
        collectFiles(BROWSER_PRIVACY,    mRetainedBrowserPrivacy);

        if (TRASH < mCards.size() && mCards[TRASH].check && mCards[TRASH].check->isChecked())
            mCleanTrash = true;
        if (mCheckSnapFlatpak && mCheckSnapFlatpak->isChecked())
            mCleanSnapFlatpak = true;
    }

    if (mFilesToDelete.isEmpty() && !mCleanTrash && !mCleanSnapFlatpak) return;

    mBtnCleanSelected->setEnabled(false);
    mBtnScanSystem->setEnabled(false);
    if (mBtnSelectAll) mBtnSelectAll->setEnabled(false);
    for (const CategoryCard &c : mCards)
        if (c.check) c.check->setEnabled(false);

    mCleanInProgress = true;
    mWorkerFuture = QtConcurrent::run([this]() { systemClean(); });
}

void SystemCleanerPage::systemClean()
{
    mTotalCleanedSize = 0;

    QList<CleanerService::CleanCategory> cats;
    if (mCleanTrash)                         cats << CleanerService::TRASH;
    if (mCleanSnapFlatpak)                   cats << CleanerService::SNAP_FLATPAK_REVISIONS;
    if (!mFilesToDelete.isEmpty())           cats << CleanerService::APPLICATION_CACHES;
    if (!cats.isEmpty())
        mCleanerService->maybeTakeSnapshot(cats);

    if (mCleanTrash)
        mTotalCleanedSize += mCleanerService->cleanTrash();

    if (mCleanSnapFlatpak) {
        ToolManager *tmr = ToolManager::ins();
        QList<StaleSnapRevision> snapRevs = tmr->getStaleSnapRevisions();
        for (const StaleSnapRevision &rev : snapRevs)
            mTotalCleanedSize += rev.size;
        tmr->removeStaleSnapRevisions(snapRevs);
        tmr->removeUnusedFlatpakRuntimes();
    }

    if (!mFilesToDelete.isEmpty())
        mTotalCleanedSize += mCleanerService->cleanFiles(mFilesToDelete);

    emit cleanFinishedS();
}

void SystemCleanerPage::onCleanFinished()
{
    if (mCleaningFromCard) {
        // Clean was initiated from footer — reset cards and hide inline results
        for (CategoryCard &c : mCards) {
            if (c.check && c.check->isChecked() && c.lastSize > 0) {
                c.lblSize->setText(QStringLiteral("\u2014"));
                c.lastSize = 0;
            }
        }
        mRetainedPackageCaches.clear(); mRetainedCrashReports.clear();
        mRetainedAppLogs.clear();       mRetainedAppCaches.clear();
        mRetainedDevToolCaches.clear(); mRetainedBrokenSymlinks.clear();
        mRetainedBrowserPrivacy.clear(); mRetainedSnapFlatpak.clear();

        mHasScanned = false;
        ui->cleanerPage->hide();
        ui->treeWidgetScanResult->clear();
        updateFooterTotal();

        for (const CategoryCard &c : mCards)
            if (c.check) c.check->setEnabled(true);
        mBtnScanSystem->setEnabled(true);
        mBtnSchedule->setEnabled(true);
        if (mBtnSelectAll) mBtnSelectAll->setEnabled(true);

    } else {
        // Clean from tree results page (page 1)
        QTreeWidget *tree = ui->treeWidgetScanResult;

        for (int k = mChildrenToRemove.size() - 1; k >= 0; --k) {
            int parentIdx = mChildrenToRemove.at(k).first;
            int childIdx  = mChildrenToRemove.at(k).second;
            if (childIdx == -1) {
                delete tree->takeTopLevelItem(parentIdx);
            } else {
                QTreeWidgetItem *parent = tree->topLevelItem(parentIdx);
                if (parent) delete parent->takeChild(childIdx);
            }
        }

        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *it = tree->topLevelItem(i);
            quint64 remaining = 0;
            for (int j = 0; j < it->childCount(); ++j)
                remaining += it->child(j)->data(1, SortRole).toULongLong();
            it->setText(0, QString("%1 (%2)")
                        .arg(it->data(2, 1).toString())
                        .arg(it->childCount()));
            it->setText(1, FormatUtil::formatBytes(remaining));
        }

        ui->treeWidgetScanResult->setEnabled(true);
    }

    mCleanInProgress = false;
    mCleaningFromCard = false;
}

void SystemCleanerPage::onSnapshotTaken(const QString &toolName)
{
    mLblSnapshotToast->setText(tr("✓ Restore point created via %1. Disable in Settings.").arg(toolName));
    mLblSnapshotToast->show();
    QTimer::singleShot(5000, mLblSnapshotToast, &QLabel::hide);
}

void SystemCleanerPage::onAccessNeededDetected(const QString &message,
                                                const QString &deepLink)
{
    if (!mLblAccessNeeded)
        return;
    if (message.isEmpty()) {
        mLblAccessNeeded->hide();
        return;
    }
    // The cleaner emits a translatable message and a platform deep link. We
    // turn the deep link into an inline rich-text anchor so the user can open
    // the Privacy & Security pane without us pre-building a button. The
    // QLabel::linkActivated handler (wired in buildCleanerFooter) routes the
    // URL through QDesktopServices so the System Settings sheet appears.
    QString body = message.toHtmlEscaped();
    if (!deepLink.isEmpty()) {
        body += QStringLiteral(" <a href=\"%1\">%2</a>")
                    .arg(deepLink.toHtmlEscaped(),
                         tr("Open Privacy & Security…").toHtmlEscaped());
    }
    mLblAccessNeeded->setText(body);
    mLblAccessNeeded->show();
}

// ─── Tree widget helpers ──────────────────────────────────────────────────────

quint64 SystemCleanerPage::addTreeRoot(const CleanCategories &cat, const QString &title,
                                        const QFileInfoList &infos, bool noChild)
{
    QTreeWidgetItem *root = new QTreeWidgetItem(ui->treeWidgetScanResult);
    root->setData(2, 0, cat);
    root->setData(2, 1, title);
    if (!infos.isEmpty())
        root->setData(3, 0, infos.at(0).absoluteDir().path());
    root->setCheckState(0, Qt::Checked);

    QFont rootFont = root->font(0);
    rootFont.setPointSize(10);
    rootFont.setWeight(QFont::DemiBold);
    root->setFont(0, rootFont);
    root->setFont(1, rootFont);

    quint64 totalSize = 0;
    if (!noChild) {
        for (const QFileInfo &i : infos) {
            QString path = i.absoluteFilePath();
            quint64 size = FileUtil::getFileSize(path);
            addTreeChild(path, i.fileName(), size, root);
            totalSize += size;
        }
        root->setText(0, QString("%1 (%2)").arg(title).arg(infos.count()));
        for (int i = 0; i < root->childCount(); ++i)
            root->child(i)->setCheckState(0, Qt::Checked);
    } else {
        if (!infos.isEmpty())
            totalSize += FileUtil::getFileSize(infos.first().absoluteFilePath());
        root->setText(0, title);
    }
    root->setText(1, FormatUtil::formatBytes(totalSize));
    root->setData(1, SortRole, totalSize);
    return totalSize;
}

void SystemCleanerPage::addTreeChild(const QString &data, const QString &text,
                                      const quint64 &size, QTreeWidgetItem *parent)
{
    ByteTreeWidget *item = new ByteTreeWidget(parent);
    item->setValues(text, size, data);
    item->setIcon(0, mDefaultIcon);

    QFont childFont = item->font(0);
    childFont.setPointSize(9);
    childFont.setWeight(QFont::Normal);
    item->setFont(0, childFont);
    item->setFont(1, childFont);
}

void SystemCleanerPage::addTreeChild(const CleanCategories &cat, const QString &text,
                                      const quint64 &size)
{
    ByteTreeWidget *item = new ByteTreeWidget(ui->treeWidgetScanResult);
    item->setValues(text, size, cat);
}

void SystemCleanerPage::on_treeWidgetScanResult_itemClicked(QTreeWidgetItem *item, const int &column)
{
    if (column == 0) {
        Qt::CheckState cs = (item->checkState(column) == Qt::Checked ? Qt::Checked : Qt::Unchecked);
        for (int i = 0; i < item->childCount(); ++i)
            item->child(i)->setCheckState(column, cs);
        updateFooterTotal();
    }
}

void SystemCleanerPage::on_cbSortBy_currentIndexChanged(int idx)
{
    switch (idx) {
        case 0: ui->treeWidgetScanResult->sortItems(0, Qt::AscendingOrder);  break;
        case 1: ui->treeWidgetScanResult->sortItems(0, Qt::DescendingOrder); break;
        case 2: ui->treeWidgetScanResult->sortItems(1, Qt::AscendingOrder);  break;
        case 3: ui->treeWidgetScanResult->sortItems(1, Qt::DescendingOrder); break;
    }
}

// ─── Context menu ─────────────────────────────────────────────────────────────

void SystemCleanerPage::onTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->treeWidgetScanResult->itemAt(pos);
    if (!item || !item->parent()) return;

    QString path = item->data(2, 0).toString();
    if (path.isEmpty()) return;

    QMenu menu(this);
    QAction *actExclude = menu.addAction(tr("Always exclude this"));
    QAction *chosen = menu.exec(ui->treeWidgetScanResult->viewport()->mapToGlobal(pos));
    if (chosen == actExclude) {
        QFileInfo fi(path);
        CleanerService::ExclusionEntry::Type type = fi.isDir()
            ? CleanerService::ExclusionEntry::Folder
            : CleanerService::ExclusionEntry::File;
        mCleanerService->addExclusion(type, path);

        QTreeWidgetItem *parent = item->parent();
        delete parent->takeChild(parent->indexOfChild(item));

        quint64 remaining = 0;
        for (int j = 0; j < parent->childCount(); ++j)
            remaining += parent->child(j)->data(1, SortRole).toULongLong();
        parent->setText(0, QString("%1 (%2)")
                        .arg(parent->data(2, 1).toString())
                        .arg(parent->childCount()));
        parent->setText(1, FormatUtil::formatBytes(remaining));
    }
}

void SystemCleanerPage::onManageExclusions()
{
    ExclusionManagerDialog dlg(this, mAppManager);
    dlg.exec();
}

// ─── Schedule indicator ───────────────────────────────────────────────────────

void SystemCleanerPage::initScheduleIndicator()
{
    mScheduleIndicator = new QFrame(this);
    mScheduleIndicator->setObjectName("scheduleIndicator");

    QHBoxLayout *indicatorLayout = new QHBoxLayout(mScheduleIndicator);
    indicatorLayout->setContentsMargins(12, 6, 12, 6);

    mLblNextSchedule = new QLabel;
    mLblNextSchedule->setObjectName("lblNextSchedule");
    mLblLastSchedule = new QLabel;
    mLblLastSchedule->setObjectName("lblLastSchedule");

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    textLayout->addWidget(mLblNextSchedule);
    textLayout->addWidget(mLblLastSchedule);
    indicatorLayout->addLayout(textLayout, 1);

    mScheduleIndicator->raise();

    connect(mScheduleManager, &ScheduleManager::schedulesChanged,
            this, &SystemCleanerPage::updateScheduleIndicator);

    updateScheduleIndicator();
}

void SystemCleanerPage::repositionScheduleIndicator()
{
    if (!mScheduleIndicator || !mScheduleIndicator->isVisible()) return;

    int outerMarginLR     = 15;
    int outerMarginBottom = 15;
    // Footer is always visible — always offset above it
    int footerH = mCleanerFooter ? mCleanerFooter->sizeHint().height() + 6 : 0;
    int indicatorH = mScheduleIndicator->sizeHint().height();
    int w = width() - outerMarginLR * 2;
    int x = outerMarginLR;
    int y = height() - indicatorH - outerMarginBottom - footerH;

    mScheduleIndicator->setGeometry(x, y, w, indicatorH);
    mScheduleIndicator->raise();
}

void SystemCleanerPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    repositionScheduleIndicator();
    if (!mHasScanned && !mScanInProgress && !mCleanInProgress)
        startBackgroundSizeScan();
}

void SystemCleanerPage::updateScheduleIndicator()
{
    QList<ScheduleManager::CleaningSchedule> schedules = mScheduleManager->getAllSchedules();

    bool hasEnabled = false;
    QDateTime earliest;
    QString nextName;
    QDateTime lastRun;
    quint64 lastBytes = 0;

    for (const auto &s : schedules) {
        if (!s.enabled) continue;
        hasEnabled = true;

        QDateTime next = mScheduleManager->getNextRunTime(s);
        if (!earliest.isValid() || next < earliest) {
            earliest = next;
            nextName = s.name;
        }
        if (s.lastRun.isValid() && (!lastRun.isValid() || s.lastRun > lastRun)) {
            lastRun    = s.lastRun;
            lastBytes  = s.lastBytesFreed;
        }
    }

    // Hide indicator while inline results are showing (avoid overlap)
    if (!hasEnabled || (ui->cleanerPage && ui->cleanerPage->isVisible())) {
        mScheduleIndicator->hide();
        return;
    }

    mScheduleIndicator->show();

    if (earliest.isValid())
        mLblNextSchedule->setText(
            tr("Next: %1 \xe2\x80\x94 %2").arg(nextName, earliest.toString("ddd, MMM d h:mm AP")));

    if (lastRun.isValid())
        mLblLastSchedule->setText(
            tr("Last: %1 \xe2\x80\x94 cleaned %2")
                .arg(lastRun.toString("MMM d"))
                .arg(FormatUtil::formatBytes(lastBytes)));
    else
        mLblLastSchedule->setText(tr("No previous scheduled cleans"));

    repositionScheduleIndicator();
}
