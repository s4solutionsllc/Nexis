#include "stacer_importer.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include "Managers/setting_manager.h"

// Stacer stores config at AppConfigLocation (org=oguzhaninan, app=Stacer).
// On Linux that resolves to ~/.config/oguzhaninan/Stacer/settings.ini.
QString StacerImporter::defaultConfigPath()
{
    return QDir::homePath() + "/.config/oguzhaninan/Stacer/settings.ini";
}

StacerImportResult StacerImporter::parse(const QString &filePath, SettingManager *sm)
{
    StacerImportResult result;
    result.filePath = filePath;

    if (!QFile::exists(filePath)) {
        result.fileFound = false;
        result.errorMessage = QString("Stacer config not found at %1").arg(filePath);
        return result;
    }
    result.fileFound = true;

    QSettings s(filePath, QSettings::IniFormat);
    if (s.status() != QSettings::NoError) {
        result.errorMessage = "Could not read Stacer config file.";
        return result;
    }

    // Keys present in Stacer's settings.ini that map directly to Nexis.
    // Each entry: {stacerKey, nexisLabel, currentNexisValue, importedValue}
    struct Mapping {
        QString stacerKey;
        QString label;
        std::function<QString()> nexisGet;
        std::function<void(const QString &)> nexisSet;
    };

    const QList<Mapping> mappings = {
        {
            "CPUAlertPercent",
            "CPU alert threshold",
            [sm]() { return QString::number(sm->getCpuAlertPercent()); },
            [sm](const QString &v) { sm->setCpuAlertPercent(v.toInt()); }
        },
        {
            "MemoryAlertPercent",
            "Memory alert threshold",
            [sm]() { return QString::number(sm->getMemoryAlertPercent()); },
            [sm](const QString &v) { sm->setMemoryAlertPercent(v.toInt()); }
        },
        {
            "DiskAlertPercent",
            "Disk alert threshold",
            [sm]() { return QString::number(sm->getDiskAlertPercent()); },
            [sm](const QString &v) { sm->setDiskAlertPercent(v.toInt()); }
        },
        {
            "DiskName",
            "Default disk",
            [sm]() { return sm->getDiskName(); },
            [sm](const QString &v) { sm->setDiskName(v); }
        },
        {
            "AppQuitDialogDontAsk",
            "Skip quit confirmation dialog",
            [sm]() { return sm->getAppQuitDialogDontAsk() ? "true" : "false"; },
            [sm](const QString &v) { sm->setAppQuitDialogDontAsk(v == "true"); }
        },
        {
            "AppQuitDialogChoice",
            "Default quit action",
            [sm]() { return sm->getAppQuitDialogChoice(); },
            [sm](const QString &v) { sm->setAppQuitDialogChoice(v); }
        },
        {
            "Language",
            "Language",
            [sm]() { return sm->getLanguage(); },
            [sm](const QString &v) { sm->setLanguage(v); }
        },
        {
            "StartPage",
            "Start page",
            [sm]() { return sm->getStartPage(); },
            [sm](const QString &v) {
                sm->setStartPage(SettingManager::migrateStartPageId(v));
            }
        },
    };

    // Stacer keys known to have no Nexis counterpart.
    const QStringList knownNoEquivalent = { "ThemeName", "FontSizeOffset" };

    for (const Mapping &m : mappings) {
        if (!s.contains(m.stacerKey))
            continue;

        QString imported = s.value(m.stacerKey).toString();
        // Normalise booleans written by Qt as "true"/"false" or "1"/"0".
        if (imported == "1") imported = "true";
        if (imported == "0") imported = "false";

        // For StartPage, migrate the value before comparing.
        QString compareImported = imported;
        if (m.stacerKey == "StartPage")
            compareImported = SettingManager::migrateStartPageId(imported);

        QString current = m.nexisGet();

        if (compareImported == current) {
            result.alreadyMatch.append(m.label);
        } else {
            result.willChange.append({ m.stacerKey, m.label, current, compareImported });
        }
    }

    for (const QString &key : knownNoEquivalent) {
        if (s.contains(key))
            result.noEquivalent.append(key);
    }

    return result;
}

void StacerImporter::apply(const StacerImportResult &result, SettingManager *sm)
{
    QSettings s(result.filePath, QSettings::IniFormat);

    const QList<std::pair<QString, std::function<void(const QString &)>>> writers = {
        { "CPUAlertPercent",       [sm](const QString &v) { sm->setCpuAlertPercent(v.toInt()); } },
        { "MemoryAlertPercent",    [sm](const QString &v) { sm->setMemoryAlertPercent(v.toInt()); } },
        { "DiskAlertPercent",      [sm](const QString &v) { sm->setDiskAlertPercent(v.toInt()); } },
        { "DiskName",              [sm](const QString &v) { sm->setDiskName(v); } },
        { "AppQuitDialogDontAsk",  [sm](const QString &v) { sm->setAppQuitDialogDontAsk(v == "true"); } },
        { "AppQuitDialogChoice",   [sm](const QString &v) { sm->setAppQuitDialogChoice(v); } },
        { "Language",              [sm](const QString &v) { sm->setLanguage(v); } },
        { "StartPage",             [sm](const QString &v) {
            sm->setStartPage(SettingManager::migrateStartPageId(v));
        }},
    };

    for (const StacerMappedEntry &entry : result.willChange) {
        for (const auto &[key, writer] : writers) {
            if (entry.key == key) {
                // Reconstruct the original Stacer value from toValue.
                // For booleans, Stacer may have stored "1"/"0" — we normalised
                // to "true"/"false" in parse(); write the normalised form back
                // through the same typed setters (they accept the canonical form).
                writer(entry.toValue);
                break;
            }
        }
    }
}
