// SSO-15566 / SSO-15373 (CISO §4): the running-process warn/quit gate that
// sits ahead of PackageService::trashApps() in the macOS uninstaller flow.
// RunningAppGate::filterRunnable() holds the decision logic free of
// QWidget/QDialog so it can be exercised here with a mocked isRunning
// predicate and a mocked prompt/quit outcome — no display required.
//
// Tests cover:
//   1. An app that isn't running never prompts and is always included.
//   2. A running app prompts; accepting (user quit it) includes it.
//   3. A running app prompts; cancelling excludes only that item.
//   4. A mixed batch: cancelling one running app doesn't drop the others —
//      matches the acceptance criterion that a per-item cancel never blocks
//      the rest of the uninstall batch.

#include <QSet>
#include <QTest>

#include "Tools/running_app_gate.h"

class TestRunningAppGate : public QObject
{
    Q_OBJECT

private slots:
    void filterRunnable_notRunning_includedWithoutPrompt();
    void filterRunnable_runningAndQuitConfirmed_included();
    void filterRunnable_runningAndCancelled_excluded();
    void filterRunnable_mixedBatch_onlyCancelledItemExcluded();
};

void TestRunningAppGate::filterRunnable_notRunning_includedWithoutPrompt()
{
    int promptCalls = 0;
    const QStringList result = RunningAppGate::filterRunnable(
        {"/Applications/Idle.app"},
        [](const QString &) { return false; },
        [&promptCalls](const QString &) { ++promptCalls; return false; });

    QCOMPARE(result, QStringList{"/Applications/Idle.app"});
    QCOMPARE(promptCalls, 0);
}

void TestRunningAppGate::filterRunnable_runningAndQuitConfirmed_included()
{
    const QStringList result = RunningAppGate::filterRunnable(
        {"/Applications/Running.app"},
        [](const QString &) { return true; },
        [](const QString &) { return true; });

    QCOMPARE(result, QStringList{"/Applications/Running.app"});
}

void TestRunningAppGate::filterRunnable_runningAndCancelled_excluded()
{
    const QStringList result = RunningAppGate::filterRunnable(
        {"/Applications/Running.app"},
        [](const QString &) { return true; },
        [](const QString &) { return false; });

    QVERIFY(result.isEmpty());
}

void TestRunningAppGate::filterRunnable_mixedBatch_onlyCancelledItemExcluded()
{
    const QString idle    = "/Applications/Idle.app";
    const QString quit    = "/Applications/QuitsCleanly.app";
    const QString refused = "/Applications/UserCancels.app";

    const QSet<QString> running = {quit, refused};

    const QStringList result = RunningAppGate::filterRunnable(
        {idle, quit, refused},
        [&running](const QString &path) { return running.contains(path); },
        [&refused](const QString &path) { return path != refused; });

    // Cancelling the "UserCancels" app must not remove the other two items
    // from the batch — each app's outcome is independent.
    QCOMPARE(result, (QStringList{idle, quit}));
}

QTEST_MAIN(TestRunningAppGate)
#include "test_running_app_gate.moc"
