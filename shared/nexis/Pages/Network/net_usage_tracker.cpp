#include "net_usage_tracker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "Managers/setting_manager.h"
#include "Managers/info_manager.h"
#include "Managers/data_refresh_service.h"

NetUsageTracker *NetUsageTracker::instance = nullptr;
const QString NetUsageTracker::kAllInterfaces = QStringLiteral("__all__");

NetUsageTracker::NetUsageTracker(QObject *parent)
    : QObject(parent),
      mCurrentDate(QDate::currentDate()),
      mSaveTimer(new QTimer(this))
{
    mSaveTimer->setSingleShot(true);
    mSaveTimer->setInterval(60000);
    connect(mSaveTimer, &QTimer::timeout, this, &NetUsageTracker::persist);

    load();

    mLastAlertedPercent = SettingManager::ins()->getNetCapAlertLastPercent();
}

NetUsageTracker *NetUsageTracker::ins()
{
    if (!instance)
        instance = new NetUsageTracker;
    return instance;
}

void NetUsageTracker::start(DataRefreshService *drs)
{
    drs->subscribe(DataRefreshService::Signal::Network);
    connect(drs, &DataRefreshService::networkUpdated,
            this, &NetUsageTracker::onNetworkTick);
}

void NetUsageTracker::onNetworkTick(quint64 rxAbs, quint64 txAbs)
{
    const QString iface = InfoManager::ins()->getDefaultNetworkInterface();
    if (iface.isEmpty())
        return;

    mDefaultIface = iface;

    // Day rollover
    const QDate today = QDate::currentDate();
    if (today != mCurrentDate)
        mCurrentDate = today;

    // Reboot / interface restart detection: counter went backwards
    if (mLastRx.contains(iface) && rxAbs < mLastRx[iface]) {
        mLastRx[iface] = rxAbs;
        mLastTx[iface] = txAbs;
        return;
    }

    if (mLastRx.contains(iface)) {
        const quint64 dRx = rxAbs - mLastRx[iface];
        const quint64 dTx = txAbs - mLastTx[iface];

        if (dRx > 0 || dTx > 0) {
            mBuckets[iface][today].date = today;
            mBuckets[iface][today].rx += dRx;
            mBuckets[iface][today].tx += dTx;

            scheduleSave();
            checkCapThresholds();
            emit dataChanged();
        }
    }

    mLastRx[iface] = rxAbs;
    mLastTx[iface] = txAbs;
}

void NetUsageTracker::scheduleSave()
{
    if (!mSaveTimer->isActive())
        mSaveTimer->start();
}

void NetUsageTracker::checkCapThresholds()
{
    const int capGB = SettingManager::ins()->getNetCapGB();
    if (capGB <= 0)
        return;

    const int resetDay = SettingManager::ins()->getNetCapResetDay();
    const quint64 used = monthRx(kAllInterfaces, resetDay) + monthTx(kAllInterfaces, resetDay);
    const quint64 capBytes = static_cast<quint64>(capGB) * 1073741824ULL;

    if (capBytes == 0)
        return;

    const int pct = static_cast<int>(used * 100 / capBytes);

    int tier = 0;
    if (pct >= 100) tier = 100;
    else if (pct >= 90) tier = 90;
    else if (pct >= 75) tier = 75;

    if (tier > 0 && tier > mLastAlertedPercent) {
        mLastAlertedPercent = tier;
        SettingManager::ins()->setNetCapAlertLastPercent(tier);
        emit thresholdBreached(tier);
    }
}

// ── Queries ──────────────────────────────────────────────────────────────────

quint64 NetUsageTracker::todayRx(const QString &iface) const
{
    return sumRange(iface, mCurrentDate, mCurrentDate, true);
}

quint64 NetUsageTracker::todayTx(const QString &iface) const
{
    return sumRange(iface, mCurrentDate, mCurrentDate, false);
}

quint64 NetUsageTracker::weekRx(const QString &iface) const
{
    return sumRange(iface, mCurrentDate.addDays(-6), mCurrentDate, true);
}

quint64 NetUsageTracker::weekTx(const QString &iface) const
{
    return sumRange(iface, mCurrentDate.addDays(-6), mCurrentDate, false);
}

