#include "rolling_series.h"

RollingSeries::RollingSeries(const QVector<QXYSeries *> &series, double windowSeconds, int capacity)
    : mSeries(series),
      mWindowSeconds(windowSeconds),
      mCapacity(capacity)
{
    mBuffers.resize(mSeries.size());
}

void RollingSeries::push(const QVector<double> &values)
{
    if (!mClockStarted) {
        mClock.start();
        mClockStarted = true;
        mLastTickMs = 0;
    }

    const qint64 nowMs = mClock.elapsed();
    const double deltaSeconds = qMax(0.0, (nowMs - mLastTickMs) / 1000.0);
    mLastTickMs = nowMs;

    for (int j = 0; j < mBuffers.size(); ++j) {
        QList<QPointF> &buf = mBuffers[j];

        // Age every existing point by the real elapsed time since the last
        // push (B11) rather than by one fixed unit per tick.
        if (deltaSeconds > 0.0) {
            for (QPointF &pt : buf)
                pt.setX(pt.x() + deltaSeconds);
        }

        if (j < values.size())
            buf.prepend(QPointF(0.0, values.at(j)));

        // Evict anything that aged out of the window, then enforce the hard
        // capacity regardless of the window — keeps this a fixed-capacity
        // ring buffer even if the tick interval were ever much faster than
        // expected.
        while (!buf.isEmpty() && buf.last().x() > mWindowSeconds)
            buf.removeLast();
        while (buf.size() > mCapacity)
            buf.removeLast();

        if (j < mSeries.size() && mSeries.at(j)) {
            mSeries.at(j)->replace(buf);
            ++mReplaceCallCount;
        }
    }
}

double RollingSeries::windowYMax(double yFloor) const
{
    double maxVal = yFloor;
    for (const QList<QPointF> &buf : mBuffers) {
        for (const QPointF &pt : buf)
            maxVal = qMax(maxVal, pt.y());
    }
    return maxVal;
}

void RollingSeries::reset()
{
    for (int j = 0; j < mBuffers.size(); ++j) {
        mBuffers[j].clear();
        if (j < mSeries.size() && mSeries.at(j)) {
            mSeries.at(j)->replace(mBuffers[j]);
            ++mReplaceCallCount;
        }
    }
    mClockStarted = false;
    mLastTickMs = 0;
}
