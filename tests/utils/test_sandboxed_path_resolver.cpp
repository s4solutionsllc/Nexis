#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "sandboxed_path_resolver.h"

class TestSandboxedPathResolver : public QObject
{
    Q_OBJECT

private slots:
    void glob_matchesWithinBase();
    void glob_reportsByteSizes();
    void walk_recursesSubdirectories();
    void walk_doesNotFollowSymlinkedDirectoryOutOfSandbox();
    void regex_filtersByRelativePath();
    void resolve_traversalPatternCannotEscapeBase();
    void resolve_symlinkedFileCannotEscapeBase();
    void isPathConfinedTo_rejectsRealTraversalTarget();
    void isPathConfinedTo_acceptsBaseDirItself();

private:
    static void writeFile(const QString &path, const QByteArray &content = "x");
};

void TestSandboxedPathResolver::writeFile(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
    f.close();
}

void TestSandboxedPathResolver::glob_matchesWithinBase()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    writeFile(tmp.filePath("cache1.tmp"));
    writeFile(tmp.filePath("cache2.tmp"));
    writeFile(tmp.filePath("keepme.log"));

    const auto matches = SandboxedPathResolver::resolve(
        tmp.path(), QString(), "*.tmp", SandboxedPathResolver::MatchKind::Glob);

    QCOMPARE(matches.size(), 2);
    for (const auto &m : matches)
        QVERIFY(SandboxedPathResolver::isPathConfinedTo(m.absolutePath, tmp.path()));
}

void TestSandboxedPathResolver::glob_reportsByteSizes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.filePath("sized.tmp"), QByteArray(1234, 'a'));

    const auto matches = SandboxedPathResolver::resolve(
        tmp.path(), QString(), "*.tmp", SandboxedPathResolver::MatchKind::Glob);

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first().sizeBytes, qint64(1234));
}

void TestSandboxedPathResolver::walk_recursesSubdirectories()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir(tmp.path()).mkpath("a/b/c");
    writeFile(tmp.filePath("top.cache"));
    writeFile(tmp.filePath("a/mid.cache"));
    writeFile(tmp.filePath("a/b/c/deep.cache"));

    const auto matches = SandboxedPathResolver::resolve(
        tmp.path(), QString(), QString(), SandboxedPathResolver::MatchKind::Walk);

    QCOMPARE(matches.size(), 3);
}

void TestSandboxedPathResolver::walk_doesNotFollowSymlinkedDirectoryOutOfSandbox()
{
    QTemporaryDir sandbox;
    QTemporaryDir outside;
    QVERIFY(sandbox.isValid());
    QVERIFY(outside.isValid());

    writeFile(outside.filePath("secret.txt"), "should not be reachable");
    writeFile(sandbox.filePath("visible.txt"));

    const QString linkPath = sandbox.filePath("escape-link");
    if (!QFile::link(outside.path(), linkPath))
        QSKIP("symlink creation not supported in this environment");

    const auto matches = SandboxedPathResolver::resolve(
        sandbox.path(), QString(), QString(), SandboxedPathResolver::MatchKind::Walk);

    for (const auto &m : matches)
        QVERIFY(!m.absolutePath.contains("secret.txt"));
}

void TestSandboxedPathResolver::regex_filtersByRelativePath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir(tmp.path()).mkpath("logs");
    writeFile(tmp.filePath("logs/app.log"));
    writeFile(tmp.filePath("logs/app.log.1"));
    writeFile(tmp.filePath("keep.txt"));

    const auto matches = SandboxedPathResolver::resolve(
        tmp.path(), QString(), R"(^logs/app\.log(\.\d+)?$)",
        SandboxedPathResolver::MatchKind::Regex);

    QCOMPARE(matches.size(), 2);
}

void TestSandboxedPathResolver::resolve_traversalPatternCannotEscapeBase()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    QDir(sandbox.path()).mkpath("nested");

    // A crafted subPath walking out of the sandbox toward a file that is
    // guaranteed to exist on any Linux/macOS box (SSO-23859 AC: a crafted
    // path-traversal-style pattern must not escape the resolved base).
    const auto matches = SandboxedPathResolver::resolve(
        sandbox.path(), "nested/../../../../../../../../etc", "passwd",
        SandboxedPathResolver::MatchKind::Glob);

    QVERIFY(matches.isEmpty());
}

void TestSandboxedPathResolver::resolve_symlinkedFileCannotEscapeBase()
{
    QTemporaryDir sandbox;
    QTemporaryDir outside;
    QVERIFY(sandbox.isValid());
    QVERIFY(outside.isValid());

    writeFile(outside.filePath("secret.tmp"), "outside data");

    const QString linkPath = sandbox.filePath("looks-local.tmp");
    if (!QFile::link(outside.filePath("secret.tmp"), linkPath))
        QSKIP("symlink creation not supported in this environment");

    const auto matches = SandboxedPathResolver::resolve(
        sandbox.path(), QString(), "*.tmp", SandboxedPathResolver::MatchKind::Glob);

    QVERIFY(matches.isEmpty());
}

void TestSandboxedPathResolver::isPathConfinedTo_rejectsRealTraversalTarget()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());

    QVERIFY(!SandboxedPathResolver::isPathConfinedTo(
        sandbox.path() + "/../../../../../../../../etc/passwd", sandbox.path()));
}

void TestSandboxedPathResolver::isPathConfinedTo_acceptsBaseDirItself()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());

    QVERIFY(SandboxedPathResolver::isPathConfinedTo(sandbox.path(), sandbox.path()));
}

QTEST_MAIN(TestSandboxedPathResolver)
#include "test_sandboxed_path_resolver.moc"
