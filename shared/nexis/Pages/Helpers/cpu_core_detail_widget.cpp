#include "cpu_core_detail_widget.h"
#include "cpu_core_item_delegate.h"
#include "cpu_core_list_model.h"

#include "signal_mapper.h"

namespace {
// Compact by design: ~8 rows visible before the list scrolls internally,
// so a 128-core host doesn't turn this card into a mile-long page.
constexpr int kVisibleRows = 8;
constexpr int kRowHeight = 22;
}

CpuCoreDetailWidget::CpuCoreDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            mDelegate, &CpuCoreItemDelegate::refreshThemeColors);
}

void CpuCoreDetailWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mModel = new CpuCoreListModel(this);
    mDelegate = new CpuCoreItemDelegate(this);

    mListView = new QListView(this);
    mListView->setObjectName("cpuCoreDetailList");
    mListView->setModel(mModel);
    mListView->setItemDelegate(mDelegate);
    // Uniform sizes let QListView skip a full layout pass per row — the key
    // scaling property at 32+ cores, since it turns a relayout into O(visible)
    // instead of O(coreCount).
    mListView->setUniformItemSizes(true);
    mListView->setSelectionMode(QAbstractItemView::NoSelection);
    mListView->setFocusPolicy(Qt::NoFocus);
    mListView->setFrameShape(QFrame::NoFrame);
    mListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mListView->setFixedHeight(kRowHeight * kVisibleRows);
    root->addWidget(mListView);
}

void CpuCoreDetailWidget::onCpuUpdated(const QList<int> &percents, const QList<double> &clocks)
{
    mModel->updateData(percents, clocks);
}
