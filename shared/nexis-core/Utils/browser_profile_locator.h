#ifndef BROWSER_PROFILE_LOCATOR_H
#define BROWSER_PROFILE_LOCATOR_H

#include <QList>
#include <QString>

#include "nexis-core_global.h"

// SSO-23860: locates installed Firefox / Chromium-family browser profiles and
// their history/cookie SQLite databases for the Deep Cleaning Engine's
// browser-specific surgical row deletion. CleanerActionInterpreter (SSO-23859)
// only resolves the generic $$home$$/$$cache$$ CleanerML variables — an
// app-specific token like $$profile$$ has no meaning there, since knowing
// where a browser keeps its profiles requires per-browser knowledge this
// class owns instead.
class NEXISCORESHARED_EXPORT BrowserProfileLocator
{
public:
    enum class Family {
        Firefox,
        Chromium,
    };

    struct Profile {
        Family family = Family::Firefox;
        QString browserName;    // e.g. "Firefox", "Google Chrome", "Brave"
        QString profileName;    // e.g. "default-release", "Default", "Profile 1"
        QString profileDir;     // absolute profile directory
        QString historyDbPath;  // places.sqlite (Firefox) / History (Chromium); empty if absent
        QString cookiesDbPath;  // cookies.sqlite (Firefox) / Cookies (Chromium); empty if absent
    };

    // Scans platform-conventional install locations under homeDir (defaults
    // to QStandardPaths::HomeLocation when empty) and returns one Profile per
    // discovered browser profile that has at least a history or cookies DB
    // present. Never touches DB contents — file-existence discovery only.
    static QList<Profile> detectProfiles(const QString &homeDir = QString());

private:
    BrowserProfileLocator();
};

#endif // BROWSER_PROFILE_LOCATOR_H
