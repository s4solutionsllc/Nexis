#ifndef SUSTAINED_THRESHOLD_TRACKER_H
#define SUSTAINED_THRESHOLD_TRACKER_H

#include "nexis-core_global.h"

#include <QtGlobal>

// SSO-27148: decides when a metric has stayed above a threshold long enough to
// warrant a notification. Pure windowing logic (no Qt event loop, no clock of
// its own) so it is unit-testable with synthetic timestamps; the caller feeds
// monotonic milliseconds.
//
// One notification per episode: once fired, the tracker stays quiet until the
// value drops back to or below the threshold. A new episode within `cooldownMs`
// of the last notification is held back (and fires the moment the cooldown
// expires if the metric is still above), so a metric oscillating around the
// threshold cannot spam the tray.
class NEXISCORESHARED_EXPORT SustainedThresholdTracker
{
public:
    struct Config {
        double threshold = 0.0;     // value must be strictly greater to count
        qint64 durationMs = 0;      // 0 = fire on the first sample above
        qint64 cooldownMs = 0;      // minimum spacing between notifications
        // A gap between samples longer than this (window hidden, system
        // asleep) means the "sustained" claim can no longer be vouched for,
        // so the episode restarts instead of spanning the gap.
        qint64 maxGapMs = 30000;
    };

    SustainedThresholdTracker() = default;
    explicit SustainedThresholdTracker(const Config &config) : mConfig(config) {}

    void setConfig(const Config &config);
    const Config &config() const { return mConfig; }

    // Feed one sample; returns true when a notification should be raised now.
    bool update(double value, qint64 nowMs);

    // Forget the current episode (not the last-notified time, so disabling and
    // re-enabling an alert cannot be used to bypass the cooldown).
    void reset();

    bool isAbove() const { return mAboveSinceMs >= 0; }

private:
    Config mConfig;
    qint64 mAboveSinceMs = -1;
    qint64 mLastSampleMs = -1;
    qint64 mLastFiredMs = -1;
    bool mFiredThisEpisode = false;
};

#endif // SUSTAINED_THRESHOLD_TRACKER_H
