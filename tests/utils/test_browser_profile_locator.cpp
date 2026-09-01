// SSO-23860: BrowserProfileLocator — discovers Firefox / Chromium-family
// browser profile directories and their history/cookie DB paths under a
// caller-supplied home dir, so this is unit-testable without touching the
// real $HOME.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "browser_profile_locator.h"

namespace {

void writeFile(const QString &path, const QByteArray &content = "x")
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
    f.close();
}

} // namespace

class TestBrowserProfileLocator : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void firefox_profileWithBothDbs_isDetected();
    void firefox_profileWithNoDbs_isSkipped();
    void chromium_defaultProfile_isDetected();
    void chromium_numberedProfile_isDetected();
    void chromium_prefersNetworkCookiesOverLegacyPath();
    void chromium_nonProfileDirectory_isIgnored();
    void noInstalledBrowsers_returnsEmpty();

private:
    QScopedPointer<QTemporaryDir> mTmp;
    QString mHome;

#ifdef Q_OS_MAC
    QString firefoxProfilesBase() const { return mHome + "/Library/Application Support/Firefox/Profiles"; }
    QString chromiumBase(const QString &subdir) const { return mHome + "/Library/Application Support/" + subdir; }
#else
    QString firefoxProfilesBase() const { return mHome + "/.mozilla/firefox"; }
    QString chromiumBase(const QString &subdir) const { return mHome + "/.config/" + subdir; }
#endif
};

void TestBrowserProfileLocator::init()
{
    mTmp.reset(new QTemporaryDir());
    QVERIFY(mTmp->isValid());
    mHome = mTmp->filePath("home");
    QVERIFY(QDir().mkpath(mHome));
}

void TestBrowserProfileLocator::firefox_profileWithBothDbs_isDetected()
{
    const QString profileDir = firefoxProfilesBase() + "/abc123.default-release";
    writeFile(profileDir + "/places.sqlite");
    writeFile(profileDir + "/cookies.sqlite");

    const auto profiles = BrowserProfileLocator::detectProfiles(mHome);
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().family, BrowserProfileLocator::Family::Firefox);
    QCOMPARE(profiles.first().browserName, QStringLiteral("Firefox"));
    QCOMPARE(profiles.first().profileName, QStringLiteral("abc123.default-release"));
    QCOMPARE(profiles.first().historyDbPath, profileDir + "/places.sqlite");
    QCOMPARE(profiles.first().cookiesDbPath, profileDir + "/cookies.sqlite");
}

void TestBrowserProfileLocator::firefox_profileWithNoDbs_isSkipped()
{
    const QString profileDir = firefoxProfilesBase() + "/empty.default";
    QVERIFY(QDir().mkpath(profileDir));

    QVERIFY(BrowserProfileLocator::detectProfiles(mHome).isEmpty());
}

void TestBrowserProfileLocator::chromium_defaultProfile_isDetected()
{
#ifdef Q_OS_MAC
    const QString profileDir = mHome + "/Library/Application Support/Google/Chrome/Default";
#else
    const QString profileDir = chromiumBase("google-chrome") + "/Default";
#endif
    writeFile(profileDir + "/History");
    writeFile(profileDir + "/Cookies");

    const auto profiles = BrowserProfileLocator::detectProfiles(mHome);
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().family, BrowserProfileLocator::Family::Chromium);
    QCOMPARE(profiles.first().browserName, QStringLiteral("Google Chrome"));
    QCOMPARE(profiles.first().profileName, QStringLiteral("Default"));
    QVERIFY(profiles.first().historyDbPath.endsWith("/History"));
    QVERIFY(profiles.first().cookiesDbPath.endsWith("/Cookies"));
}

void TestBrowserProfileLocator::chromium_numberedProfile_isDetected()
{
#ifdef Q_OS_MAC
    const QString profileDir = mHome + "/Library/Application Support/BraveSoftware/Brave-Browser/Profile 1";
#else
    const QString profileDir = chromiumBase("BraveSoftware/Brave-Browser") + "/Profile 1";
#endif
    writeFile(profileDir + "/History");

    const auto profiles = BrowserProfileLocator::detectProfiles(mHome);
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().browserName, QStringLiteral("Brave"));
    QCOMPARE(profiles.first().profileName, QStringLiteral("Profile 1"));
    QVERIFY(profiles.first().cookiesDbPath.isEmpty());
}

void TestBrowserProfileLocator::chromium_prefersNetworkCookiesOverLegacyPath()
{
#ifdef Q_OS_MAC
    const QString profileDir = mHome + "/Library/Application Support/Google/Chrome/Default";
#else
    const QString profileDir = chromiumBase("google-chrome") + "/Default";
#endif
    writeFile(profileDir + "/History");
    writeFile(profileDir + "/Cookies");                 // legacy location
    writeFile(profileDir + "/Network/Cookies");          // modern (Chrome 96+) location

    const auto profiles = BrowserProfileLocator::detectProfiles(mHome);
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().cookiesDbPath, profileDir + "/Network/Cookies");
}

void TestBrowserProfileLocator::chromium_nonProfileDirectory_isIgnored()
{
#ifdef Q_OS_MAC
    const QString base = mHome + "/Library/Application Support/Google/Chrome";
#else
    const QString base = chromiumBase("google-chrome");
#endif
    // "System Profile" / "Guest Profile" and similar are not "Default" or
    // "Profile N" and must not be picked up.
    writeFile(base + "/Crashpad/somefile");
    writeFile(base + "/System Profile/History");

    QVERIFY(BrowserProfileLocator::detectProfiles(mHome).isEmpty());
}

void TestBrowserProfileLocator::noInstalledBrowsers_returnsEmpty()
{
    QVERIFY(BrowserProfileLocator::detectProfiles(mHome).isEmpty());
}

QTEST_MAIN(TestBrowserProfileLocator)
#include "test_browser_profile_locator.moc"
