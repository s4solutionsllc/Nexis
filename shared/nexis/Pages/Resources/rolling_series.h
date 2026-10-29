#ifndef ROLLINGSERIES_H
#define ROLLINGSERIES_H

#include <QVector>
#include <QList>
#include <QPointF>
#include <QtCharts>
#include <QElapsedTimer>

// SSO-26718: fixed-capacity ring buffer feeding N QXYSeries from one shared
// per-tick push, replacing the Resources page's copy-pasted "shift every
// point one-by-one, insert, trim" loops (B9/B10/B15 on SSO-26716's audit).
// Every push() does exactly one QXYSeries::replace(QList<QPointF>) call per
// series — not N point-by-point replace()/insert() calls.
//
// X values are real elapsed seconds, tracked via QElapsedTimer, rather than
// a sample count (B11) — DataRefreshService's tick interval changes with
// PowerMode (1s/2s/5s — data_refresh_service.cpp), so a fixed "one sample =
// one second" assumption silently lies on battery/unfocused.
class RollingSeries
{
public:
    // series: one QXYSeries per tracked value, pushed together each tick.
    // windowSeconds: points older than this age out of both the chart and
    // the windowed Y max (see windowYMax()). capacity: hard cap on buffered
    // points per series regardless of windowSeconds, so the buffer stays
    // fixed-capacity even under a pathologically fast tick.
    explicit RollingSeries(const QVector<QXYSeries *> &series,
                           double windowSeconds = 60.0,
                           int capacity = 256);

    // Pushes one new value per entry in `values`. values.size() may be less
    // than series().size() (e.g. a GPU that dropped out this tick, or a
    // drive that stopped reporting a temperature) — the remaining series are
    // still aged and re-sent, just without a new point prepended. Always
    // issues one replace() per series, since aging shifts every existing
    // point's X this tick.
    void push(const QVector<double> &values);

    // Max Y value among all currently buffered points, floored at yFloor.
    // Shrinks on its own as old spikes age out of the window (B9) — callers
    // feed the result into HistoryChart::setYMax() after each push().
    double windowYMax(double yFloor) const;

    // Clears all buffered samples and restarts the elapsed-time clock. Call
    // on page re-activate (B12) so a hidden gap never renders as a frozen,
    // stale chart — it starts collecting fresh samples from empty instead.
    void reset();

    double windowSeconds() const { return mWindowSeconds; }
    int capacity() const { return mCapacity; }

    // Exposed for the SSO-26718 PR's before/after point-write measurement
    // (a debug counter is explicitly acceptable per the issue's acceptance
    // criteria) — not used by production logic.
    qint64 replaceCallCount() const { return mReplaceCallCount; }

private:
    QVector<QXYSeries *> mSeries;
    double mWindowSeconds;
    int mCapacity;
    QVector<QList<QPointF>> mBuffers; // per-series ring buffer, front = newest (x = 0)
    QElapsedTimer mClock;
    bool mClockStarted = false;
    qint64 mLastTickMs = 0;
    qint64 mReplaceCallCount = 0;
};

#endif // ROLLINGSERIES_H
