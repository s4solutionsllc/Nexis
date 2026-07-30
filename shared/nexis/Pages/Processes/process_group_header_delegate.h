#ifndef PROCESS_GROUP_HEADER_DELEGATE_H
#define PROCESS_GROUP_HEADER_DELEGATE_H

#include <QStyledItemDelegate>

// SSO-15376: paints the "Apps" / "Background" section-header rows in the
// Processes tree — DS §3 header anatomy (left accent bar + title) scaled
// down to a first-column-spanned tree row instead of a standalone widget.
// Rows without GroupHeaderRole set fall through to the default
// QStyledItemDelegate paint/sizeHint, so ordinary process rows are
// unaffected — this delegate only ever replaces Col_Pid's delegate.
class ProcessGroupHeaderDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ProcessGroupHeaderDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // PROCESS_GROUP_HEADER_DELEGATE_H
