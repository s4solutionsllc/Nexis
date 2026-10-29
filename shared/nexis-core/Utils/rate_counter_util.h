#ifndef RATE_COUNTER_UTIL_H
#define RATE_COUNTER_UTIL_H

#include "nexis-core_global.h"

#include <QtGlobal>

// SSO-26717 (B1): shared guard for monotonically-increasing byte/IO counters
// (disk read/write, network RX/TX) that can occasionally step backwards —
// device re-enumeration, or a sample that fails and reports 0. Unsigned
// subtraction on a backward step would wrap to ~16 EiB, which then latches a
// chart's Y axis at that scale for the rest of the session.
class NEXISCORESHARED_EXPORT RateCounterUtil
{
public:
    static quint64 guardedDelta(quint64 previous, quint64 current);
};

#endif // RATE_COUNTER_UTIL_H
