#include "nvidia_smi_cache.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>

#include "Utils/command_util.h"

#include <climits>

namespace NvidiaSmiCache {

namespace {

struct Cache {
    QMutex mutex;
    QHash<int, Sample> samples;
    QElapsedTimer lastRefresh;
    bool everRefreshed = false;
};

Cache &cache()
{
    static Cache c;
    return c;
}

} // namespace

void refresh()
{
    if (!CommandUtil::isExecutable("nvidia-smi"))
        return;

    // One invocation covers every NVIDIA device. index first so we can map
    // results back to the caller's indices regardless of enumeration order.
    ExecResult result;
    try {
        result = CommandUtil::execWithStatus(
            "nvidia-smi",
            {"--query-gpu=index,utilization.gpu,fan.speed",
             "--format=csv,noheader,nounits"},
            3000);
    } catch (...) {
        qWarning() << "nvidia-smi batched query failed";
        return;
    }

    if (result.exitCode != 0)
        return;

    QHash<int, Sample> newSamples;
    const QStringList lines = result.output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QStringList parts = line.split(',');
        if (parts.size() < 3)
            continue;

        bool idxOk = false;
        const int idx = parts.at(0).trimmed().toInt(&idxOk);
        if (!idxOk)
            continue;

        Sample s;
        bool ok = false;

        // utilization.gpu may be "[N/A]" on some cards/drivers
        const QString utilStr = parts.at(1).trimmed();
        int util = utilStr.toInt(&ok);
        s.utilization = ok ? util : -1;

        const QString fanStr = parts.at(2).trimmed();
        int fan = fanStr.toInt(&ok);
        s.fanPercent = ok ? fan : -1;

        newSamples.insert(idx, s);
    }

    Cache &c = cache();
    QMutexLocker lock(&c.mutex);
    c.samples = std::move(newSamples);
    c.lastRefresh.start();
    c.everRefreshed = true;
}

Sample get(int index)
{
    Cache &c = cache();
    QMutexLocker lock(&c.mutex);
    return c.samples.value(index, Sample{});
}

int ageMs()
{
    Cache &c = cache();
    QMutexLocker lock(&c.mutex);
    if (!c.everRefreshed)
        return INT_MAX;
    return static_cast<int>(c.lastRefresh.elapsed());
}

} // namespace NvidiaSmiCache
