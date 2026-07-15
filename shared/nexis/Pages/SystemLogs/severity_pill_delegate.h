#ifndef SEVERITY_PILL_DELEGATE_H
#define SEVERITY_PILL_DELEGATE_H

#include <QStyledItemDelegate>

// Renders the Severity column as a status pill (DS §5): a rounded
// @chartGridColor track with status-colored text, keyed off the syslog
// severity int stored on Qt::UserRole. Row background/selection/hover/
// zebra painting is left to the base delegate; only the default text
// draw is suppressed (initStyleOption) in favor of the pill.
class SeverityPillDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SeverityPillDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option,
                          const QModelIndex &index) const override;
};

#endif // SEVERITY_PILL_DELEGATE_H
