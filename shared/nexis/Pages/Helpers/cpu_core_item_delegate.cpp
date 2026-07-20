#include "cpu_core_item_delegate.h"
#include "cpu_core_list_model.h"

#include <Managers/app_manager.h>

#include <QPainter>

namespace {
constexpr int kRowHeight = 22;
constexpr int kBarWidth = 60;
constexpr int kBarHeight = 6;
}

CpuCoreItemDelegate::CpuCoreItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    refreshThemeColors();
}

void CpuCoreItemDelegate::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    // DS §4 key/value row convention (style.qss:772-815): @color06 labels,
    // @color05 values, @borderColor divider. @cpuColor is the same accent
    // used to elevate the CPU charts above (setElevated("cpu")).
    mLabelColor    = QColor(sv->value("@color06").toString());
    mValueColor    = QColor(sv->value("@color05").toString());
    mBarTrackColor = QColor(sv->value("@borderColor").toString());
    mBarFillColor  = QColor(sv->value("@cpuColor").toString());
    mDividerColor  = QColor(sv->value("@borderColor").toString());
}

QSize CpuCoreItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(-1, kRowHeight);
}

void CpuCoreItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect rect = option.rect;
    const int pct = index.data(CpuCoreListModel::UtilizationRole).toInt();
    const double mhz = index.data(CpuCoreListModel::FrequencyMhzRole).toDouble();
    const QString label = index.data(Qt::DisplayRole).toString();

    if (option.state & QStyle::State_Selected)
        painter->fillRect(rect, option.palette.highlight());

    const int margin = 10;
    int x = rect.left() + margin;
    const int midY = rect.top() + rect.height() / 2;

    // Label ("CPU N")
    QFont labelFont = option.font;
    const QRect labelRect(x, rect.top(), 60, rect.height());
    painter->setPen(mLabelColor);
    painter->setFont(labelFont);
    painter->drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, label);
    x = labelRect.right();

    // Inline utilization bar
    x += margin;
    const QRect barTrack(x, midY - kBarHeight / 2, kBarWidth, kBarHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(mBarTrackColor);
    painter->drawRoundedRect(barTrack, kBarHeight / 2.0, kBarHeight / 2.0);

    const int fillWidth = qBound(0, pct, 100) * kBarWidth / 100;
    if (fillWidth > 0) {
        const QRect barFill(x, midY - kBarHeight / 2, fillWidth, kBarHeight);
        painter->setBrush(mBarFillColor);
        painter->drawRoundedRect(barFill, kBarHeight / 2.0, kBarHeight / 2.0);
    }
    x += kBarWidth + margin;

    // Value text ("42%  ·  3.60 GHz")
    QString valueText = tr("%1%").arg(pct);
    if (mhz > 0.0)
        valueText += tr("  ·  %1 GHz").arg(mhz / 1000.0, 0, 'f', 2);

    const QRect valueRect(x, rect.top(), rect.right() - x - margin, rect.height());
    painter->setPen(mValueColor);
    painter->drawText(valueRect, Qt::AlignVCenter | Qt::AlignLeft, valueText);

    // Row divider
    painter->setPen(mDividerColor);
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    painter->restore();
}
