#include "boot_analysis_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QDateTime>
#include <QtConcurrent>

#include "utilities.h"
#include "Managers/info_manager.h"

BootAnalysisPage::BootAnalysisPage(QWidget *parent, InfoManager *infoManager)
    : QWidget(parent)
    , mIm(infoManager ? infoManager : InfoManager::ins())
{
    buildUi();
    onRefresh();
}

BootAnalysisPage::~BootAnalysisPage()
{
    mCancelled.storeRelaxed(1);
    if (mFuture.isRunning())
        mFuture.waitForFinished();
}

void BootAnalysisPage::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(60, 10, 60, 20);
    outer->setSpacing(0);

    // ---- Toolbar (DS §3 header anatomy, NEX F2 shared recipe) ----
    auto *sectionHeaderRow = new QWidget(this);
    sectionHeaderRow->setObjectName("sectionHeaderRow");
    auto *headerRow = new QHBoxLayout(sectionHeaderRow);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);

    auto *accentBar = new QFrame(sectionHeaderRow);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("accentToken", "accent");
    accentBar->setFrameShape(QFrame::NoFrame);
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(26);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    headerRow->addWidget(accentBar);

    auto *headerTextCol = new QVBoxLayout;
    headerTextCol->setContentsMargins(0, 0, 0, 0);
    headerTextCol->setSpacing(0);

    mLblTitle = new QLabel(tr("Boot Analysis"), sectionHeaderRow);
    mLblTitle->setObjectName("sectionHeaderTitle");
    headerTextCol->addWidget(mLblTitle);

    mLblSource = new QLabel(tr("Startup timing"), sectionHeaderRow);
    mLblSource->setObjectName("sectionHeaderSource");
    headerTextCol->addWidget(mLblSource);

    headerRow->addLayout(headerTextCol, 1);

    mBtnRefresh = new QPushButton(tr("Refresh"), sectionHeaderRow);
    mBtnRefresh->setObjectName("btnBootAnalysisRefresh");
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(mBtnRefresh, 0, Qt::AlignTop);

    outer->addWidget(sectionHeaderRow);
    outer->addSpacing(8);

    // ---- Uptime card (DS §2 elevated card + DS §3 header anatomy + DS §4
    // 14pt/700 hero value) ----
    mUptimeContainer = new QWidget(this);
    mUptimeContainer->setObjectName("bootAnalysisUptimeContainer");
    mUptimeContainer->setAttribute(Qt::WA_StyledBackground, true);
    mUptimeContainer->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(mUptimeContainer, 90, 26);

    auto *uptimeLayout = new QVBoxLayout(mUptimeContainer);
    uptimeLayout->setContentsMargins(14, 12, 14, 10);
    uptimeLayout->setSpacing(6);

    auto *uptimeHeaderRow = new QHBoxLayout;
    uptimeHeaderRow->setSpacing(8);

    auto *uptimeAccent = new QFrame(mUptimeContainer);
    uptimeAccent->setObjectName("sectionHeaderAccent");
    uptimeAccent->setFrameShape(QFrame::NoFrame);
    uptimeAccent->setFixedWidth(3);
    uptimeAccent->setMinimumHeight(26);
    uptimeAccent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    uptimeHeaderRow->addWidget(uptimeAccent);

    auto *uptimeTextCol = new QVBoxLayout;
    uptimeTextCol->setContentsMargins(0, 0, 0, 0);
    uptimeTextCol->setSpacing(4);

    mLblUptimeTitle = new QLabel(mUptimeContainer);
    mLblUptimeTitle->setObjectName("sectionHeaderTitle");
    uptimeTextCol->addWidget(mLblUptimeTitle);

    mLblUptimeValue = new QLabel(mUptimeContainer);
    mLblUptimeValue->setObjectName("metricTileValue");
    uptimeTextCol->addWidget(mLblUptimeValue);

    mLblUptimeMeta = new QLabel(mUptimeContainer);
    mLblUptimeMeta->setObjectName("sectionHeaderSource");
    mLblUptimeMeta->setVisible(false);
    uptimeTextCol->addWidget(mLblUptimeMeta);

    uptimeHeaderRow->addLayout(uptimeTextCol, 1);
    uptimeLayout->addLayout(uptimeHeaderRow);

    outer->addWidget(mUptimeContainer);
    outer->addSpacing(8);
    mUptimeContainer->setVisible(false);

    // ---- Table (per-service breakdown; out of this item's scope) ----
    mTable = new QTableWidget(0, 3, this);
    mTable->setObjectName("tblBootAnalysis");
    mTable->setHorizontalHeaderLabels({tr("Service"), tr("Duration"), tr("Impact")});
    mTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTable->verticalHeader()->setVisible(false);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->setSelectionMode(QAbstractItemView::NoSelection);
    // SSO-3502: read-only data display (NoSelection + NoEditTriggers); skipped
    // by tab order because nothing focusable lives inside.
    mTable->setFocusPolicy(Qt::NoFocus);
    mTable->setAlternatingRowColors(true);
    mTable->setShowGrid(false);
    outer->addWidget(mTable, 1);

    // ---- Empty state (DS §5, NEX F3 shared recipe): upgrades the old
    // text-only #lblBootEmptyState into icon + explanation + next-action,
    // inside its own elevated card (DS §2). ----
    mEmptyContainer = new QWidget(this);
    mEmptyContainer->setObjectName("bootAnalysisEmptyContainer");
    mEmptyContainer->setAttribute(Qt::WA_StyledBackground, true);
    mEmptyContainer->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(mEmptyContainer, 90, 26);

    auto *emptyContainerLayout = new QVBoxLayout(mEmptyContainer);
    emptyContainerLayout->setContentsMargins(0, 0, 0, 0);
    emptyContainerLayout->setSpacing(0);

    mEmptyState = new QWidget(mEmptyContainer);
    mEmptyState->setObjectName("bootAnalysisEmptyState");
    auto *emptyLayout = new QVBoxLayout(mEmptyState);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch();

    auto *emptyIcon = new QLabel(QString::fromUtf8("\xE2\x8F\xB0"), mEmptyState); // alarm-clock glyph
    emptyIcon->setObjectName("emptyStateIcon");
    emptyIcon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    emptyLayout->addWidget(emptyIcon);

    mLblEmptyHeading = new QLabel(mEmptyState);
    mLblEmptyHeading->setObjectName("lblBootAnalysisEmptyHeading");
    mLblEmptyHeading->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    mLblEmptyHeading->setWordWrap(true);
    mLblEmptyHeading->setText(tr("Per-service boot timing isn't available on macOS"));
    emptyLayout->addWidget(mLblEmptyHeading);

    mLblEmptyText = new QLabel(mEmptyState);
    mLblEmptyText->setObjectName("emptyStateText");
    mLblEmptyText->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    mLblEmptyText->setWordWrap(true);
    mLblEmptyText->setText(tr("macOS does not expose per-service startup durations. "
                              "System uptime since last boot is shown above."));
    emptyLayout->addWidget(mLblEmptyText);

    auto *emptyBtnRow = new QHBoxLayout;
    emptyBtnRow->addStretch();
    mBtnRefreshUptime = new QPushButton(tr("Refresh uptime"), mEmptyState);
    mBtnRefreshUptime->setObjectName("btnBootAnalysisRefreshUptime");
    mBtnRefreshUptime->setAccessibleName("primary");
    mBtnRefreshUptime->setCursor(Qt::PointingHandCursor);
    emptyBtnRow->addWidget(mBtnRefreshUptime);
    emptyBtnRow->addStretch();
    emptyLayout->addLayout(emptyBtnRow);

    emptyLayout->addStretch();
    emptyContainerLayout->addWidget(mEmptyState);

    outer->addWidget(mEmptyContainer, 1);
    mEmptyContainer->setVisible(false);

    // ---- Status ----
    mLblStatus = new QLabel(this);
    mLblStatus->setObjectName("lblBootAnalysisStatus");
    mLblStatus->setAlignment(Qt::AlignRight);
    outer->addWidget(mLblStatus);

    connect(mBtnRefresh, &QPushButton::clicked, this, &BootAnalysisPage::onRefresh);
    connect(mBtnRefreshUptime, &QPushButton::clicked, this, &BootAnalysisPage::onRefresh);
}

