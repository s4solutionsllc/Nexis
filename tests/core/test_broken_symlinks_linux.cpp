// GH#532 / SSO-26631: Qt does not enumerate broken symlinks unless
// QDir::System is set, so SystemInfoLinux::getBrokenSymlinks() always
// returned an empty list. This exercises the real implementation against a
// fake $HOME, covering both the QDir::System fix and the exclusion list that
// keeps it from flagging intentionally-dangling locks / internal plumbing as
// deletable "broken" symlinks.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include "system_info_linux.h"

namespace {

bool mkdirP(const QString &path) { return QDir().mkpath(path); }

void makeDanglingSymlink(const QString &linkPath)
{
    mkdirP(QFileInfo(linkPath).absolutePath());
    QFile::link("/nonexistent-" + QFileInfo(linkPath).fileName(), linkPath);
}

void makeFile(const QString &path)
{
    mkdirP(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
}

class ScopedFakeHome
{
public:
    ScopedFakeHome() : mDir(new QTemporaryDir())
    {
        Q_ASSERT(mDir->isValid());
        mHadHome = qEnvironmentVariableIsSet("HOME");
        mPrevHome = qgetenv("HOME");
        qputenv("HOME", mDir->path().toUtf8());
    }

    ~ScopedFakeHome()
    {
        if (mHadHome)
            qputenv("HOME", mPrevHome);
        else
            qunsetenv("HOME");
    }

    QString path() const { return mDir->path(); }

private:
    QScopedPointer<QTemporaryDir> mDir;
    bool mHadHome = false;
    QByteArray mPrevHome;
};

} // namespace

class TestBrokenSymlinksLinux : public QObject
{
    Q_OBJECT

private slots:
    void detectsDanglingSymlinks();
    void excludesFlatpakIconsAndTrash();
    void excludesKnownIntentionalLocks();
    void ignoresValidSymlinksAndOutOfScopeDirs();
};

void TestBrokenSymlinksLinux::detectsDanglingSymlinks()
{
    ScopedFakeHome home;

    makeDanglingSymlink(home.path() + "/.local/test_dangling");
    makeDanglingSymlink(home.path() + "/.local/sub/.hidden_dangling");
    makeDanglingSymlink(home.path() + "/bin/dangling_bin");

    SystemInfoLinux info;
    QFileInfoList result = info.getBrokenSymlinks();

    QStringList names;
    for (const QFileInfo &fi : result)
        names << fi.fileName();

    QVERIFY(names.contains("test_dangling"));
    QVERIFY(names.contains(".hidden_dangling"));
    QVERIFY(names.contains("dangling_bin"));
}

void TestBrokenSymlinksLinux::excludesFlatpakIconsAndTrash()
{
    ScopedFakeHome home;

    makeDanglingSymlink(home.path() + "/.local/share/flatpak/exports/share/icons/dangling_flatpak");
    makeDanglingSymlink(home.path() + "/.local/share/icons/dangling_icon");
    makeDanglingSymlink(home.path() + "/.local/share/Trash/files/dangling_trash");
    makeDanglingSymlink(home.path() + "/.local/real_dangling");

    SystemInfoLinux info;
    QFileInfoList result = info.getBrokenSymlinks();

    QStringList names;
    for (const QFileInfo &fi : result)
        names << fi.fileName();

    QVERIFY(!names.contains("dangling_flatpak"));
    QVERIFY(!names.contains("dangling_icon"));
    QVERIFY(!names.contains("dangling_trash"));
    QVERIFY(names.contains("real_dangling"));
}

void TestBrokenSymlinksLinux::excludesKnownIntentionalLocks()
{
    ScopedFakeHome home;

    makeDanglingSymlink(home.path() + "/.local/share/some-app/SingletonLock");
    makeDanglingSymlink(home.path() + "/.local/share/some-app/SingletonCookie");
    makeDanglingSymlink(home.path() + "/.local/share/some-app/SingletonSocket");
    makeDanglingSymlink(home.path() + "/.local/share/firefox/profile/lock");
    makeDanglingSymlink(home.path() + "/.local/share/firefox/profile/.parentlock");

    SystemInfoLinux info;
    QFileInfoList result = info.getBrokenSymlinks();

    for (const QFileInfo &fi : result) {
        QVERIFY2(fi.fileName() != "SingletonLock", "SingletonLock should be excluded");
        QVERIFY2(fi.fileName() != "SingletonCookie", "SingletonCookie should be excluded");
        QVERIFY2(fi.fileName() != "SingletonSocket", "SingletonSocket should be excluded");
        QVERIFY2(fi.fileName() != "lock", "lock should be excluded");
        QVERIFY2(fi.fileName() != ".parentlock", ".parentlock should be excluded");
    }
}

void TestBrokenSymlinksLinux::ignoresValidSymlinksAndOutOfScopeDirs()
{
    ScopedFakeHome home;

    makeFile(home.path() + "/.local/real_target");
    mkdirP(home.path() + "/.local");
    QFile::link(home.path() + "/.local/real_target", home.path() + "/.local/valid_link");

    // Outside the scanned roots (~/.local, ~/bin, /usr/local/bin) — expected
    // not to be listed, per the issue's own notes.
    makeDanglingSymlink(home.path() + "/.cache/dangling_out_of_scope");

    SystemInfoLinux info;
    QFileInfoList result = info.getBrokenSymlinks();

    QStringList names;
    for (const QFileInfo &fi : result)
        names << fi.fileName();

    QVERIFY(!names.contains("valid_link"));
    QVERIFY(!names.contains("dangling_out_of_scope"));
}

QTEST_MAIN(TestBrokenSymlinksLinux)
#include "test_broken_symlinks_linux.moc"
