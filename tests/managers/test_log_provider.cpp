// SSO-3363 / audit H2: LogProvider::cancel() must not double-delete or
// dereference a null QProcess pointer when the connected finished() slot
// fires synchronously while waitForFinished() is processing events.
//
// SSO-3384 / audit WI-22: exercise the streaming ndjson parser the macOS
// log provider uses (MacOsLogStreamParser) — bounded memory, incremental
// parsing, and cap-and-kill semantics under synthetic `log show --style
// ndjson` input.

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "Pages/SystemLogs/log_provider.h"

class TestableLogProvider : public LogProvider
{
    Q_OBJECT
public:
    explicit TestableLogProvider(QObject *parent = nullptr)
        : LogProvider(parent) {}

    void fetchLogs(int /*maxEntries*/ = 0, int /*maxSeverity*/ = 0) override
    {
        if (mBusy)
            return;
        mBusy = true;
        mProcess = new QProcess(this);

        // The historical "broken" cleanup pattern: the slot nulls the member
        // and deletes the QProcess. cancel() must defend against this firing
        // during waitForFinished().
        connect(mProcess, &QProcess::finished, this,
                [this](int, QProcess::ExitStatus) {
                    if (mProcess) {
                        mProcess->deleteLater();
                        mProcess = nullptr;
                    }
                    mBusy = false;
                });

        mProcess->start(QStringLiteral("sh"),
                        {QStringLiteral("-c"), QStringLiteral("sleep 5")});
        mProcess->waitForStarted(3000);
    }

    // Start a child that exits immediately, so the connected slot performs
    // the cleanup naturally before cancel() ever runs.
    void fetchInstantlyFinishing()
    {
        if (mBusy)
            return;
        mBusy = true;
        mProcess = new QProcess(this);
        connect(mProcess, &QProcess::finished, this,
                [this](int, QProcess::ExitStatus) {
                    if (mProcess) {
                        mProcess->deleteLater();
                        mProcess = nullptr;
                    }
                    mBusy = false;
                });
        mProcess->start(QStringLiteral("sh"),
                        {QStringLiteral("-c"), QStringLiteral("true")});
    }

    QProcess *currentProcess() const { return mProcess; }
};

class TestLogProvider : public QObject
{
    Q_OBJECT

private slots:
    // SSO-3363 / H2: cancel() correctness against the historical UAF.
    void cancel_noOpWhenNoProcess();
    void cancel_safeWhileRunning();
    void cancel_safeWhenCalledTwice();
    void cancel_safeAfterNaturalFinish();

    // SSO-3384 / WI-22: MacOsLogStreamParser bounded-memory streaming.
    void streamParser_capsAtMaxEntriesAndSignalsKill();
    void streamParser_handlesPartialLinesAcrossChunks();
    void streamParser_dropsMalformedRecordsWithoutAborting();
    void streamParser_finishFlushesTrailingUnterminatedLine();
    void streamParser_takeEntriesReturnsNewestFirst();
    void streamParser_ignoresInputAfterCapReached();

private:
    // Build one ndjson line that mimics a real `log show --style ndjson`
    // record. We only fill the fields the parser actually reads.
    static QByteArray makeRecord(int seq, const char *type = "Default");
};

void TestLogProvider::cancel_noOpWhenNoProcess()
{
    TestableLogProvider provider;
    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);

    // Must not crash, must not change state.
    provider.cancel();

    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);
}

void TestLogProvider::cancel_safeWhileRunning()
{
    TestableLogProvider provider;
    provider.fetchLogs();

    QVERIFY(provider.isBusy());
    QProcess *p = provider.currentProcess();
    QVERIFY(p != nullptr);
    QCOMPARE(p->state(), QProcess::Running);

    QSignalSpy destroyedSpy(p, &QObject::destroyed);

    // The historical bug: waitForFinished() would synchronously deliver
    // finished() (from kill()'s CrashExit), the slot would null mProcess
    // and call deleteLater(), and the following mProcess->deleteLater()
    // in cancel() would dereference null.
    provider.cancel();

    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);

    // The QProcess must be destroyed exactly once, via the deleteLater()
    // queued by cancel() itself (slot was disconnected first).
    QTRY_COMPARE_WITH_TIMEOUT(destroyedSpy.count(), 1, 5000);
}

void TestLogProvider::cancel_safeWhenCalledTwice()
{
    TestableLogProvider provider;
    provider.fetchLogs();
    QVERIFY(provider.currentProcess() != nullptr);

    provider.cancel();
    // Second cancel() must be a no-op on the now-null member.
    provider.cancel();

    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);
}

void TestLogProvider::cancel_safeAfterNaturalFinish()
{
    TestableLogProvider provider;
    provider.fetchInstantlyFinishing();

    // Spin the event loop until the slot has cleaned up.
    QTRY_VERIFY_WITH_TIMEOUT(provider.currentProcess() == nullptr, 5000);
    QVERIFY(!provider.isBusy());

    // Now cancel() runs with a null member — must early-return safely.
    provider.cancel();
}

