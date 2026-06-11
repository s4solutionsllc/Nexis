// SSO-3365 / audit H4: FileSearchService::moveToTrash and ::deleteFile used to
// run CommandUtil::exec directly on the UI thread. CommandUtil::exec throws a
// raw QString on QProcess error (e.g. FailedToStart, timeout) — that exception
// escaped through the Qt event loop and aborted the app. The service now
// dispatches on a worker thread and reports success/failure via the
// fileOperationFinished signal. These tests assert both:
//   - the success path actually removes the file (proves the worker ran), and
//   - a forced exec failure is caught (no exception escapes) and surfaced
//     via the signal with hadError=true.

#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>

#include "Services/file_search_service.h"

class TestFileSearchService : public QObject
{
    Q_OBJECT

private slots:
    void deleteFile_success_emitsNoError();
    void moveToTrash_execFailure_caughtAndReported();

private:
    QString writeTempFile(QTemporaryDir &dir, const QString &name);
    // Match the owner of the target file so isAnotherUser==false and we take
    // the non-sudo CommandUtil::exec branch (pkexec would prompt and block
    // the test).
    static QString ownerOf(const QString &path);
};

QString TestFileSearchService::writeTempFile(QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.path() + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return QString();
    }
    QTextStream(&f) << "test\n";
    f.close();
    return path;
}

QString TestFileSearchService::ownerOf(const QString &path)
{
    return QFileInfo(path).owner();
}

void TestFileSearchService::deleteFile_success_emitsNoError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString path = writeTempFile(tmp, "victim.txt");
    QVERIFY(!path.isEmpty());
    QVERIFY(QFile::exists(path));

    FileSearchService *svc = FileSearchService::ins();
    QSignalSpy spy(svc, &FileSearchService::fileOperationFinished);

    svc->deleteFile(path, ownerOf(path));

    QVERIFY(spy.wait(15000));
    QCOMPARE(spy.count(), 1);

    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).value<FileSearchService::FileOperation>(),
             FileSearchService::FileOperation::Delete);
    QCOMPARE(args.at(1).toString(), path);
    QCOMPARE(args.at(2).toBool(), false);
    QVERIFY(args.at(3).toString().isEmpty());

    QVERIFY(!QFile::exists(path));
}

void TestFileSearchService::moveToTrash_execFailure_caughtAndReported()
{
    // Point PATH at a directory that doesn't exist so QProcess can't resolve
    // "mv" → QProcess::FailedToStart → CommandUtil::exec throws QString. The
    // worker must catch and report via the signal; the synchronous call into
    // moveToTrash must never throw. PATH is restored after spy.wait() to avoid
    // racing the worker thread that hasn't called QProcess::start() yet.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = writeTempFile(tmp, "wont-move.txt");
    QVERIFY(!path.isEmpty());

    const QByteArray savedPath = qgetenv("PATH");
    qputenv("PATH", "/nexis-sso3365-no-such-dir");

    FileSearchService *svc = FileSearchService::ins();
    QSignalSpy spy(svc, &FileSearchService::fileOperationFinished);

    bool threw = false;
    try {
        svc->moveToTrash(path, "wont-move.txt", ownerOf(path));
    } catch (...) {
        threw = true;
    }

    const bool spyFired = spy.wait(15000);

    // Restore PATH no matter what — keeps the rest of the process well-formed
    // and avoids leaving a broken env if a later assertion aborts.
    qputenv("PATH", savedPath);

    QVERIFY(!threw);
    QVERIFY(spyFired);
    QCOMPARE(spy.count(), 1);

    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).value<FileSearchService::FileOperation>(),
             FileSearchService::FileOperation::MoveToTrash);
    QCOMPARE(args.at(1).toString(), path);
    QCOMPARE(args.at(2).toBool(), true);

    // The source file should still exist — the mv never ran.
    QVERIFY(QFile::exists(path));
}

QTEST_MAIN(TestFileSearchService)
#include "test_file_search_service.moc"
