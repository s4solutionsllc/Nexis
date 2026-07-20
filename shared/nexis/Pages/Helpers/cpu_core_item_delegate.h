#ifndef CPU_CORE_ITEM_DELEGATE_H
#define CPU_CORE_ITEM_DELEGATE_H

#include <QColor>
#include <QStyledItemDelegate>

// SSO-15378: paints one compact per-core row (label + inline utilization
// bar + util%/frequency text) directly via QPainter instead of instantiating
// a QWidget per core — the row count this delegate is asked to paint is
// bounded by the viewport height, not the core count, which is what keeps
// the per-core detail view responsive at 32+ cores.
class CpuCoreItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CpuCoreItemDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

public slots:
    void refreshThemeColors();

private:
    QColor mLabelColor;
    QColor mValueColor;
    QColor mBarTrackColor;
    QColor mBarFillColor;
    QColor mDividerColor;
};

#endif // CPU_CORE_ITEM_DELEGATE_H
