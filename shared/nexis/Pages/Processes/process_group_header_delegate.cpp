#include "process_group_header_delegate.h"
#include "nexis_roles.h"
#include "dpi.h"
#include "Managers/app_manager.h"

#include <QPainter>
#include <QSettings>

ProcessGroupHeaderDelegate::ProcessGroupHeaderDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ProcessGroupHeaderDelegate::paint(QPainter *painter,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    if (!index.data(GroupHeaderRole).toBool()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // DS §3 header anatomy: a 3px accent bar (radius 1) to the left of a
    // title, on the DS §7 flat-row fill (shadow stays on the container).
    QSettings *sv = AppManager::ins()->getStyleValues();
    const QColor fill = sv
        ? QColor(sv->value(QStringLiteral("@cardBg"), QStringLiteral("#2A2C32")).toString())
        : QColor(QStringLiteral("#2A2C32"));
    const QColor accent = sv
        ? QColor(sv->value(QStringLiteral("@accentColor"), QStringLiteral("#FF6B1A")).toString())
        : QColor(QStringLiteral("#FF6B1A"));
    const QColor titleColor = sv
        ? QColor(sv->value(QStringLiteral("@color06"), QStringLiteral("#9A9DA6")).toString())
        : QColor(QStringLiteral("#9A9DA6"));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    painter->fillRect(option.rect, fill);

    const int barWidth = Dpi::scale(3);
    const int barHeight = Dpi::scale(16);
    const QRect barRect(option.rect.left() + Dpi::scale(10),
                         option.rect.top() + (option.rect.height() - barHeight) / 2,
                         barWidth, barHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(accent);
    painter->drawRoundedRect(barRect, 1, 1);

    QFont titleFont = option.font;
    titleFont.setPointSize(9);
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(titleColor);

    QRect textRect = option.rect;
    textRect.setLeft(barRect.right() + Dpi::scale(8));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                       index.data(Qt::DisplayRole).toString());

    painter->restore();
}

QSize ProcessGroupHeaderDelegate::sizeHint(const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    if (!index.data(GroupHeaderRole).toBool())
        return base;

    return QSize(base.width(), Dpi::scale(32));
}
