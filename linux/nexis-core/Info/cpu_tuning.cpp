#include "cpu_tuning.h"

#include "Utils/command_util.h"
#include "Utils/file_util.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace CpuTuning {

namespace {

constexpr const char *kCpuRoot        = "/sys/devices/system/cpu";
constexpr const char *kCpufreqBoost   = "/sys/devices/system/cpu/cpufreq/boost";
constexpr const char *kPstateNoTurbo  = "/sys/devices/system/cpu/intel_pstate/no_turbo";

QString cpuPath(int index, const QString &sub)
{
    return QStringLiteral("%1/cpu%2/cpufreq/%3")
        .arg(QString::fromLatin1(kCpuRoot)).arg(index).arg(sub);
}

QList<int> onlineCpuIndices()
{
    QList<int> indices;
    QDir root(kCpuRoot);
    static const QRegularExpression re(R"(^cpu(\d+)$)");
    for (const QString &name :
         root.entryList(QStringList() << "cpu[0-9]*",
                        QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto m = re.match(name);
        if (!m.hasMatch())
            continue;
        // Only include entries that actually have a cpufreq subdirectory —
        // offline or disabled cores lack it.
        if (!QFileInfo::exists(cpuPath(m.captured(1).toInt(), QString())))
            continue;
        indices.append(m.captured(1).toInt());
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

quint64 readUlongLong(const QString &path)
{
    const QString s = FileUtil::readStringFromFile(path).trimmed();
    bool ok = false;
    const quint64 v = s.toULongLong(&ok);
    return ok ? v : 0;
}

Turbo readTurbo(const QString &driver)
{
    // Intel pstate: /sys/.../intel_pstate/no_turbo — 0 = turbo on, 1 = off.
    if (QFileInfo::exists(kPstateNoTurbo)) {
        const QString s = FileUtil::readStringFromFile(kPstateNoTurbo).trimmed();
        return (s == "0") ? Turbo::On : Turbo::Off;
    }
    // acpi-cpufreq / amd-pstate (when not in active mode):
    if (QFileInfo::exists(kCpufreqBoost)) {
        const QString s = FileUtil::readStringFromFile(kCpufreqBoost).trimmed();
        return (s == "1") ? Turbo::On : Turbo::Off;
    }
    Q_UNUSED(driver)
    return Turbo::Unsupported;
}

} // namespace

Snapshot readSnapshot()
{
    Snapshot s;
    const auto indices = onlineCpuIndices();
    if (indices.isEmpty())
        return s;
    s.available = true;

    s.scalingDriver = FileUtil::readStringFromFile(
        cpuPath(indices.first(), "scaling_driver")).trimmed();

    const QString governors = FileUtil::readStringFromFile(
        cpuPath(indices.first(), "scaling_available_governors")).trimmed();
    if (!governors.isEmpty())
        s.availableGovernors = governors.split(' ', Qt::SkipEmptyParts);

    s.turbo = readTurbo(s.scalingDriver);

    for (int idx : indices) {
        CoreSnapshot c;
        c.index        = idx;
        c.governor     = FileUtil::readStringFromFile(
                            cpuPath(idx, "scaling_governor")).trimmed();
        c.scalingMinKHz = readUlongLong(cpuPath(idx, "scaling_min_freq"));
        c.scalingMaxKHz = readUlongLong(cpuPath(idx, "scaling_max_freq"));
        c.cpuinfoMinKHz = readUlongLong(cpuPath(idx, "cpuinfo_min_freq"));
        c.cpuinfoMaxKHz = readUlongLong(cpuPath(idx, "cpuinfo_max_freq"));
        s.cores.append(c);
    }

    return s;
}

bool writeFreqRange(quint64 minKHz, quint64 maxKHz)
{
    if (minKHz == 0 || maxKHz == 0 || minKHz > maxKHz)
        return false;

    // Kernel enforces min <= max atomically per-file. If we lower max below
    // current min we have to write min first, else we'd get EINVAL. Write
    // min first unconditionally — harmless otherwise.
    const QString cmd = QStringLiteral(
        "echo %1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq >/dev/null "
        "&& echo %2 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq >/dev/null")
        .arg(minKHz).arg(maxKHz);

    // FR-117 (SSO-3367/SSO-3471): sudoExecWithStatus's exit code is now
    // authoritative for the pkexec write, replacing the sysfs read-back.
    const ExecResult result = CommandUtil::sudoExecWithStatus("sh", {"-c", cmd});
    if (!result.ok())
        qWarning() << "cpu_tuning: failed to write freq range" << minKHz << "-" << maxKHz << ":" << result.error;
    return result.ok();
}

bool writeTurbo(bool on)
{
    if (QFileInfo::exists(kPstateNoTurbo)) {
        const int wanted = on ? 0 : 1;
        const ExecResult result = CommandUtil::sudoExecWithStatus("sh", {"-c",
            QStringLiteral("echo %1 > %2").arg(wanted).arg(kPstateNoTurbo)});
        if (!result.ok())
            qWarning() << "cpu_tuning: failed to write turbo (intel_pstate) state" << wanted << ":" << result.error;
        return result.ok();
    }
    if (QFileInfo::exists(kCpufreqBoost)) {
        const int wanted = on ? 1 : 0;
        const ExecResult result = CommandUtil::sudoExecWithStatus("sh", {"-c",
            QStringLiteral("echo %1 > %2").arg(wanted).arg(kCpufreqBoost)});
        if (!result.ok())
            qWarning() << "cpu_tuning: failed to write turbo (cpufreq boost) state" << wanted << ":" << result.error;
        return result.ok();
    }
    return false;
}

bool writeGovernor(int cpuIndex, const QString &governor)
{
    if (governor.isEmpty() || governor.contains(QRegularExpression("[^a-zA-Z_]")))
        return false;   // defensive — these go through sh -c

    if (cpuIndex < 0) {
        const QString cmd = QStringLiteral(
            "echo %1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null")
            .arg(governor);
        const ExecResult result = CommandUtil::sudoExecWithStatus("sh", {"-c", cmd});
        if (!result.ok())
            qWarning() << "cpu_tuning: failed to write governor" << governor << "(all cores):" << result.error;
        return result.ok();
    }

    const QString cmd = QStringLiteral("echo %1 > %2")
        .arg(governor).arg(cpuPath(cpuIndex, "scaling_governor"));
    const ExecResult result = CommandUtil::sudoExecWithStatus("sh", {"-c", cmd});
    if (!result.ok())
        qWarning() << "cpu_tuning: failed to write governor" << governor << "for cpu" << cpuIndex << ":" << result.error;
    return result.ok();
}

bool writePerCoreGovernors(const QList<QPair<int, QString>> &perCore)
{
    if (perCore.isEmpty())
        return false;

    // Validate all inputs before composing the shell command — refuse
    // anything that could be mistaken for an injection.
    QStringList stanzas;
    stanzas.reserve(perCore.size());
    for (const auto &pair : perCore) {
        const int idx = pair.first;
        const QString gov = pair.second;
        if (idx < 0)
            return false;
        if (gov.isEmpty() || gov.contains(QRegularExpression("[^a-zA-Z_]")))
            return false;
        stanzas << QStringLiteral("echo %1 > %2")
            .arg(gov).arg(cpuPath(idx, "scaling_governor"));
    }

    const QString cmd = stanzas.join(" && ");
    const ExecResult result = CommandUtil::sudoExecWithStatus("sh", {"-c", cmd});
    if (!result.ok())
        qWarning() << "cpu_tuning: failed to write per-core governors:" << result.error;
    return result.ok();
}

} // namespace CpuTuning
