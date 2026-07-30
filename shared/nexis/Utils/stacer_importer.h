#pragma once

#include <QString>
#include <QList>

class SettingManager;

// One setting that will be changed by the import.
struct StacerMappedEntry {
    QString key;        // SettingKeys constant
    QString label;      // Human-readable label shown in the preview
    QString fromValue;  // Current Nexis value
    QString toValue;    // Value read from Stacer config
};

struct StacerImportResult {
    bool fileFound = false;
    QString filePath;
    QString errorMessage;

    QList<StacerMappedEntry> willChange;   // settings that differ and will be written
    QStringList alreadyMatch;              // settings present in Stacer but identical to Nexis
    QStringList noEquivalent;              // Stacer keys that have no Nexis counterpart
};

class StacerImporter
{
public:
    // Returns the canonical Stacer config path for the current user.
    // (~/.config/oguzhaninan/Stacer/settings.ini on Linux)
    static QString defaultConfigPath();

    // Reads filePath and compares against sm's current values.
    // Returns a result describing what would change; nothing is written.
    static StacerImportResult parse(const QString &filePath, SettingManager *sm);

    // Applies result.willChange to sm. Call only after the user confirms.
    static void apply(const StacerImportResult &result, SettingManager *sm);
};
