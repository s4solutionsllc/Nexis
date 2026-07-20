#include "cpu_core_detail_widget.h"
#include "cpu_core_item_delegate.h"
#include "cpu_core_list_model.h"

#include "signal_mapper.h"
#include "utilities.h"
#include <Managers/app_manager.h>

#include <QHBoxLayout>
#include <QStyle>

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

    mCard = new QFrame(this);
    mCard->setObjectName("cpuCoreDetailCard");
    root->addWidget(mCard);

    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 14, 16, 14);
    card->setSpacing(8);

    mAccentBar = new QFrame(mCard);
    mAccentBar->setObjectName("sectionHeaderAccent");
    mAccentBar->setFixedWidth(3);
    mAccentBar->setFrameShape(QFrame::NoFrame);
    mAccentBar->setVisible(false);

    mTitle = new QLabel(tr("Per-Core Detail"), mCard);
    mTitle->setObjectName("cpuCoreDetailTitle");
    QFont titleFont = mTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    mTitle->setFont(titleFont);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);
    titleRow->addWidget(mAccentBar);
    titleRow->addWidget(mTitle);
    titleRow->addStretch();
    card->addLayout(titleRow);

    mModel = new CpuCoreListModel(this);
    mDelegate = new CpuCoreItemDelegate(this);

    mListView = new QListView(mCard);
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
    card->addWidget(mListView);
}

void CpuCoreDetailWidget::onCpuUpdated(const QList<int> &percents, const QList<double> &clocks)
{
    mModel->updateData(percents, clocks);
}

void CpuCoreDetailWidget::setElevated(const QString &accentToken)
{
    mCard->setAttribute(Qt::WA_StyledBackground, true);
    mCard->setProperty("cardRole", "elevated");
    mCard->style()->unpolish(mCard);
    mCard->style()->polish(mCard);

    mAccentBar->setProperty("accentToken", accentToken);
    mAccentBar->setVisible(true);
    mAccentBar->style()->unpolish(mAccentBar);
    mAccentBar->style()->polish(mAccentBar);

    Utilities::addDropShadow(mCard, 90, 26);
}
