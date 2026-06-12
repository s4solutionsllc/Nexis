#include <QFile>
#include <QStringList>
#include <QTest>

#include "oomd_info_parser.h"

class TestOomdInfoParser : public QObject
{
    Q_OBJECT

private:
    QByteArray loadFixture(const QString &name) const
    {
        const QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                             + "/tests/fixtures/oomd/" + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

    QStringList loadFixtureLines(const QString &name) const
    {
        return QString::fromUtf8(loadFixture(name))
            .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

private slots:
    void parseSystemctlShow_basic();
    void parseSystemctlShow_emptyValue();
    void parseSystemctlShow_ignoresBlankAndKeyless();
    void parseSystemctlShow_masked();

    void parseCgroupV2KeyedFile_basic();
    void parseCgroupV2KeyedFile_ignoresMalformed();

    void parseOomdJournalLines_killEvents();
    void parseOomdJournalLines_ignoresInformational();
    void parseOomdJournalLines_capsAtMax();
    void parseOomdJournalLines_unitFromCgroupPath();

    void assembleSnapshot_combinesAll();
    void assembleSnapshot_oomdMaskedStillReportsKernelKill();
    void assembleSnapshot_unavailableWhenSilent();
    void assembleSnapshot_cgroupV2FlagPropagates();
};

void TestOomdInfoParser::parseSystemctlShow_basic()
{
    const QByteArray bytes = loadFixture(QStringLiteral("systemctl_show_active.txt"));
    QVERIFY(!bytes.isEmpty());

    const auto props = OomdInfoParser::parseSystemctlShow(bytes);
    QCOMPARE(props.value(QStringLiteral("LoadState")), QStringLiteral("loaded"));
    QCOMPARE(props.value(QStringLiteral("ActiveState")), QStringLiteral("active"));
    QCOMPARE(props.value(QStringLiteral("OOMKills")), QStringLiteral("42"));
    QCOMPARE(props.value(QStringLiteral("ManagedOOMKills")), QStringLiteral("39"));
}

void TestOomdInfoParser::parseSystemctlShow_emptyValue()
{
    const QByteArray bytes = QByteArrayLiteral("LoadState=\nActiveState=inactive\n");
    const auto props = OomdInfoParser::parseSystemctlShow(bytes);
    // LoadState should be present-but-empty.
    QVERIFY(props.contains(QStringLiteral("LoadState")));
    QCOMPARE(props.value(QStringLiteral("LoadState")), QString());
    QCOMPARE(props.value(QStringLiteral("ActiveState")), QStringLiteral("inactive"));
}

void TestOomdInfoParser::parseSystemctlShow_ignoresBlankAndKeyless()
{
    const QByteArray bytes = QByteArrayLiteral(
        "\n=onlyvalue\nLoadState=loaded\n   \n=other\nOOMKills=7\n");
    const auto props = OomdInfoParser::parseSystemctlShow(bytes);
    QCOMPARE(props.size(), 2);
    QCOMPARE(props.value(QStringLiteral("LoadState")), QStringLiteral("loaded"));
    QCOMPARE(props.value(QStringLiteral("OOMKills")), QStringLiteral("7"));
}

void TestOomdInfoParser::parseSystemctlShow_masked()
{
    const QByteArray bytes = loadFixture(QStringLiteral("systemctl_show_masked.txt"));
    const auto props = OomdInfoParser::parseSystemctlShow(bytes);
    QCOMPARE(props.value(QStringLiteral("LoadState")), QStringLiteral("masked"));
    QCOMPARE(props.value(QStringLiteral("OOMKills")), QStringLiteral("0"));
}

void TestOomdInfoParser::parseCgroupV2KeyedFile_basic()
{
    const QByteArray bytes = loadFixture(QStringLiteral("memory_events_basic.txt"));
    const auto events = OomdInfoParser::parseCgroupV2KeyedFile(bytes);
    QCOMPARE(events.value(QStringLiteral("low")), 0ULL);
    QCOMPARE(events.value(QStringLiteral("oom")), 4ULL);
    QCOMPARE(events.value(QStringLiteral("oom_kill")), 3ULL);
    QCOMPARE(events.value(QStringLiteral("oom_group_kill")), 0ULL);
}

void TestOomdInfoParser::parseCgroupV2KeyedFile_ignoresMalformed()
{
    // Non-numeric value rows are skipped; missing-separator rows are skipped.
    const QByteArray bytes = QByteArrayLiteral(
        "oom_kill 5\n"
        "garbage\n"
        "key not-a-number\n"
        "tabbed\t9\n"
        "  spaced 12\n");
    const auto events = OomdInfoParser::parseCgroupV2KeyedFile(bytes);
    QCOMPARE(events.value(QStringLiteral("oom_kill")), 5ULL);
    QCOMPARE(events.value(QStringLiteral("tabbed")), 9ULL);
    QCOMPARE(events.value(QStringLiteral("spaced")), 12ULL);
    QVERIFY(!events.contains(QStringLiteral("garbage")));
    QVERIFY(!events.contains(QStringLiteral("key")));
}

void TestOomdInfoParser::parseOomdJournalLines_killEvents()
{
    const QStringList lines = loadFixtureLines(QStringLiteral("journal_kill_events.txt"));
    QVERIFY(!lines.isEmpty());

    const auto events = OomdInfoParser::parseOomdJournalLines(lines);
    // 3 kill lines + 1 informational refresh line in the fixture; refresh dropped.
    QCOMPARE(events.size(), 3);

    QCOMPARE(events.at(0).tasksKilled, 3);
    QCOMPARE(events.at(0).unit, QStringLiteral("firefox.service"));
    QVERIFY(events.at(0).reason.contains(QStringLiteral("memory pressure")));
    QVERIFY(events.at(0).when.isValid());

    QCOMPARE(events.at(1).tasksKilled, 1);
    QCOMPARE(events.at(1).unit, QStringLiteral("session-2.scope"));
    QVERIFY(events.at(1).reason.contains(QStringLiteral("swap")));

    QCOMPARE(events.at(2).tasksKilled, 7);
    QCOMPARE(events.at(2).unit, QStringLiteral("postgresql.service"));
}

void TestOomdInfoParser::parseOomdJournalLines_ignoresInformational()
{
    const QStringList lines{
        QStringLiteral("2026-06-11T13:40:00+0000 host systemd-oomd[1]: Started monitoring /user.slice"),
        QStringLiteral("not-an-oomd-line"),
        QStringLiteral("2026-06-11T13:41:00+0000 host systemd-oomd[1]: Killed 2 tasks in /system.slice/app.service due to memory pressure"),
    };
    const auto events = OomdInfoParser::parseOomdJournalLines(lines);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).tasksKilled, 2);
    QCOMPARE(events.at(0).unit, QStringLiteral("app.service"));
}

