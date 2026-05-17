#ifndef NET_USAGE_TRACKER_H
#define NET_USAGE_TRACKER_H

#include <QObject>
#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QTimer>

#include <Info/network_info.h>

class DataRefreshService;

struct DailyBucket {
    QDate date;
    quint64 rx = 0;
    quint64 tx = 0;
};

class NetUsageTracker : public QObject
{
    Q_OBJECT

public:
    static NetUsageTracker *ins();

    void start(DataRefreshService *drs);

    quint64 todayRx(const QString &iface) const;
    quint64 todayTx(const QString &iface) const;
    quint64 weekRx(const QString &iface) const;
    quint64 weekTx(const QString &iface) const;
    quint64 monthRx(const QString &iface, int resetDay) const;
    quint64 monthTx(const QString &iface, int resetDay) const;

    QList<DailyBucket> history(const QString &iface, int days) const;
    QStringList trackedInterfaces() const;

    static const QString kAllInterfaces;

signals:
    void thresholdBreached(int percent);
    void dataChanged();

private slots:
    void onPerInterfaceTick(const QHash<QString, NetInterfaceStats> &stats);
    void persist();

private:
    explicit NetUsageTracker(QObject *parent = nullptr);

    static NetUsageTracker *instance;

    void load();
    void scheduleSave();
    void checkCapThresholds();
    QDate periodStart(int resetDay) const;
    quint64 sumRange(const QString &iface, QDate from, QDate to, bool rx) const;

    // mBuckets[iface][date] = DailyBucket
    QHash<QString, QHash<QDate, DailyBucket>> mBuckets;

    QHash<QString, quint64> mLastRx;
    QHash<QString, quint64> mLastTx;
    QDate mCurrentDate;

    QTimer *mSaveTimer = nullptr;
    int mLastAlertedPercent = 0;
};

#endif // NET_USAGE_TRACKER_H