QByteArray TestLogProvider::makeRecord(int seq, const char *type)
{
    // Stable, parseable ISO-8601-with-ms timestamps. Sequence grows so we
    // can assert ordering on `takeEntries()`.
    const QDateTime base = QDateTime(QDate(2026, 6, 10), QTime(12, 0, 0), Qt::UTC);
    const QString ts = base.addMSecs(seq).toString(Qt::ISODateWithMs);
    return QByteArray("{\"timestamp\":\"") + ts.toUtf8()
         + "\",\"messageType\":\"" + type + "\","
         + "\"subsystem\":\"com.example.subsystem\","
         + "\"process\":\"someproc\","
         + "\"eventMessage\":\"entry " + QByteArray::number(seq) + "\"}\n";
}

void TestLogProvider::streamParser_capsAtMaxEntriesAndSignalsKill()
{
    const int cap = 500;
    MacOsLogStreamParser parser(cap);

    // Feed 5000 records — 10× the cap — in 64 KB chunks so we exercise the
    // mid-chunk early-stop path the provider relies on.
    QByteArray bulk;
    bulk.reserve(1 << 20);
    for (int i = 0; i < 5000; ++i)
        bulk.append(makeRecord(i));

    bool capHit = false;
    int offset = 0;
    const int chunkSize = 64 * 1024;
    while (offset < bulk.size() && !capHit) {
        const int n = qMin(chunkSize, bulk.size() - offset);
        capHit = parser.feed(QByteArray::fromRawData(bulk.constData() + offset, n));
        offset += n;
    }

    QVERIFY2(capHit, "parser should signal cap reached before the full stream is consumed");
    QCOMPARE(parser.retainedCount(), cap);
    QCOMPARE(parser.capReached(), true);
    // Once capped, the partial buffer is cleared — no unbounded growth.
    QCOMPARE(parser.bufferedTailBytes(), 0);
}

void TestLogProvider::streamParser_handlesPartialLinesAcrossChunks()
{
    MacOsLogStreamParser parser(50);

    QByteArray bulk;
    for (int i = 0; i < 10; ++i)
        bulk.append(makeRecord(i));

    // Feed one byte at a time — every chunk boundary lands mid-line. The
    // parser must accumulate and only emit on '\n'.
    for (int i = 0; i < bulk.size(); ++i)
        parser.feed(QByteArray(1, bulk.at(i)));

    QCOMPARE(parser.retainedCount(), 10);
    QCOMPARE(parser.linesDropped(), 0);
    // No trailing newline-less line left over: every record ends in '\n'.
    QCOMPARE(parser.bufferedTailBytes(), 0);
}

void TestLogProvider::streamParser_dropsMalformedRecordsWithoutAborting()
{
    MacOsLogStreamParser parser(50);

    QByteArray stream;
    stream.append(makeRecord(0));
    stream.append("this is not json\n");
    stream.append(makeRecord(1));
    stream.append("{ \"missing\": \"closing brace\"\n");
    stream.append(makeRecord(2));

    parser.feed(stream);

    QCOMPARE(parser.retainedCount(), 3);
    QCOMPARE(parser.linesDropped(), 2);
}

void TestLogProvider::streamParser_finishFlushesTrailingUnterminatedLine()
{
    MacOsLogStreamParser parser(50);

    // A `log show` run that we kill mid-line leaves us with a final record
    // that has no trailing '\n'. `finish()` should flush it.
    QByteArray stream;
    stream.append(makeRecord(0));
    stream.append(makeRecord(1));
    QByteArray trailing = makeRecord(2);
    trailing.chop(1); // strip the '\n'
    stream.append(trailing);

    parser.feed(stream);
    QCOMPARE(parser.retainedCount(), 2);
    QVERIFY(parser.bufferedTailBytes() > 0);

    parser.finish();
    QCOMPARE(parser.retainedCount(), 3);
    QCOMPARE(parser.bufferedTailBytes(), 0);
}

void TestLogProvider::streamParser_takeEntriesReturnsNewestFirst()
{
    MacOsLogStreamParser parser(50);

    QByteArray stream;
    // Records 0..9 in chronological order — `takeEntries()` should return
    // them with the latest first to match the page's display contract.
    for (int i = 0; i < 10; ++i)
        stream.append(makeRecord(i));

    parser.feed(stream);
    QList<LogEntry> entries = parser.takeEntries();
    QCOMPARE(entries.size(), 10);
    for (int i = 1; i < entries.size(); ++i)
        QVERIFY2(entries[i - 1].timestamp >= entries[i].timestamp,
                 "entries should be sorted descending by timestamp");
}

void TestLogProvider::streamParser_ignoresInputAfterCapReached()
{
    MacOsLogStreamParser parser(10);

    QByteArray firstBatch;
    for (int i = 0; i < 10; ++i)
        firstBatch.append(makeRecord(i));
    QVERIFY(parser.feed(firstBatch));
    QCOMPARE(parser.retainedCount(), 10);

    // Any further chunks must be no-ops — both for retained count and for
    // the buffered tail (we never want to hold on to post-kill drainage).
    QByteArray afterCap;
    for (int i = 10; i < 100; ++i)
        afterCap.append(makeRecord(i));
    QVERIFY(parser.feed(afterCap));
    QCOMPARE(parser.retainedCount(), 10);
    QCOMPARE(parser.bufferedTailBytes(), 0);
}

QTEST_MAIN(TestLogProvider)
#include "test_log_provider.moc"
