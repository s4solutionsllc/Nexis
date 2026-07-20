#ifndef CPU_CORE_LIST_MODEL_H
#define CPU_CORE_LIST_MODEL_H

#include <QAbstractListModel>
#include <QList>

// SSO-15378: backing store for the compact per-core detail list. A model +
// QListView (see CpuCoreDetailWidget) renders only the rows that are
// actually visible, so this stays responsive at 32+ cores instead of the
// one-row-widget-per-core pattern used by CpuTuningWidget's advanced
// governor grid.
class CpuCoreListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        UtilizationRole = Qt::UserRole + 1,
        FrequencyMhzRole,
    };

    explicit CpuCoreListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // `percents` follows the DataRefreshService::cpuUpdated convention:
    // index 0 = overall %, indices 1..N = per-core %. `clocks` is per-core
    // MHz, indexed 0..N-1 (may be shorter than the core count, or empty, on
    // hosts without a readable frequency source — rows just omit the MHz
    // suffix in that case).
    void updateData(const QList<int> &percents, const QList<double> &clocks);

private:
    int mCoreCount = 0;
    QList<int> mUtilization;
    QList<double> mFrequencyMhz;
};

#endif // CPU_CORE_LIST_MODEL_H
