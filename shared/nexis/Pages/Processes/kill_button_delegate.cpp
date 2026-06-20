#include "kill_button_delegate.h"
#include "Managers/app_manager.h"
#include <QPainter>
#include <QSettings>

KillButtonDelegate::KillButtonDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void KillButtonDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    // Draw standard row background (selection, hover, alternating).
    QStyledItemDelegate::paint(painter, option, index);

    QSettings *sv = AppManager::ins()->getStyleValues();
    QColor killColor(sv ? sv->value(QStringLiteral("@destructiveColor"),
                                    QStringLiteral("#E05454")).toString()
                        : QStringLiteral("#E05454"));

    const bool hovered = (option.state & QStyle::State_MouseOver);
    if (hovered)
        killColor = killColor.lighter(130);

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);
    QFont f = option.font;
    f.setPointSize(9);
    f.setBold(true);
    painter->setFont(f);
    painter->setPen(killColor);
    painter->drawText(option.rect, Qt::AlignCenter, QString(QChar(0x2715))); // ✕
    painter->restore();
}

QSize KillButtonDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return {24, 24};
}