void BootAnalysisPage::onRefresh()
{
    mBtnRefresh->setEnabled(false);
    mBtnRefresh->setText(tr("Analyzing…"));
    mLblStatus->clear();
    mCancelled.storeRelaxed(0);

    BootAnalysisInfo *info = mIm->bootAnalysisInfo();
    mFuture = QtConcurrent::run([info]() -> BootAnalysisData {
        return info->analyze();
    });

    auto *watcher = new QFutureWatcher<BootAnalysisData>(this);
    connect(watcher, &QFutureWatcher<BootAnalysisData>::finished, this, [this, watcher]() {
        if (!mCancelled.loadRelaxed())
            populate(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(mFuture);
}

void BootAnalysisPage::populate(const BootAnalysisData &data)
{
    mBtnRefresh->setEnabled(true);
    mBtnRefresh->setText(tr("Refresh"));

    mTable->setVisible(true);
    mUptimeContainer->setVisible(false);
    mEmptyContainer->setVisible(false);
    mTable->setRowCount(0);

    if (!data.available) {
        mLblStatus->setText(data.error.isEmpty() ? tr("Boot analysis not available.") : data.error);
        return;
    }

    // Uptime card: title + 14pt/700 hero value (DS §2/§3/§4).
    if (data.totalBootMs > 0) {
        double secs = data.totalBootMs / 1000.0;
        mUptimeContainer->setVisible(true);
        mLblUptimeValue->setText(tr("%1 s").arg(secs, 0, 'f', 1));
#ifdef Q_OS_MACOS
        mLblUptimeTitle->setText(tr("System uptime since last boot"));
        mLblUptimeMeta->setVisible(false);
#else
        mLblUptimeTitle->setText(tr("Total boot time"));
        mLblUptimeMeta->setText(tr("%1 services").arg(data.entries.size()));
        mLblUptimeMeta->setVisible(true);
#endif
    }

#ifdef Q_OS_MACOS
    if (data.entries.isEmpty()) {
        mTable->setVisible(false);
        mEmptyContainer->setVisible(true);
        mLblStatus->clear();
        return;
    }
#endif

    mTable->setRowCount(data.entries.size());
    for (int row = 0; row < data.entries.size(); ++row) {
        const BootEntry &e = data.entries.at(row);

        auto *nameItem = new QTableWidgetItem(e.name);
        nameItem->setToolTip(e.name);

        double secs = e.durationMs / 1000.0;
        QString durStr = (e.durationMs < 1000.0)
            ? QString("%1 ms").arg(static_cast<int>(e.durationMs))
            : QString("%1 s").arg(secs, 0, 'f', 2);
        auto *durItem = new QTableWidgetItem(durStr);
        durItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto *impactItem = new QTableWidgetItem(e.impact);
        impactItem->setTextAlignment(Qt::AlignCenter);

        mTable->setItem(row, 0, nameItem);
        mTable->setItem(row, 1, durItem);
        mTable->setItem(row, 2, impactItem);
    }

    mLblStatus->setText(tr("Last analyzed: %1")
                        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}
