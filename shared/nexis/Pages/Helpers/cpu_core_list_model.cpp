#include "cpu_core_list_model.h"

CpuCoreListModel::CpuCoreListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CpuCoreListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return mCoreCount;
}

QVariant CpuCoreListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= mCoreCount)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return tr("CPU %1").arg(index.row());
    case UtilizationRole:
        return index.row() < mUtilization.size() ? mUtilization.at(index.row()) : 0;
    case FrequencyMhzRole:
        return index.row() < mFrequencyMhz.size() ? mFrequencyMhz.at(index.row()) : 0.0;
    default:
        return QVariant();
    }
}

void CpuCoreListModel::updateData(const QList<int> &percents, const QList<double> &clocks)
{
    // percents[0] is the aggregate; per-core entries start at index 1.
    const int coreCount = qMax(0, percents.size() - 1);

    if (coreCount != mCoreCount) {
        beginResetModel();
        mCoreCount = coreCount;
        mUtilization = percents.mid(1);
        mFrequencyMhz = clocks;
        endResetModel();
        return;
    }

    mUtilization = percents.mid(1);
    mFrequencyMhz = clocks;
    if (mCoreCount > 0)
        emit dataChanged(index(0), index(mCoreCount - 1), {UtilizationRole, FrequencyMhzRole});
}
