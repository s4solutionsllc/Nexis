#ifndef KILL_BUTTON_DELEGATE_H
#define KILL_BUTTON_DELEGATE_H

#include <QStyledItemDelegate>

// Renders a small ✕ kill icon in each row of the Processes table.
// Click handling is done via QTableView::clicked in ProcessesPage;
// this delegate is painting-only.
class KillButtonDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit KillButtonDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // KILL_BUTTON_DELEGATE_H
