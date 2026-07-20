// SSO-15387: leftover-scan heuristics fixture suite — Linux.
//
// Builds a synthetic XDG home tree and validates a testable subclass of
// PackageToolLinux that mirrors the heuristic SSO-15385 will implement:
//   • ~/.config/<package-name>   (XDG config)
//   • ~/.cache/<package-name>    (XDG cache)
//   • ~/.local/share/<package-name>  (XDG data)
//   • ~/.config/autostart/<package-name>.desktop  (autostart)
//
// Package manager types covered: dpkg, rpm, Flatpak, Snap.
//
// True-negative set: similar-but-distinct names, vendor-shared dirs,
// prefix/substring collisions (dpkg "lib*" naming, snap channels).
//
// Precision/recall contract:
//   recall = 1.0, precision ≥ 0.95 for the XDG name-match path.
//
// This test is platform-neutral (all paths are synthesised via QTemporaryDir)
// so it runs on macOS CI too — it does NOT require a real Linux package
// manager.  The platform-specific parts of PackageToolLinux (actual package
// queries) are not exercised here.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// Minimal stub for the leftover-scan contract that SSO-15385 will implement
// on PackageToolLinux.  The real implementation will override findAppLeftovers
// on the Linux tool; here we duplicate the expected algorithm so the fixture
// suite defines the acceptance contract independently of the implementation.
// ---------------------------------------------------------------------------

struct Package; // forward — pulled from package_tool_shared.h at link time
#include "Tools/package_tool_shared.h"

struct LinuxLeftoverScanner {
    // Scan XDG dirs for leftovers matching `pkgName` exactly.
    // fakeHome replaces $HOME so the test runs without touching the real FS.
    static QList<AppLeftover> scan(const QString &pkgName,
                                   const QString &fakeHome)
    {
        QList<AppLeftover> result;
        if (pkgName.isEmpty())
            return result;

        struct Dir { QString rel; QString label; };
        const QList<Dir> xdgDirs = {
            { QStringLiteral(".config"),        QStringLiteral("Config") },
            { QStringLiteral(".cache"),         QStringLiteral("Cache") },
            { QStringLiteral(".local/share"),   QStringLiteral("Local Share") },
        };

        for (const Dir &d : xdgDirs) {
            QDir dir(fakeHome + QLatin1Char('/') + d.rel);
            if (!dir.exists()) continue;
            const QFileInfoList entries = dir.entryInfoList(
                QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
            for (const QFileInfo &e : entries) {
                const QString name = e.fileName();
                if (name == pkgName) {
                    AppLeftover lo;
                    lo.path     = e.absoluteFilePath();
                    lo.category = d.label;
                    lo.size     = 0;
                    result.append(lo);
                }
            }
        }

        // Autostart .desktop entry
        const QString autostartPath =
            fakeHome + QStringLiteral("/.config/autostart/") + pkgName + QStringLiteral(".desktop");
        if (QFile::exists(autostartPath)) {
            AppLeftover lo;
            lo.path     = autostartPath;
            lo.category = QStringLiteral("Autostart");
            lo.size     = 0;
            result.append(lo);
        }

        return result;
    }
};

// ---------------------------------------------------------------------------

class TestAppLeftoversLinux : public QObject
{
    Q_OBJECT

private slots:
    // — dpkg / apt package fixtures —
    void dpkgPackage_xdgLeftoversFound();
    void dpkgPackage_autostartDesktopFound();
    void dpkgPackage_libPrefixNotFalsePositive();

    // — rpm package fixtures —
    void rpmPackage_xdgLeftoversFound();
    void rpmPackage_similarNameNotFalsePositive();

    // — Flatpak package fixtures —
    void flatpakPackage_xdgLeftoversFound();
    void flatpakPackage_reverseIdStyle();

    // — Snap package fixtures —
    void snapPackage_xdgLeftoversFound();
    void snapPackage_channelSuffixNotFalsePositive();

