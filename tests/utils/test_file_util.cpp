#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include "file_util.h"

class TestFileUtil : public QObject
{
    Q_OBJECT

private slots:
    void readStringFromFile_validFile();
    void readStringFromFile_nonexistent();
    void readStringFromFile_emptyFile();
    void readListFromFile_multipleLines();
    void writeFile_roundTrip();
    void writeFile_invalidPath();
    void getFileSize_singleFile();
    void getFileSize_recursiveDir();
    void getFileSize_nonexistent();
    void directoryList_withFiles();
};

void TestFileUtil::readStringFromFile_validFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString path = tmp.path() + "/test.txt";

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello world");
    f.close();

    QString content = FileUtil::readStringFromFile(path);
    QCOMPARE(content.trimmed(), QString("hello world"));
}

void TestFileUtil::readStringFromFile_nonexistent()
{
    QString content = FileUtil::readStringFromFile("/nonexistent/path/file.txt");
    QVERIFY(content.isEmpty());
}

void TestFileUtil::readStringFromFile_emptyFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString path = tmp.path() + "/empty.txt";

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    QString content = FileUtil::readStringFromFile(path);
    QVERIFY(content.isEmpty());
}

void TestFileUtil::readListFromFile_multipleLines()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString path = tmp.path() + "/lines.txt";

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("line1\nline2\nline3\n");
    f.close();

    QStringList lines = FileUtil::readListFromFile(path);
    QVERIFY(lines.size() >= 3);
    QCOMPARE(lines.at(0), QString("line1"));
    QCOMPARE(lines.at(1), QString("line2"));
    QCOMPARE(lines.at(2), QString("line3"));
}

void TestFileUtil::writeFile_roundTrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString path = tmp.path() + "/roundtrip.txt";

    bool ok = FileUtil::writeFile(path, "test content");
    QVERIFY(ok);

    QString content = FileUtil::readStringFromFile(path);
    QCOMPARE(content.trimmed(), QString("test content"));
}

void TestFileUtil::writeFile_invalidPath()
{
    bool ok = FileUtil::writeFile("/nonexistent/dir/file.txt", "data");
    QVERIFY(!ok);
}

void TestFileUtil::getFileSize_singleFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString path = tmp.path() + "/sized.txt";

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QByteArray data(1234, 'x');
    f.write(data);
    f.close();

    quint64 size = FileUtil::getFileSize(path);
    QCOMPARE(size, static_cast<quint64>(1234));
}

void TestFileUtil::getFileSize_recursiveDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir().mkpath(tmp.path() + "/sub");

    QFile f1(tmp.path() + "/a.txt");
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write(QByteArray(100, 'a'));
    f1.close();

    QFile f2(tmp.path() + "/sub/b.txt");
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write(QByteArray(200, 'b'));
    f2.close();

    quint64 size = FileUtil::getFileSize(tmp.path());
    QVERIFY(size >= 300);
}

void TestFileUtil::getFileSize_nonexistent()
{
    quint64 size = FileUtil::getFileSize("/nonexistent/path");
    QCOMPARE(size, static_cast<quint64>(0));
}

void TestFileUtil::directoryList_withFiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QFile f1(tmp.path() + "/file1.txt");
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.close();
    QFile f2(tmp.path() + "/file2.txt");
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.close();
    QDir().mkpath(tmp.path() + "/subdir");

    QStringList listing = FileUtil::directoryList(tmp.path());
    QVERIFY(listing.contains("file1.txt") || listing.contains(tmp.path() + "/file1.txt"));
}

QTEST_MAIN(TestFileUtil)
#include "test_file_util.moc"
