#ifndef PIN_SORT_FILTER_PROXY_MODEL_H
#define PIN_SORT_FILTER_PROXY_MODEL_H

#include <QSortFilterProxyModel>

// FR-116: sort model that keeps "pinned" rows at the top regardless of
// the user's chosen sort column. Pinned state is read from a custom
// data role (PinnedRole) on column 0 of the source model. When the role
// differs between two rows, pinned < unpinned. When they match, fall
// back to the standard QSortFilterProxyModel comparison (which uses the
// SortRole configured by ProcessesPage).
class PinSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    static constexpr int PinnedRole = Qt::UserRole + 3;

    explicit PinSortFilterProxyModel(QObject *parent = nullptr);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

#endif // PIN_SORT_FILTER_PROXY_MODEL_H
