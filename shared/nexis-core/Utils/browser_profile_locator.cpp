#include "browser_profile_locator.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

struct ChromiumBrowser {
    QString name;
    QString configSubdir; // relative to the platform's browser-config base dir
};

// Chromium-family browsers Nexis recognizes. Every entry stores its profile
// data under <base>/<configSubdir>/{Default,Profile N}.
QList<ChromiumBrowser> chromiumBrowsers()
{
    return {
        {QStringLiteral("Google Chrome"),
#ifdef Q_OS_MAC
         QStringLiteral("Google/Chrome")},
#else
         QStringLiteral("google-chrome")},
#endif
        {QStringLiteral("Chromium"),
#ifdef Q_OS_MAC
         QStringLiteral("Chromium")},
#else
         QStringLiteral("chromium")},
#endif
        // Same config subdir name on both platforms.
        {QStringLiteral("Brave"), QStringLiteral("BraveSoftware/Brave-Browser")},
        {QStringLiteral("Microsoft Edge"),
#ifdef Q_OS_MAC
         QStringLiteral("Microsoft Edge")},
#else
         QStringLiteral("microsoft-edge")},
#endif
    };
}

#ifdef Q_OS_MAC
QString chromiumConfigBase(const QString &homeDir)
{
    return homeDir + QStringLiteral("/Library/Application Support");
}
QString firefoxProfilesBase(const QString &homeDir)
{
    return homeDir + QStringLiteral("/Library/Application Support/Firefox/Profiles");
}
#else
QString chromiumConfigBase(const QString &homeDir)
{
    return homeDir + QStringLiteral("/.config");
}
QString firefoxProfilesBase(const QString &homeDir)
{
    return homeDir + QStringLiteral("/.mozilla/firefox");
}
#endif

// Chrome 96+ moved Cookies under Network/; older releases (and some forks)
// still keep it at the profile root. Check both, preferring the modern path.
QString findChromiumCookiesDb(const QString &profileDir)
{
    const QString modern = profileDir + QStringLiteral("/Network/Cookies");
    if (QFileInfo::exists(modern))
        return modern;
    const QString legacy = profileDir + QStringLiteral("/Cookies");
    if (QFileInfo::exists(legacy))
        return legacy;
    return QString();
}

void appendChromiumProfiles(const QString &homeDir, QList<BrowserProfileLocator::Profile> &out)
{
    const QString base = chromiumConfigBase(homeDir);

    for (const ChromiumBrowser &browser : chromiumBrowsers()) {
        QDir browserDir(base + QLatin1Char('/') + browser.configSubdir);
        if (!browserDir.exists())
            continue;

        const QFileInfoList entries =
            browserDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            const QString name = entry.fileName();
            if (name != QLatin1String("Default") && !name.startsWith(QLatin1String("Profile ")))
                continue;

            const QString profileDir = entry.absoluteFilePath();
            const QString historyDb = profileDir + QStringLiteral("/History");
            const QString cookiesDb = findChromiumCookiesDb(profileDir);
            const bool hasHistory = QFileInfo::exists(historyDb);

            if (!hasHistory && cookiesDb.isEmpty())
                continue;

            BrowserProfileLocator::Profile profile;
            profile.family = BrowserProfileLocator::Family::Chromium;
            profile.browserName = browser.name;
            profile.profileName = name;
            profile.profileDir = profileDir;
            profile.historyDbPath = hasHistory ? historyDb : QString();
            profile.cookiesDbPath = cookiesDb;
            out.append(profile);
        }
    }
}

void appendFirefoxProfiles(const QString &homeDir, QList<BrowserProfileLocator::Profile> &out)
{
    QDir profilesDir(firefoxProfilesBase(homeDir));
    if (!profilesDir.exists())
        return;

    const QFileInfoList entries = profilesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString profileDir = entry.absoluteFilePath();
        const QString historyDb = profileDir + QStringLiteral("/places.sqlite");
        const QString cookiesDb = profileDir + QStringLiteral("/cookies.sqlite");
        const bool hasHistory = QFileInfo::exists(historyDb);
        const bool hasCookies = QFileInfo::exists(cookiesDb);

        if (!hasHistory && !hasCookies)
            continue;

        BrowserProfileLocator::Profile profile;
        profile.family = BrowserProfileLocator::Family::Firefox;
        profile.browserName = QStringLiteral("Firefox");
        profile.profileName = entry.fileName();
        profile.profileDir = profileDir;
        profile.historyDbPath = hasHistory ? historyDb : QString();
        profile.cookiesDbPath = hasCookies ? cookiesDb : QString();
        out.append(profile);
    }
}

} // namespace

BrowserProfileLocator::BrowserProfileLocator()
{
}

QList<BrowserProfileLocator::Profile> BrowserProfileLocator::detectProfiles(const QString &homeDir)
{
    const QString home =
        homeDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : homeDir;

    QList<Profile> profiles;
    if (home.isEmpty())
        return profiles;

    appendFirefoxProfiles(home, profiles);
    appendChromiumProfiles(home, profiles);
    return profiles;
}