QDate NetUsageTracker::periodStart(int resetDay) const
{
    const QDate today = mCurrentDate;
    QDate start(today.year(), today.month(), qMin(resetDay, today.daysInMonth()));
    if (start > today)
        start = start.addMonths(-1);
    return start;
}

quint64 NetUsageTracker::monthRx(const QString &iface, int resetDay) const
{
    return sumRange(iface, periodStart(resetDay), mCurrentDate, true);
}

quint64 NetUsageTracker::monthTx(const QString &iface, int resetDay) const
{
    return sumRange(iface, periodStart(resetDay), mCurrentDate, false);
}

quint64 NetUsageTracker::sumRange(const QString &iface, QDate from, QDate to, bool rx) const
{
    quint64 total = 0;

    auto sumIface = [&](const QString &key) {
        const auto it = mBuckets.constFind(key);
        if (it == mBuckets.constEnd())
            return;
        for (QDate d = from; d <= to; d = d.addDays(1)) {
            const auto bit = it->constFind(d);
            if (bit != it->constEnd())
                total += rx ? bit->rx : bit->tx;
        }
    };

    if (iface == kAllInterfaces) {
        for (const QString &key : mBuckets.keys())
            sumIface(key);
    } else {
        sumIface(iface);
    }

    return total;
}

QList<DailyBucket> NetUsageTracker::history(const QString &iface, int days) const
{
    QList<DailyBucket> result;
    const QDate from = mCurrentDate.addDays(-(days - 1));

    for (QDate d = from; d <= mCurrentDate; d = d.addDays(1)) {
        DailyBucket b;
        b.date = d;
        b.rx = 0;
        b.tx = 0;

        if (iface == kAllInterfaces) {
            for (const QString &key : mBuckets.keys()) {
                const auto it = mBuckets.constFind(key);
                if (it == mBuckets.constEnd()) continue;
                const auto bit = it->constFind(d);
                if (bit != it->constEnd()) {
                    b.rx += bit->rx;
                    b.tx += bit->tx;
                }
            }
        } else {
            const auto it = mBuckets.constFind(iface);
            if (it != mBuckets.constEnd()) {
                const auto bit = it->constFind(d);
                if (bit != it->constEnd()) {
                    b.rx = bit->rx;
                    b.tx = bit->tx;
                }
            }
        }

        result.append(b);
    }

    return result;
}

QStringList NetUsageTracker::trackedInterfaces() const
{
    return mBuckets.keys();
}

// ── Persistence ──────────────────────────────────────────────────────────────

void NetUsageTracker::persist()
{
    const QDate cutoff = QDate::currentDate().addDays(-90);
    QJsonObject root;

    for (auto ifaceIt = mBuckets.constBegin(); ifaceIt != mBuckets.constEnd(); ++ifaceIt) {
        QJsonObject ifaceObj;
        for (auto dayIt = ifaceIt->constBegin(); dayIt != ifaceIt->constEnd(); ++dayIt) {
            if (dayIt.key() < cutoff)
                continue;
            QJsonObject bucket;
            bucket["rx"] = static_cast<qint64>(dayIt->rx);
            bucket["tx"] = static_cast<qint64>(dayIt->tx);
            ifaceObj[dayIt.key().toString(Qt::ISODate)] = bucket;
        }
        if (!ifaceObj.isEmpty())
            root[ifaceIt.key()] = ifaceObj;
    }

    SettingManager::ins()->setNetUsageHistory(
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

void NetUsageTracker::load()
{
    const QString json = SettingManager::ins()->getNetUsageHistory();
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    for (auto ifaceIt = root.constBegin(); ifaceIt != root.constEnd(); ++ifaceIt) {
        if (!ifaceIt->isObject()) continue;
        const QJsonObject ifaceObj = ifaceIt->toObject();
        for (auto dayIt = ifaceObj.constBegin(); dayIt != ifaceObj.constEnd(); ++dayIt) {
            const QDate d = QDate::fromString(dayIt.key(), Qt::ISODate);
            if (!d.isValid()) continue;
            const QJsonObject b = dayIt->toObject();
            DailyBucket bucket;
            bucket.date = d;
            bucket.rx = static_cast<quint64>(b["rx"].toDouble());
            bucket.tx = static_cast<quint64>(b["tx"].toDouble());
            mBuckets[ifaceIt.key()][d] = bucket;
        }
    }
}