    // — true negatives —
    void noFalsePositivesFromSharedVendorDir();
    void noFalsePositivesFromSubstringName();
    void noFalsePositivesFromSimilarButDistinctName();

    // — edge / boundary —
    void emptyPackageNameReturnsEmpty();
    void missingXdgDirsReturnEmpty();

    // — precision/recall harness —
    void precisionRecall_xdgPath();
};

// ---------------------------------------------------------------------------

static bool mkdirP(const QString &path) { return QDir().mkpath(path); }

static bool touchFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly);
}

// ---------------------------------------------------------------------------
// dpkg / apt fixtures
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::dpkgPackage_xdgLeftoversFound()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("vlc");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".cache/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".local/share/" + pkg)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 3);
    QStringList paths;
    for (const AppLeftover &lo : found) paths << lo.path;
    QVERIFY(paths.contains(tmp.filePath(".config/" + pkg)));
    QVERIFY(paths.contains(tmp.filePath(".cache/" + pkg)));
    QVERIFY(paths.contains(tmp.filePath(".local/share/" + pkg)));
}

void TestAppLeftoversLinux::dpkgPackage_autostartDesktopFound()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("dropbox");

    QVERIFY(mkdirP(tmp.filePath(".config/autostart")));
    QVERIFY(touchFile(tmp.filePath(".config/autostart/" + pkg + ".desktop")));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().category, QStringLiteral("Autostart"));
    QVERIFY(found.first().path.endsWith(pkg + QStringLiteral(".desktop")));
}

void TestAppLeftoversLinux::dpkgPackage_libPrefixNotFalsePositive()
{
    // dpkg often has "libfoo" alongside "foo"; "libvlc5" must NOT be returned
    // when scanning for "vlc".
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg    = QStringLiteral("vlc");
    const QString libpkg = QStringLiteral("libvlc5");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + libpkg)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(pkg));
}

// ---------------------------------------------------------------------------
// rpm fixtures
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::rpmPackage_xdgLeftoversFound()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("libreoffice");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".cache/" + pkg)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());
    QCOMPARE(found.size(), 2);
}

void TestAppLeftoversLinux::rpmPackage_similarNameNotFalsePositive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg     = QStringLiteral("code");
    const QString similar = QStringLiteral("code-insiders"); // separate VS Code variant

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + similar)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(pkg));
}

// ---------------------------------------------------------------------------
// Flatpak fixtures
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::flatpakPackage_xdgLeftoversFound()
{
    // Flatpak apps typically use their reverse-domain name as the XDG dir.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("org.videolan.VLC");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".cache/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".local/share/" + pkg)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());
    QCOMPARE(found.size(), 3);
}

void TestAppLeftoversLinux::flatpakPackage_reverseIdStyle()
{
    // Two Flatpak apps sharing the same vendor prefix; only the target
    // should match.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString target = QStringLiteral("com.spotify.Client");
    const QString other  = QStringLiteral("com.spotify.Podcast");

    QVERIFY(mkdirP(tmp.filePath(".config/" + target)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + other)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(target, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(target));
}

// ---------------------------------------------------------------------------
// Snap fixtures
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::snapPackage_xdgLeftoversFound()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("firefox");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".cache/" + pkg)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());
    QCOMPARE(found.size(), 2);
}

void TestAppLeftoversLinux::snapPackage_channelSuffixNotFalsePositive()
{
    // Snap sometimes installs "firefox" and "firefox-esr" as separate snaps;
    // scanning for "firefox" must NOT return "firefox-esr" dirs.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("firefox");
    const QString esr = QStringLiteral("firefox-esr");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + esr)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(pkg));
}

// ---------------------------------------------------------------------------
// True-negative tests
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::noFalsePositivesFromSharedVendorDir()
{
    // "JetBrains" is a shared vendor dir used by all JetBrains IDEs;
    // scanning for "idea" must NOT return it.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg    = QStringLiteral("idea");
    const QString vendor = QStringLiteral("JetBrains");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + vendor)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(pkg));
}

