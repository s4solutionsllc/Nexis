#include "sparkle_update_floor_store.h"

#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>

namespace {

QString s_testBackingPath;

QString backingFilePath()
{
    if (!s_testBackingPath.isEmpty())
        return s_testBackingPath;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QLatin1String("/sparkle_update_floors.ini");
}

// bundleId is a reverse-DNS identifier (e.g. "com.example.App") and never
// contains '/', so it is safe to use directly as a flat QSettings key.
QSettings backingSettings()
{
    return QSettings(backingFilePath(), QSettings::IniFormat);
}

} // namespace

QString SparkleUpdateFloorStore::floorVersion(const QString &bundleId)
{
    if (bundleId.isEmpty())
        return {};
    QSettings settings = backingSettings();
    return settings.value(bundleId).toString();
}

void SparkleUpdateFloorStore::initializeIfAbsent(const QString &bundleId,
                                                 const QString &installedVersion)
{
    if (bundleId.isEmpty() || installedVersion.isEmpty())
        return;
    QSettings settings = backingSettings();
    if (settings.contains(bundleId))
        return;
    settings.setValue(bundleId, installedVersion);
}

void SparkleUpdateFloorStore::ratchetAfterVerifiedInstall(const QString &bundleId,
                                                           const QString &newVersion)
{
    if (bundleId.isEmpty() || newVersion.isEmpty())
        return;
    QSettings settings = backingSettings();
    const QVersionNumber current = QVersionNumber::fromString(settings.value(bundleId).toString());
    const QVersionNumber incoming = QVersionNumber::fromString(newVersion);
    if (current.isNull() || incoming > current)
        settings.setValue(bundleId, newVersion);
}

void SparkleUpdateFloorStore::setBackingFilePathForTesting(const QString &path)
{
    s_testBackingPath = path;
}