void TestOomdInfoParser::parseOomdJournalLines_capsAtMax()
{
    QStringList lines;
    for (int i = 0; i < 5; ++i) {
        lines << QStringLiteral(
            "2026-06-11T13:30:%1+0000 host systemd-oomd[1]: Killed 1 task in "
            "/system.slice/svc-%2.service due to memory pressure")
            .arg(i, 2, 10, QChar('0'))
            .arg(i);
    }
    const auto capped = OomdInfoParser::parseOomdJournalLines(lines, 2);
    QCOMPARE(capped.size(), 2);
    QCOMPARE(capped.at(0).unit, QStringLiteral("svc-0.service"));
    QCOMPARE(capped.at(1).unit, QStringLiteral("svc-1.service"));
}

void TestOomdInfoParser::parseOomdJournalLines_unitFromCgroupPath()
{
    // Cgroup path with a .scope leaf — should pick the scope as the unit.
    const QStringList lines{
        QStringLiteral("2026-06-11T13:30:42+0000 host systemd-oomd[1]: Killed 1 task in /user.slice/user-1000.slice/session-7.scope due to memory pressure"),
    };
    const auto events = OomdInfoParser::parseOomdJournalLines(lines);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).unit, QStringLiteral("session-7.scope"));
    QCOMPARE(events.at(0).cgroupPath,
             QStringLiteral("/user.slice/user-1000.slice/session-7.scope"));
}

void TestOomdInfoParser::assembleSnapshot_combinesAll()
{
    const auto props = OomdInfoParser::parseSystemctlShow(
        loadFixture(QStringLiteral("systemctl_show_active.txt")));
    const auto cgroup = OomdInfoParser::parseCgroupV2KeyedFile(
        loadFixture(QStringLiteral("memory_events_basic.txt")));
    const auto events = OomdInfoParser::parseOomdJournalLines(
        loadFixtureLines(QStringLiteral("journal_kill_events.txt")));

    const auto snap = OomdInfoParser::assembleSnapshot(
        /*cgroupV2=*/true, props, cgroup, events);

    QVERIFY(snap.available);
    QVERIFY(snap.cgroupV2);
    QCOMPARE(snap.loadState, QStringLiteral("loaded"));
    QCOMPARE(snap.activeState, QStringLiteral("active"));
    QCOMPARE(snap.oomKills, 42ULL);
    QCOMPARE(snap.managedOomKills, 39ULL);
    QCOMPARE(snap.systemOomKill, 3ULL);
    QCOMPARE(snap.recentEvents.size(), 3);
}

void TestOomdInfoParser::assembleSnapshot_oomdMaskedStillReportsKernelKill()
{
    const auto props = OomdInfoParser::parseSystemctlShow(
        loadFixture(QStringLiteral("systemctl_show_masked.txt")));
    const QMap<QString, quint64> cgroup{
        {QStringLiteral("oom_kill"), 11ULL}
    };
    const auto snap = OomdInfoParser::assembleSnapshot(
        /*cgroupV2=*/true, props, cgroup, {});

    QVERIFY(snap.available);
    QCOMPARE(snap.loadState, QStringLiteral("masked"));
    QCOMPARE(snap.oomKills, 0ULL);
    QCOMPARE(snap.systemOomKill, 11ULL);
    QVERIFY(snap.recentEvents.isEmpty());
}

void TestOomdInfoParser::assembleSnapshot_unavailableWhenSilent()
{
    const auto snap = OomdInfoParser::assembleSnapshot(
        /*cgroupV2=*/false,
        QMap<QString, QString>(),
        QMap<QString, quint64>(),
        {});
    QVERIFY(!snap.available);
    QVERIFY(!snap.cgroupV2);
    QVERIFY(snap.recentEvents.isEmpty());
}

void TestOomdInfoParser::assembleSnapshot_cgroupV2FlagPropagates()
{
    const auto snap = OomdInfoParser::assembleSnapshot(
        /*cgroupV2=*/true,
        QMap<QString, QString>(),
        QMap<QString, quint64>(),
        {});
    QVERIFY(snap.cgroupV2);
    // Even with no systemd-oomd or memory.events data, the presence of v2
    // alone is enough signal to render the "v2 unified hierarchy detected"
    // panel rather than hiding it.
    QVERIFY(snap.available);
}

QTEST_MAIN(TestOomdInfoParser)
#include "test_oomd_info_parser.moc"
