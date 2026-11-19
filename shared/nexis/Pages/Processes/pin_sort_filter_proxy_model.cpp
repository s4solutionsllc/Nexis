#include "pin_sort_filter_proxy_model.h"
#include "nexis_roles.h"

PinSortFilterProxyModel::PinSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void PinSortFilterProxyModel::setTreeMode(bool enabled)
{
    if (mTreeMode == enabled)
        return;
    mTreeMode = enabled;
    invalidateFilter();
}

bool PinSortFilterProxyModel::isGroupHeader(const QModelIndex &sourceIndex) const
{
    return sourceModel()->index(sourceIndex.row(), 0, sourceIndex.parent())
        .data(GroupHeaderRole).toBool();
}

bool PinSortFilterProxyModel::filterAcceptsRow(int sourceRow,
                                                const QModelIndex &sourceParent) const
{
    if (mTreeMode && isGroupHeader(sourceModel()->index(sourceRow, 0, sourceParent)))
        return false;
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool PinSortFilterProxyModel::lessThan(const QModelIndex &left,
                                        const QModelIndex &right) const
{
    // SSO-15376: root-level rows are the Apps/Background section headers —
    // keep them in insertion order (Apps first) no matter which data column
    // the user sorts by. Only rows *within* a group should reorder on a
    // column-header click. Mirrors the pinned-row inversion below so the
    // group order also survives an Ascending/Descending toggle.
    if (isGroupHeader(left) && isGroupHeader(right)) {
        return (sortOrder() == Qt::AscendingOrder) ? (left.row() < right.row())
                                                     : (left.row() > right.row());
    }

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
