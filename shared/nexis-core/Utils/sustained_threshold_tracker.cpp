#include "sustained_threshold_tracker.h"

void SustainedThresholdTracker::setConfig(const Config &config)
{
    mConfig = config;
    reset();
}

void SustainedThresholdTracker::reset()
{
    mAboveSinceMs = -1;
    mLastSampleMs = -1;
    mFiredThisEpisode = false;
}

bool SustainedThresholdTracker::update(double value, qint64 nowMs)
{
    const bool clockWentBackwards = mLastSampleMs >= 0 && nowMs < mLastSampleMs;
    const bool gapTooLong = mLastSampleMs >= 0 && nowMs - mLastSampleMs > mConfig.maxGapMs;
    if (clockWentBackwards || gapTooLong) {
        mAboveSinceMs = -1;
        mFiredThisEpisode = false;
    }
    mLastSampleMs = nowMs;

    if (!(value > mConfig.threshold)) {
        mAboveSinceMs = -1;
        mFiredThisEpisode = false;
        return false;
    }

    if (mAboveSinceMs < 0)
        mAboveSinceMs = nowMs;

    if (mFiredThisEpisode || nowMs - mAboveSinceMs < mConfig.durationMs)
        return false;

    if (mLastFiredMs >= 0 && nowMs >= mLastFiredMs && nowMs - mLastFiredMs < mConfig.cooldownMs)
        return false;

    mFiredThisEpisode = true;
    mLastFiredMs = nowMs;
    return true;
}
