// SSO-15385: exercises the REAL LeftoverScannerLinux::scanLeftovers() against
// the SSO-15387 fixture scenarios. test_app_leftovers_linux.cpp intentionally
// duplicates the matching algorithm as a platform-neutral contract test — it
// does not call the production code path. This file closes that gap by
// pointing the actual XDG env vars at a QTemporaryDir and calling the real
// implementation, so a regression in leftover_scanner_linux.cpp's matching
// logic is caught here even if the contract test still passes.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

#include "Tools/leftover_scanner_linux.h"

namespace {

bool mkdirP(const QString &path) { return QDir().mkpath(path); }

bool touchFile(const QString &path)
{
    mkdirP(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly);
}

// Points XDG_CONFIG_HOME/XDG_CACHE_HOME/XDG_DATA_HOME (and $HOME, so the
// LifecycleDenyList home-boundary check doesn't reject every fixture path)
// at a fresh temp dir for the lifetime of the object, and restores the
// previous environment on destruction.
class ScopedFakeHome
{
public:
    ScopedFakeHome() : mDir(new QTemporaryDir())
    {
        Q_ASSERT(mDir->isValid());
        save("HOME", mHome);
        save("XDG_CONFIG_HOME", mXdgConfig);
        save("XDG_CACHE_HOME", mXdgCache);
        save("XDG_DATA_HOME", mXdgData);

        qputenv("HOME", mDir->path().toUtf8());
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_CACHE_HOME");
        qunsetenv("XDG_DATA_HOME");
    }

    ~ScopedFakeHome()
    {
        restore("HOME", mHome);
        restore("XDG_CONFIG_HOME", mXdgConfig);
        restore("XDG_CACHE_HOME", mXdgCache);
        restore("XDG_DATA_HOME", mXdgData);
    }

    QString path() const { return mDir->path(); }

private:
    struct SavedVar { bool wasSet = false; QByteArray value; };

    void save(const char *name, SavedVar &out)
    {
        out.wasSet = qEnvironmentVariableIsSet(name);
        if (out.wasSet)
            out.value = qgetenv(name);
    }

    void restore(const char *name, const SavedVar &saved)
    {
        if (saved.wasSet)
            qputenv(name, saved.value);
        else
            qunsetenv(name);
    }

    QScopedPointer<QTemporaryDir> mDir;
    SavedVar mHome, mXdgConfig, mXdgCache, mXdgData;
};

} // namespace

class TestLeftoverScannerLinuxReal : public QObject
{
    Q_OBJECT

private slots:
    void exactMatch_xdgDirsFound();
    void hyphenSiblingPackage_notFalsePositive();
    void autostartDesktopSuffix_matches();
    void reverseDnsFlatpakId_exactMatchOnly();
    void reverseDnsShortName_matchesLastSegment();
    void libPrefix_notFalsePositive();
};

void TestLeftoverScannerLinuxReal::exactMatch_xdgDirsFound()
{
    ScopedFakeHome home;
    const QString pkg = QStringLiteral("vlc");

    QVERIFY(mkdirP(home.path() + "/.config/" + pkg));
    QVERIFY(mkdirP(home.path() + "/.cache/" + pkg));
    QVERIFY(mkdirP(home.path() + "/.local/share/" + pkg));

    const auto found = LeftoverScannerLinux::scanLeftovers({pkg});
    QCOMPARE(found.size(), 3);
}

// Regression coverage: leafMatches() previously treated '-', '_', and ' ' as
// prefix delimiters, so scanning for "code" would also flag "code-insiders",
// "firefox" would flag "firefox-esr", and "gnome-terminal" would flag
// "gnome-terminal-server" — deleting an unrelated package's data. Only '.'
// is a valid delimiter now.
void TestLeftoverScannerLinuxReal::hyphenSiblingPackage_notFalsePositive()
{
    struct Case { QString target; QString sibling; };
    const QList<Case> cases = {
        { QStringLiteral("code"),           QStringLiteral("code-insiders") },
        { QStringLiteral("firefox"),        QStringLiteral("firefox-esr") },
        { QStringLiteral("gnome-terminal"), QStringLiteral("gnome-terminal-server") },
    };

    for (const Case &c : cases) {
        ScopedFakeHome caseHome;
        QVERIFY(mkdirP(caseHome.path() + "/.config/" + c.target));
        QVERIFY(mkdirP(caseHome.path() + "/.config/" + c.sibling));

        const auto found = LeftoverScannerLinux::scanLeftovers({c.target});
        QCOMPARE(found.size(), 1);
        QVERIFY(found.first().path.endsWith(c.target));
    }
}

void TestLeftoverScannerLinuxReal::autostartDesktopSuffix_matches()
{
    ScopedFakeHome home;
    const QString pkg = QStringLiteral("dropbox");

    QVERIFY(touchFile(home.path() + "/.config/autostart/" + pkg + ".desktop"));

    const auto found = LeftoverScannerLinux::scanLeftovers({pkg});
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().category, QStringLiteral("Autostart"));
}

void TestLeftoverScannerLinuxReal::reverseDnsFlatpakId_exactMatchOnly()
{
    ScopedFakeHome home;
    const QString target = QStringLiteral("com.spotify.Client");
    const QString other  = QStringLiteral("com.spotify.Podcast");

    QVERIFY(mkdirP(home.path() + "/.config/" + target));
    QVERIFY(mkdirP(home.path() + "/.config/" + other));

    const auto found = LeftoverScannerLinux::scanLeftovers({target});
    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(target));
}

void TestLeftoverScannerLinuxReal::reverseDnsShortName_matchesLastSegment()
{
    ScopedFakeHome home;
    QVERIFY(mkdirP(home.path() + "/.config/org.mozilla.firefox"));

    const auto found = LeftoverScannerLinux::scanLeftovers({QStringLiteral("firefox")});
    QCOMPARE(found.size(), 1);
}

void TestLeftoverScannerLinuxReal::libPrefix_notFalsePositive()
{
    ScopedFakeHome home;
    QVERIFY(mkdirP(home.path() + "/.config/vlc"));
    QVERIFY(mkdirP(home.path() + "/.config/libvlc5"));

    const auto found = LeftoverScannerLinux::scanLeftovers({QStringLiteral("vlc")});
    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(QStringLiteral("vlc")));
}

QTEST_MAIN(TestLeftoverScannerLinuxReal)
#include "test_leftover_scanner_linux_real.moc"
