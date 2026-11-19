// SSO-27151: uninstaller shared-data guard. Pure id/leaf-name classification
// (SharedDataGuard::classify) for both macOS bundle ids and Linux package
// names, plus the REAL LeftoverScannerLinux::scanLeftovers() against a
// synthetic sibling-app fixture under a temporary $HOME.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Tools/leftover_scanner_linux.h"
#include "Tools/shared_data_guard.h"

namespace {

bool touchFile(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly);
}

class ScopedFakeHome
{
public:
    ScopedFakeHome()
    {
        Q_ASSERT(mDir.isValid());
        mOldHome = qgetenv("HOME");
        qputenv("HOME", mDir.path().toUtf8());
        for (const char *v : {"XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_DATA_HOME"}) {
            mOldXdg.append({v, qgetenv(v), qEnvironmentVariableIsSet(v)});
            qunsetenv(v);
        }
    }
    ~ScopedFakeHome()
    {
        qputenv("HOME", mOldHome);
        for (const auto &x : mOldXdg) {
            if (x.wasSet)
                qputenv(x.name, x.value);
            else
                qunsetenv(x.name);
        }
    }
    QString path() const { return mDir.path(); }

private:
    struct Saved { const char *name; QByteArray value; bool wasSet; };
    QTemporaryDir mDir;
    QByteArray mOldHome;
    QList<Saved> mOldXdg;
};

} // namespace

class TestSharedDataGuard : public QObject
{
    Q_OBJECT

private slots:
    void familyBase_stripsChannelToken();
    void familyBase_leavesPlainIdsAlone();
    void areVariants_sameFamilyDifferentIds();

    void classify_macLongerSiblingBundleIdOwnsLeaf();
    void classify_macOwnLeafIsNotShared();
    void classify_macUninstallingBetaKeepsOwnData();
    void classify_macStableWithInstalledBetaIsShared();
    void classify_macUnrelatedPrefixSiblingIsNotShared();
    void classify_linuxEsrVariantSharesUnsuffixedProfile();
    void classify_linuxHyphenatedDistinctPackageIsNotAVariant();
    void classify_caseInsensitive();
    void classify_nothingInstalledNeverShared();

    void linuxScanner_siblingFixtureMarksSharedAndKeepsOwn();
    void linuxScanner_noInstalledIdsMarksNothing();
    void linuxScanner_longerSiblingDirIsSharedWithItsOwner();
};

void TestSharedDataGuard::familyBase_stripsChannelToken()
{
    QCOMPARE(SharedDataGuard::familyBase("com.google.Chrome.beta"), QString("com.google.Chrome"));
    QCOMPARE(SharedDataGuard::familyBase("firefox-esr"), QString("firefox"));
    QCOMPARE(SharedDataGuard::familyBase("code-insiders"), QString("code"));
    QCOMPARE(SharedDataGuard::familyBase("App_Nightly"), QString("App"));
}

void TestSharedDataGuard::familyBase_leavesPlainIdsAlone()
{
    QCOMPARE(SharedDataGuard::familyBase("firefox"), QString("firefox"));
    QCOMPARE(SharedDataGuard::familyBase("gnome-terminal-server"), QString("gnome-terminal-server"));
    QCOMPARE(SharedDataGuard::familyBase("beta"), QString("beta"));
    QCOMPARE(SharedDataGuard::familyBase(QString()), QString());
}

void TestSharedDataGuard::areVariants_sameFamilyDifferentIds()
{
    QVERIFY(SharedDataGuard::areVariants("firefox", "firefox-esr"));
    QVERIFY(SharedDataGuard::areVariants("com.example.App.beta", "com.example.App.canary"));
    QVERIFY(!SharedDataGuard::areVariants("firefox", "firefox"));
    QVERIFY(!SharedDataGuard::areVariants("firefox", "chromium"));
    QVERIFY(!SharedDataGuard::areVariants("", "firefox"));
}

void TestSharedDataGuard::classify_macLongerSiblingBundleIdOwnsLeaf()
{
    const auto claim = SharedDataGuard::classify(
        "com.example.App.beta.plist", "com.example.App", {"com.example.App.beta"});
    QVERIFY(claim.isShared());
    QCOMPARE(claim.siblingId, QString("com.example.App.beta"));
    QVERIFY(claim.reason.contains("com.example.App.beta"));

    QVERIFY(SharedDataGuard::classify(
        "com.example.App.beta", "com.example.App", {"com.example.App.beta"}).isShared());
}

void TestSharedDataGuard::classify_macOwnLeafIsNotShared()
{
    QVERIFY(!SharedDataGuard::classify(
        "com.example.App.plist", "com.example.App", {"com.example.Other"}).isShared());
}

