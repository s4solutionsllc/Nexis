// SSO-23860: surgical Firefox/Chromium-family browser history & cookie
// deletion (Deep Cleaning Engine, SSO-15366). Reuses the existing
// TrustSafetyPreviewDialog/TrustSafetyRunner (SSO-15380) for dry-run
// preview, live confirm, and cancel — same seam CleanerActionInterpreter
// (SSO-23859) adopts, no new confirmation dialog.

#ifndef BROWSER_SQLITE_CLEANER_H
#define BROWSER_SQLITE_CLEANER_H

#include <Common/trust_safety_types.h>
#include <Utils/browser_profile_locator.h>

#include <QSet>
#include <QString>

class BrowserSqliteCleaner : public TrustSafetyActionProvider
{
public:
    // keptCookieDomains: the selective cookie keeper allowlist. A cookie
    // survives deletion if its host matches an entry exactly or is a
    // subdomain of one (a leading "." on either side, as browsers store for
    // domain-wide cookies, is ignored for matching). Case-insensitive.
    BrowserSqliteCleaner(QList<BrowserProfileLocator::Profile> profiles,
                          QSet<QString> keptCookieDomains);

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override;

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override;

    // Exposed standalone so the keeper-matching rule is unit-testable
    // without a real database.
    static bool isCookieKept(const QString &cookieHost, const QSet<QString> &keptCookieDomains);

private:
    void scanHistory(const BrowserProfileLocator::Profile &profile,
                      const std::function<void(const TrustSafetyActionItem &)> &itemFound);
    void scanCookies(const BrowserProfileLocator::Profile &profile,
                      const std::function<void(const TrustSafetyActionItem &)> &itemFound);

    TrustSafetyActionResult deleteHistory(const QString &dbPath, bool isFirefox, bool dryRun);
    TrustSafetyActionResult deleteCookies(const QString &dbPath, bool isFirefox, bool dryRun);

    QList<BrowserProfileLocator::Profile> mProfiles;
    QSet<QString> mKeptCookieDomains;
};

#endif // BROWSER_SQLITE_CLEANER_H
