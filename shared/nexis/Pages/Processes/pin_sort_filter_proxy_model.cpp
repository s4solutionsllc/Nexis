#include "pin_sort_filter_proxy_model.h"

PinSortFilterProxyModel::PinSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

bool PinSortFilterProxyModel::lessThan(const QModelIndex &left,
                                        const QModelIndex &right) const
{
    // Pin state lives on the column-0 item regardless of which column the
    // user is sorting by — always look there.
    const QModelIndex leftPid  = sourceModel()->index(left.row(),  0, left.parent());
    const QModelIndex rightPid = sourceModel()->index(right.row(), 0, right.parent());

    const bool lp = leftPid.data(PinnedRole).toBool();
    const bool rp = rightPid.data(PinnedRole).toBool();
    if (lp != rp) {
        // Pinned rows must sort to the top regardless of Ascending /
        // Descending toggle — invert the natural `lp > rp` when descending.
        const bool pinnedIsLess = (sortOrder() == Qt::AscendingOrder) ? lp : !lp;
        return pinnedIsLess;
    }

    return QSortFilterProxyModel::lessThan(left, right);
}