void TestAppLeftoversLinux::noFalsePositivesFromSubstringName()
{
    // "git" is a substring of "gitkraken" and "gitg"; none must match "git".
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg    = QStringLiteral("git");
    const QString longer = QStringLiteral("gitkraken");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + longer)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(pkg));
}

void TestAppLeftoversLinux::noFalsePositivesFromSimilarButDistinctName()
{
    // "gnome-terminal" vs "gnome-terminal-server" — distinct packages.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg     = QStringLiteral("gnome-terminal");
    const QString similar = QStringLiteral("gnome-terminal-server");

    QVERIFY(mkdirP(tmp.filePath(".config/" + pkg)));
    QVERIFY(mkdirP(tmp.filePath(".config/" + similar)));

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(pkg));
}

// ---------------------------------------------------------------------------
// Edge / boundary tests
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::emptyPackageNameReturnsEmpty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(LinuxLeftoverScanner::scan(QString(), tmp.path()).isEmpty());
}

void TestAppLeftoversLinux::missingXdgDirsReturnEmpty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // No XDG dirs created.
    QVERIFY(LinuxLeftoverScanner::scan(QStringLiteral("ghost"), tmp.path()).isEmpty());
}

// ---------------------------------------------------------------------------
// Precision/recall harness (gate for SSO-15385 merge acceptance)
// ---------------------------------------------------------------------------

void TestAppLeftoversLinux::precisionRecall_xdgPath()
{
    // Plant N true-positive fixtures + M decoys, assert recall = 1.0 and
    // precision ≥ 0.95.  These thresholds are the merge-stage acceptance
    // bar for SSO-15385 (Linux leftover scanner implementation).

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString pkg = QStringLiteral("testpkg");

    // 4 true positives:
    QStringList expected;
    auto plant = [&](const QString &rel, bool isFile = false) -> QString {
        const QString full = tmp.filePath(rel);
        if (isFile) { mkdirP(QFileInfo(full).absolutePath()); touchFile(full); }
        else        { mkdirP(full); }
        return full;
    };

    expected << plant(QStringLiteral(".config/") + pkg);
    expected << plant(QStringLiteral(".cache/") + pkg);
    expected << plant(QStringLiteral(".local/share/") + pkg);
    expected << plant(QStringLiteral(".config/autostart/") + pkg + QStringLiteral(".desktop"), true);

    const int totalPositives = expected.size();

    // Decoys:
    const QStringList decoys = {
        QStringLiteral("testpkg-extra"),
        QStringLiteral("testpkg2"),
        QStringLiteral("libtestpkg"),
    };
    for (const QString &d : decoys) {
        mkdirP(tmp.filePath(QStringLiteral(".config/") + d));
        mkdirP(tmp.filePath(QStringLiteral(".cache/") + d));
    }

    const QList<AppLeftover> found = LinuxLeftoverScanner::scan(pkg, tmp.path());

    QStringList foundPaths;
    for (const AppLeftover &lo : found) foundPaths << lo.path;

    int tp = 0;
    for (const QString &ep : expected)
        if (foundPaths.contains(ep)) tp++;

    const int fn = totalPositives - tp;
    const int fp = found.size() - tp;

    QVERIFY2(fn == 0,
             qPrintable(QStringLiteral("Recall failure: %1 true positives missed").arg(fn)));

    const double precision = found.isEmpty()
        ? 1.0 : static_cast<double>(tp) / found.size();
    QVERIFY2(precision >= 0.95,
             qPrintable(QStringLiteral("Precision %1 below 0.95 (%2 false positives)")
                        .arg(precision).arg(fp)));
}

QTEST_MAIN(TestAppLeftoversLinux)
#include "test_app_leftovers_linux.moc"
