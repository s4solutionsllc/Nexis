#include <QTest>
#include <cstring>

#include "Utils/headless_util.h"

// SSO-3368 / audit H6: pure decision functions used by main() to steer
// scheduled `--clean` runs onto the offscreen QPA platform before
// QApplication construction. These tests cover the parse + decision
// directly because launching the real binary headless is impractical
// in this CI matrix (no Qt platform plugin guarantee on builders).

class TestHeadlessUtil : public QObject
{
    Q_OBJECT

private slots:
    // ── isHeadlessArgv ───────────────────────────────────────────────────────
    void isHeadlessArgv_noArgs_false();
    void isHeadlessArgv_onlyProgramName_false();
    void isHeadlessArgv_guiFlags_false();
    void isHeadlessArgv_clean_true();
    void isHeadlessArgv_cleanLast_true();
    void isHeadlessArgv_cleanWithoutId_stillTrue();
    void isHeadlessArgv_checkThreshold_true();
    void isHeadlessArgv_mixedWithHide_true();
    void isHeadlessArgv_prefixOnly_false();
    void isHeadlessArgv_nullArgv_false();

    // ── shouldForceOffscreen ─────────────────────────────────────────────────
    void shouldForceOffscreen_headlessNoEnv_true();
    void shouldForceOffscreen_headlessWithEnv_false();
    void shouldForceOffscreen_guiNoEnv_false();
    void shouldForceOffscreen_guiWithEnv_false();

private:
    // QTest's helpers need char* (non-const) argv to match int main(int, char*[]).
    static int callIsHeadless(std::initializer_list<const char *> args)
    {
        QList<QByteArray> storage;
        storage.reserve(static_cast<int>(args.size()));
        for (const char *a : args) {
            storage.append(QByteArray(a));
        }
        QVarLengthArray<char *, 8> argv;
        for (QByteArray &s : storage) {
            argv.append(s.data());
        }
        return HeadlessUtil::isHeadlessArgv(argv.size(), argv.data()) ? 1 : 0;
    }
};

void TestHeadlessUtil::isHeadlessArgv_noArgs_false()
{
    QCOMPARE(HeadlessUtil::isHeadlessArgv(0, nullptr), false);
}

void TestHeadlessUtil::isHeadlessArgv_onlyProgramName_false()
{
    QCOMPARE(callIsHeadless({"nexis"}), 0);
}

void TestHeadlessUtil::isHeadlessArgv_guiFlags_false()
{
    QCOMPARE(callIsHeadless({"nexis", "--hide", "--nosplash"}), 0);
}

void TestHeadlessUtil::isHeadlessArgv_clean_true()
{
    QCOMPARE(callIsHeadless({"nexis", "--clean", "schedule-abc"}), 1);
}

void TestHeadlessUtil::isHeadlessArgv_cleanLast_true()
{
    // --clean as the very last argv entry must still mark the run headless,
    // even though main()'s own arg parse won't capture a schedule id.
    QCOMPARE(callIsHeadless({"nexis", "--clean"}), 1);
}

void TestHeadlessUtil::isHeadlessArgv_cleanWithoutId_stillTrue()
{
    QCOMPARE(callIsHeadless({"nexis", "--clean", "--hide"}), 1);
}

void TestHeadlessUtil::isHeadlessArgv_checkThreshold_true()
{
    QCOMPARE(callIsHeadless({"nexis", "--check-threshold"}), 1);
}

void TestHeadlessUtil::isHeadlessArgv_mixedWithHide_true()
{
    QCOMPARE(callIsHeadless({"nexis", "--hide", "--clean", "id"}), 1);
}

void TestHeadlessUtil::isHeadlessArgv_prefixOnly_false()
{
    // Substring matches must not trip the detector.
    QCOMPARE(callIsHeadless({"nexis", "--cleanup", "--check-threshold-foo"}), 0);
}

void TestHeadlessUtil::isHeadlessArgv_nullArgv_false()
{
    QCOMPARE(HeadlessUtil::isHeadlessArgv(5, nullptr), false);
}

void TestHeadlessUtil::shouldForceOffscreen_headlessNoEnv_true()
{
    QCOMPARE(HeadlessUtil::shouldForceOffscreen(true, false), true);
}

void TestHeadlessUtil::shouldForceOffscreen_headlessWithEnv_false()
{
    // If the user (or test harness) pinned QT_QPA_PLATFORM, respect it.
    QCOMPARE(HeadlessUtil::shouldForceOffscreen(true, true), false);
}

void TestHeadlessUtil::shouldForceOffscreen_guiNoEnv_false()
{
    QCOMPARE(HeadlessUtil::shouldForceOffscreen(false, false), false);
}

void TestHeadlessUtil::shouldForceOffscreen_guiWithEnv_false()
{
    QCOMPARE(HeadlessUtil::shouldForceOffscreen(false, true), false);
}

QTEST_APPLESS_MAIN(TestHeadlessUtil)
#include "test_headless_util.moc"
