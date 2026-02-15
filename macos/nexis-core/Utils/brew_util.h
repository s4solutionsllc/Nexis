#ifndef BREW_UTIL_H
#define BREW_UTIL_H

#include <QString>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Homebrew binary path — checked at well-known locations since GUI apps
// don't inherit the user's shell PATH.
inline QString findBrew()
{
    for (const QString &path : {"/opt/homebrew/bin/brew", "/usr/local/bin/brew"}) {
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return QString();
}

// Lightweight representation of a Homebrew package (formula or cask).
// Used as an intermediate structure before mapping to domain types
// (Package, APTSource, etc.).
struct BrewEntry {
    QString identifier;     // formula "name" or cask "token"
    QString displayName;    // human-readable name (same as identifier for formulae)
    QString description;    // "desc" field
    bool isCask = false;    // true for casks, false for formulae
};

// Parse the JSON output of `brew info --json=v2 --installed` and return
// a flat list of installed formulae and casks.
inline QList<BrewEntry> parseBrewJson(const QJsonDocument &doc)
{
    QList<BrewEntry> entries;
    if (doc.isNull())
        return entries;

    QJsonObject root = doc.object();

    // Installed formulae
    QJsonArray formulae = root.value("formulae").toArray();
    for (const QJsonValue &val : formulae) {
        QJsonObject obj = val.toObject();
        BrewEntry e;
        e.identifier  = obj.value("name").toString();
        e.displayName = e.identifier;
        e.description = obj.value("desc").toString();
        e.isCask      = false;
        if (!e.identifier.isEmpty())
            entries.append(e);
    }

    // Installed casks
    QJsonArray casks = root.value("casks").toArray();
    for (const QJsonValue &val : casks) {
        QJsonObject obj = val.toObject();
        BrewEntry e;
        e.identifier = obj.value("token").toString();
        e.isCask     = true;

        // Cask "name" is an array of human-friendly names (e.g. ["AltTab"])
        QJsonArray names = obj.value("name").toArray();
        e.displayName = names.isEmpty() ? e.identifier
                                        : names.first().toString();

        e.description = obj.value("desc").toString();

        if (!e.identifier.isEmpty())
            entries.append(e);
    }

    return entries;
}

#endif // BREW_UTIL_H
