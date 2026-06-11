#include "boot_analysis_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QDateTime>
#include <QtConcurrent>

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

    // ---- Header row ----
    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);

    mLblTitle = new QLabel(tr("Boot Analysis"), this);
    mLblTitle->setObjectName("lblBootAnalysisTitle");
    QFont f = mLblTitle->font();
    f.setPointSize(11);
    mLblTitle->setFont(f);

    mBtnRefresh = new QPushButton(tr("Refresh"), this);
    mBtnRefresh->setObjectName("btnBootAnalysisRefresh");
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    mBtnRefresh->setFocusPolicy(Qt::NoFocus);

    headerRow->addWidget(mLblTitle, 1);
    headerRow->addWidget(mBtnRefresh, 0);
    outer->addLayout(headerRow);

    // ---- Subtitle ----
    mLblSubtitle = new QLabel(this);
    mLblSubtitle->setObjectName("lblBootAnalysisSubtitle");
    outer->addWidget(mLblSubtitle);
    outer->addSpacing(8);

    // ---- Table ----
    mTable = new QTableWidget(0, 3, this);
    mTable->setObjectName("tblBootAnalysis");
    mTable->setHorizontalHeaderLabels({tr("Service"), tr("Duration"), tr("Impact")});
    mTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTable->verticalHeader()->setVisible(false);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->setSelectionMode(QAbstractItemView::NoSelection);
    mTable->setFocusPolicy(Qt::NoFocus);
    mTable->setAlternatingRowColors(true);
    mTable->setShowGrid(false);
    outer->addWidget(mTable, 1);

    // ---- Empty state ----
    mLblEmpty = new QLabel(this);
    mLblEmpty->setObjectName("lblBootEmptyState");
    mLblEmpty->setAlignment(Qt::AlignCenter);
    mLblEmpty->setWordWrap(true);
    mLblEmpty->setVisible(false);
    outer->addWidget(mLblEmpty, 1);

    // ---- Status ----
    mLblStatus = new QLabel(this);
    mLblStatus->setObjectName("lblBootAnalysisStatus");
    mLblStatus->setAlignment(Qt::AlignRight);
    outer->addWidget(mLblStatus);

    connect(mBtnRefresh, &QPushButton::clicked, this, &BootAnalysisPage::onRefresh);
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
    mLblEmpty->setVisible(false);
    mTable->setRowCount(0);

    if (!data.available) {
        mLblSubtitle->setText(tr("Boot analysis not available."));
        mLblStatus->setText(data.error.isEmpty() ? tr("No data.") : data.error);
        return;
    }

    // Subtitle: total time
    if (data.totalBootMs > 0) {
        double secs = data.totalBootMs / 1000.0;
#ifdef Q_OS_MACOS
        mLblSubtitle->setText(tr("System uptime since last boot: %1 s").arg(secs, 0, 'f', 1));
#else
        mLblSubtitle->setText(tr("Total boot time: %1 s  |  %2 services")
                              .arg(secs, 0, 'f', 1)
                              .arg(data.entries.size()));
#endif
    }

#ifdef Q_OS_MACOS
    if (data.entries.isEmpty()) {
        mTable->setVisible(false);
        mLblEmpty->setVisible(true);
        mLblEmpty->setText(tr("Per-service boot timing is not available on macOS.\n\nSystem uptime since last boot is shown above."));
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
