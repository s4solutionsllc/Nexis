#ifndef RESOURCE_HISTORY_STORE_H
#define RESOURCE_HISTORY_STORE_H

#include <QList>
#include <QString>
#include <optional>

// SSO-10930 / GH#220: persistent, cross-session CPU/load/memory history.
// Lives in nexis-core (not shared/nexis) so it's unit-testable without the
// GUI, mirroring RepoHealthChecker. Each instance opens its own uniquely
// named QSqlDatabase connection (BrowserSqliteCleaner's ScopedReadWriteDb
// pattern) so multiple stores (tests, or a future second caller) never
// collide on a shared connection name.
class ResourceHistoryStore
{
public:
    enum class SampleSource : int {
        LiveAggregated = 0,   // DataRefreshService was running; cpu_pct is an avg/max over the tick
        BackgroundDirect = 1  // window hidden; sampled directly off CpuInfo/MemoryInfo
    };

    struct Sample {
        qint64 ts = 0; // unix seconds, UTC
        SampleSource source = SampleSource::LiveAggregated;
        std::optional<double> cpuPct;
        std::optional<double> cpuMaxPct;
        std::optional<double> load1;
        std::optional<double> load5;
        std::optional<double> load15;
        std::optional<quint64> memUsedBytes;
        std::optional<quint64> memTotalBytes;
        std::optional<quint64> swapUsedBytes;
        std::optional<quint64> swapTotalBytes;
    };

    // One row per bucket, bucketed by (ts / bucketSecs) * bucketSecs.
    struct Bucket {
        qint64 ts = 0; // bucket start, unix seconds
        std::optional<double> cpuAvg;
        std::optional<double> cpuMax;
        std::optional<double> load1Avg;
        std::optional<double> load5Avg;
        std::optional<double> load15Avg;
        std::optional<double> memUsedAvg;
        std::optional<double> memUsedMax;
        std::optional<double> swapUsedAvg;
    };

    ResourceHistoryStore();
    ~ResourceHistoryStore();

    // Opens (creating if needed) the SQLite file at path under a fresh,
    // uniquely named connection. If the file exists but fails to open or
    // fails PRAGMA integrity_check, it is renamed to
    // "<path>.corrupt-<unixtime>" and a fresh database is created in its
    // place — callers never see a throw or a crash, only a degraded (but
    // working) store.
    bool open(const QString &path);
    void close();
    bool isOpen() const { return mOpen; }

    // Single transaction. Returns false (and logs) on any failure; never
    // partially commits.
    bool appendBatch(const QList<Sample> &samples);

    // Server-side bucketing so a UI range never has to pull more than a
    // couple hundred rows to draw a chart. Half-open on neither end:
    // fromTs and toTs are both inclusive.
    QList<Bucket> queryBuckets(qint64 fromTs, qint64 toTs, qint64 bucketSecs) const;

    // Raw rows, ordered by ts ascending — used for CSV export.
    QList<Sample> queryRaw(qint64 fromTs, qint64 toTs) const;

    bool pruneOlderThan(qint64 ts);

    // Deletes every row and VACUUMs to actually reclaim disk space.
    bool clearAll();

    qint64 rowCount() const;

    QString lastError() const { return mLastError; }

private:
    bool ensureSchema();
    bool openFresh(const QString &path);

    QString mConnName;
    QString mPath;
    bool mOpen = false;
    mutable QString mLastError;
};

#endif // RESOURCE_HISTORY_STORE_H
