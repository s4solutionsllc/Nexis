#include "Info/power_profile_info.h"
#include <QRegularExpression>

PowerProfileData PowerProfileInfo::getData() const
{
    return mData;
}

bool PowerProfileInfo::hasProfiles() const
{
    return mData.backend != PowerBackend::None && !mData.availableProfiles.isEmpty();
}

PowerProfileData PowerProfileInfo::parsePowerprofilesctlList(const QString &output)
{
    PowerProfileData data;
    data.backend = PowerBackend::PowerProfilesDaemon;

    static const QRegularExpression profileRe(R"(^\s{0,2}(\*?\s*)(\S+):\s*$)");

    const QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        QRegularExpressionMatch m = profileRe.match(line);
        if (!m.hasMatch())
            continue;

        bool isActive = m.captured(1).contains('*');
        QString profile = m.captured(2).trimmed();

        if (!profile.isEmpty() && !data.availableProfiles.contains(profile))
            data.availableProfiles.append(profile);

        if (isActive)
            data.activeProfile = profile;
    }

    if (data.activeProfile.isEmpty() && !data.availableProfiles.isEmpty())
        data.activeProfile = data.availableProfiles.first();

    return data;
}

PowerProfileData PowerProfileInfo::parseSysfsGovernors(const QString &availableGovernors,
                                                       const QString &currentGovernor,
                                                       const QString &scalingDriver)
{
    PowerProfileData data;
    data.backend = PowerBackend::Sysfs;
    data.scalingDriver = scalingDriver.trimmed();
    data.activeProfile = currentGovernor.trimmed();

    const QStringList govs = availableGovernors.trimmed().split(
        QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    for (const QString &g : govs) {
        QString trimmed = g.trimmed();
        if (!trimmed.isEmpty())
            data.availableProfiles.append(trimmed);
    }

    return data;
}

QString PowerProfileInfo::userLabelToBackendValue(const QString &label, PowerBackend backend)
{
    const QString lower = label.toLower();

    if (backend == PowerBackend::PowerProfilesDaemon) {
        if (lower == "performance")  return QStringLiteral("performance");
        if (lower == "balanced")     return QStringLiteral("balanced");
        if (lower == "power saver")  return QStringLiteral("power-saver");
        return label;
    }

    if (lower == "performance")  return QStringLiteral("performance");
    if (lower == "balanced")     return QStringLiteral("schedutil");
    if (lower == "power saver")  return QStringLiteral("powersave");
    return label;
}

QString PowerProfileInfo::backendValueToUserLabel(const QString &value, PowerBackend backend)
{
    if (backend == PowerBackend::PowerProfilesDaemon) {
        if (value == "performance")  return QStringLiteral("Performance");
        if (value == "balanced")     return QStringLiteral("Balanced");
        if (value == "power-saver")  return QStringLiteral("Power Saver");
        return value;
    }

    if (value == "performance")  return QStringLiteral("Performance");
    if (value == "powersave")    return QStringLiteral("Power Saver");
    if (value == "schedutil" || value == "ondemand" || value == "conservative")
        return QStringLiteral("Balanced");
    return value;
}
