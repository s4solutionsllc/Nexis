// GH#532 / SSO-26631: same QDir::System omission as the Linux scanner
// (system_info.cpp was duplicated per-platform), fixed and tested the same
// way. SystemInfoMacOS is only compiled on macOS, so this test is registered
// only under the CMakeLists.txt APPLE block.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include "system_info_macos.h"

namespace {

bool mkdirP(const QString &path) { return QDir().mkpath(path); }

void makeDanglingSymlink(const QString &linkPath)
{
    mkdirP(QFileInfo(linkPath).absolutePath());
    QFile::link("/nonexistent-" + QFileInfo(linkPath).fileName(), linkPath);
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

class TestBrokenSymlinksMacOS : public QObject
{
    Q_OBJECT

private slots:
    void detectsDanglingSymlinksAndExcludesKnownLocks();
};

void TestBrokenSymlinksMacOS::detectsDanglingSymlinksAndExcludesKnownLocks()
{
    ScopedFakeHome home;

    makeDanglingSymlink(home.path() + "/.local/test_dangling");
    makeDanglingSymlink(home.path() + "/.local/share/flatpak/dangling_flatpak");
    makeDanglingSymlink(home.path() + "/.local/share/icons/dangling_icon");
    makeDanglingSymlink(home.path() + "/.local/share/Trash/dangling_trash");
    makeDanglingSymlink(home.path() + "/.local/share/firefox/profile/.parentlock");

    SystemInfoMacOS info;
    QFileInfoList result = info.getBrokenSymlinks();

    QStringList names;
    for (const QFileInfo &fi : result)
        names << fi.fileName();

    QVERIFY(names.contains("test_dangling"));
    QVERIFY(!names.contains("dangling_flatpak"));
    QVERIFY(!names.contains("dangling_icon"));
    QVERIFY(!names.contains("dangling_trash"));
    QVERIFY(!names.contains(".parentlock"));
}

QTEST_MAIN(TestBrokenSymlinksMacOS)
#include "test_broken_symlinks_macos.moc"