void TestSharedDataGuard::classify_macUninstallingBetaKeepsOwnData()
{
    // The stable build is shorter than the matched beta id, so it does not own
    // the beta's leaf, and a beta is not the unsuffixed id of its family.
    QVERIFY(!SharedDataGuard::classify(
        "com.example.App.beta.plist", "com.example.App.beta", {"com.example.App"}).isShared());
}

void TestSharedDataGuard::classify_macStableWithInstalledBetaIsShared()
{
    const auto claim = SharedDataGuard::classify(
        "com.example.App.plist", "com.example.App", {"com.example.App.beta"});
    QVERIFY(claim.isShared());
    QCOMPARE(claim.siblingId, QString("com.example.App.beta"));
}

void TestSharedDataGuard::classify_macUnrelatedPrefixSiblingIsNotShared()
{
    // "com.example.AppStore" merely shares a string prefix; it is not a
    // separator-delimited owner of "com.example.App.plist".
    QVERIFY(!SharedDataGuard::classify(
        "com.example.App.plist", "com.example.App", {"com.example.AppStore"}).isShared());
}

void TestSharedDataGuard::classify_linuxEsrVariantSharesUnsuffixedProfile()
{
    const auto claim = SharedDataGuard::classify("firefox", "firefox", {"firefox-esr"});
    QVERIFY(claim.isShared());
    QCOMPARE(claim.siblingId, QString("firefox-esr"));
}

void TestSharedDataGuard::classify_linuxHyphenatedDistinctPackageIsNotAVariant()
{
    QVERIFY(!SharedDataGuard::classify(
        "gnome-terminal.desktop", "gnome-terminal", {"gnome-terminal-server"}).isShared());
}

void TestSharedDataGuard::classify_caseInsensitive()
{
    QVERIFY(SharedDataGuard::classify("Code", "code", {"code-insiders"}).isShared());
    QVERIFY(SharedDataGuard::classify(
        "COM.EXAMPLE.APP.BETA.plist", "com.example.app", {"com.example.App.Beta"}).isShared());
}

void TestSharedDataGuard::classify_nothingInstalledNeverShared()
{
    QVERIFY(!SharedDataGuard::classify("firefox", "firefox", {}).isShared());
    QVERIFY(!SharedDataGuard::classify("firefox", QString(), {"firefox-esr"}).isShared());
}

void TestSharedDataGuard::linuxScanner_siblingFixtureMarksSharedAndKeepsOwn()
{
    ScopedFakeHome home;
    // Uninstalling "firefox"; "firefox-esr" stays installed.
    QVERIFY(touchFile(home.path() + "/.config/firefox/profile.ini"));
    QVERIFY(touchFile(home.path() + "/.cache/firefox.log"));
    QVERIFY(touchFile(home.path() + "/.config/firefox-esr/prefs.js"));

    const auto found = LeftoverScannerLinux::scanLeftovers({"firefox"}, {"firefox-esr"});

    QCOMPARE(found.size(), 2);   // firefox-esr's own dir is never matched
    for (const auto &c : found) {
        QVERIFY2(c.sharedWith == "firefox-esr", qPrintable(c.path));
        QVERIFY(!c.sharedReason.isEmpty());
    }
}

void TestSharedDataGuard::linuxScanner_noInstalledIdsMarksNothing()
{
    ScopedFakeHome home;
    QVERIFY(touchFile(home.path() + "/.config/firefox/profile.ini"));

    const auto found = LeftoverScannerLinux::scanLeftovers({"firefox"});

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().sharedWith.isEmpty());
    QVERIFY(found.first().sharedReason.isEmpty());
}

void TestSharedDataGuard::linuxScanner_longerSiblingDirIsSharedWithItsOwner()
{
    ScopedFakeHome home;
    // Flatpak "org.example.App" removed while "org.example.App.Beta" stays: the
    // dot-prefix scan also matches the beta's own dir, and the unsuffixed dir
    // is the profile the beta variant shares.
    QVERIFY(touchFile(home.path() + "/.config/org.example.App/a.conf"));
    QVERIFY(touchFile(home.path() + "/.config/org.example.App.Beta/b.conf"));

    const auto found = LeftoverScannerLinux::scanLeftovers(
        {"org.example.App"}, {"org.example.App.Beta"});

    QCOMPARE(found.size(), 2);
    for (const auto &c : found) {
        QCOMPARE(c.sharedWith, QString("org.example.App.Beta"));
        QVERIFY(!c.sharedReason.isEmpty());
    }
}

QTEST_MAIN(TestSharedDataGuard)
#include "test_shared_data_guard.moc"
