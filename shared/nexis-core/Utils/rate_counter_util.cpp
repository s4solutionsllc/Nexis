#include "rate_counter_util.h"

quint64 RateCounterUtil::guardedDelta(quint64 previous, quint64 current)
{
    return current >= previous ? current - previous : 0;
}
