#ifndef PIN_SORT_FILTER_PROXY_MODEL_H
#define PIN_SORT_FILTER_PROXY_MODEL_H

#include <QSortFilterProxyModel>

// FR-116: sort model that keeps "pinned" rows at the top regardless of
// the user's chosen sort column. Pinned state is read from a custom
// data role (PinnedRole) on column 0 of the source model. When the role
// differs between two rows, pinned < unpinned. When they match, fall
// back to the standard QSortFilterProxyModel comparison (which uses the
// SortRole configured by ProcessesPage).
//
// SSO-15376: also keeps the two root-level Apps/Background section headers
// in a fixed order (Apps first), independent of both pin state and the
// chosen sort column — see lessThan()'s root-level branch.
//
// SSO-27153: in tree mode the section headers are hidden and root-level rows
// are ordinary (parentless) process rows.
class PinSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    static constexpr int PinnedRole = Qt::UserRole + 3;

    explicit PinSortFilterProxyModel(QObject *parent = nullptr);

    void setTreeMode(bool enabled);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool isGroupHeader(const QModelIndex &sourceIndex) const;

    bool mTreeMode = false;
};

#endif // PIN_SORT_FILTER_PROXY_MODEL_H
