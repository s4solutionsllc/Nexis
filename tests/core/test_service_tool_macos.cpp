#include <QtTest>
#include <QTemporaryDir>

#include "service_tool_macos.h"

// SSO-25782: ServiceToolMacOS's parsing and command construction. Nothing
// here runs launchctl; the helpers are pure, and plist discovery reads from a
// temp directory.

using Job = ServiceToolMacOS::Job;
using Domain = ServiceToolMacOS::Domain;

class TestServiceToolMacOS : public QObject
{
    Q_OBJECT

private:
    static void writePlist(const QString &path, const QString &body)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>%1</dict></plist>\n").arg(body).toUtf8());
    }

private slots:
    void launchctlList_mapsPidToRunning()
    {
        const QString output = QStringLiteral(
            "PID\tStatus\tLabel\n"
            "-\t0\tcom.example.idle\n"
            "812\t0\tcom.example.busy\n"
            "-\t-9\tcom.example.crashed\n");
        const QHash<QString, bool> jobs = ServiceToolMacOS::parseLaunchctlList(output);

        QCOMPARE(jobs.size(), 3);
        QCOMPARE(jobs.value("com.example.idle"), false);
        QCOMPARE(jobs.value("com.example.busy"), true);
        QCOMPARE(jobs.value("com.example.crashed"), false);
    }

    void printDisabled_readsBothStates_andLegacyBooleans()
    {
        const QString output = QStringLiteral(
            "\tdisabled services = {\n"
            "\t\t\"com.example.on\" => enabled\n"
            "\t\t\"com.example.off\" => disabled\n"
            "\t\t\"com.example.legacy-off\" => true\n"
            "\t\t\"com.example.legacy-on\" => false\n"
            "\t}\n");
        const QHash<QString, bool> overrides = ServiceToolMacOS::parsePrintDisabled(output);

        QCOMPARE(overrides.size(), 4);
        QCOMPARE(overrides.value("com.example.on"), false);
        QCOMPARE(overrides.value("com.example.off"), true);
        QCOMPARE(overrides.value("com.example.legacy-off"), true);
        QCOMPARE(overrides.value("com.example.legacy-on"), false);
    }

    void enabled_overrideWinsOverPlistDisabledKey()
    {
        Job job;
        job.label = "com.example.job";

        QVERIFY(ServiceToolMacOS::isEnabled(job, {}));

        job.disabledInPlist = true;
        QVERIFY(!ServiceToolMacOS::isEnabled(job, {}));
        QVERIFY(ServiceToolMacOS::isEnabled(job, {{"com.example.job", false}}));

        job.disabledInPlist = false;
        QVERIFY(!ServiceToolMacOS::isEnabled(job, {{"com.example.job", true}}));
    }

    void printState_onlyTopLevelStateCounts()
    {
        const QString running = QStringLiteral(
            "gui/501/com.example.job = {\n"
            "\tpath = /Library/LaunchAgents/com.example.job.plist\n"
            "\tstate = running\n"
            "\tpid = 30789\n"
            "\tendpoints = {\n"
            "\t\t\"com.example.job\" = {\n"
            "\t\t\tstate = active\n"
            "\t\t}\n"
            "\t}\n"
            "}\n");
        QVERIFY(ServiceToolMacOS::parsePrintIsRunning(running));

        QString stopped = running;
        stopped.replace("\tstate = running\n", "\tstate = not running\n");
        QVERIFY(!ServiceToolMacOS::parsePrintIsRunning(stopped));

        QVERIFY(!ServiceToolMacOS::parsePrintIsRunning(
            QStringLiteral("Bad request.\nCould not find service \"x\" in domain for system\n")));
    }

    void hiddenLabels_appleAndPerLaunchApplicationEntries()
    {
        QVERIFY(ServiceToolMacOS::isHiddenLabel(""));
        QVERIFY(ServiceToolMacOS::isHiddenLabel("com.apple.Finder"));
        QVERIFY(ServiceToolMacOS::isHiddenLabel("application.com.example.App.123.456"));
        QVERIFY(ServiceToolMacOS::isHiddenLabel("[0x0-0x1234].com.example"));
        QVERIFY(!ServiceToolMacOS::isHiddenLabel("com.example.agent"));
    }

    void jobsInDirectory_readsLabelProgramAndDisabled()
    {
        QTemporaryDir tmp;
        // File name deliberately differs from the Label.
        writePlist(tmp.filePath("renamed.plist"),
            "<key>Label</key><string>com.example.args</string>"
            "<key>ProgramArguments</key><array><string>/opt/example/bin/tool</string>"
            "<string>--serve</string></array>");
        writePlist(tmp.filePath("b.plist"),
            "<key>Label</key><string>com.example.program</string>"
            "<key>Program</key><string>/opt/example/bin/daemon</string>"
            "<key>Disabled</key><true/>");
        writePlist(tmp.filePath("c.plist"),
            "<key>Label</key><string>com.apple.hidden</string>");
        writePlist(tmp.filePath("d.plist"), "");

        QList<Job> jobs = ServiceToolMacOS::jobsInDirectory(tmp.path(), Domain::SystemDaemon);
        std::sort(jobs.begin(), jobs.end(), [](const Job &a, const Job &b) { return a.label < b.label; });

        QCOMPARE(jobs.size(), 2);
        QCOMPARE(jobs.at(0).label, QStringLiteral("com.example.args"));
        QCOMPARE(jobs.at(0).program, QStringLiteral("/opt/example/bin/tool"));
        QCOMPARE(jobs.at(0).plistPath, QFileInfo(tmp.filePath("renamed.plist")).absoluteFilePath());
        QCOMPARE(jobs.at(0).disabledInPlist, false);
        QVERIFY(jobs.at(0).domain == Domain::SystemDaemon);
        QCOMPARE(jobs.at(1).label, QStringLiteral("com.example.program"));
        QCOMPARE(jobs.at(1).program, QStringLiteral("/opt/example/bin/daemon"));
        QCOMPARE(jobs.at(1).disabledInPlist, true);
    }

    void enableCommand_agentsUnelevatedInGuiDomain_daemonsElevatedInSystem()
    {
        Job agent;
        agent.label = "com.example.agent";
        agent.domain = Domain::UserAgent;

        const auto enable = ServiceToolMacOS::enableCommand(agent, true, 501);
        QCOMPARE(enable.args, (QStringList{"enable", "gui/501/com.example.agent"}));
        QVERIFY(!enable.elevated);

        Job daemon;
        daemon.label = "com.example.daemon";
        daemon.domain = Domain::SystemDaemon;

        const auto disable = ServiceToolMacOS::enableCommand(daemon, false, 501);
        QCOMPARE(disable.args, (QStringList{"disable", "system/com.example.daemon"}));
        QVERIFY(disable.elevated);
    }

    void activeCommand_bootstrapWhenUnloaded_kickstartWhenLoaded_bootoutToStop()
    {
        Job agent;
        agent.label = "com.example.agent";
        agent.plistPath = "/Users/x/Library/LaunchAgents/com.example.agent.plist";

        QCOMPARE(ServiceToolMacOS::activeCommand(agent, true, false, 501).args,
                 (QStringList{"bootstrap", "gui/501", agent.plistPath}));
        QCOMPARE(ServiceToolMacOS::activeCommand(agent, true, true, 501).args,
                 (QStringList{"kickstart", "gui/501/com.example.agent"}));
        QCOMPARE(ServiceToolMacOS::activeCommand(agent, false, true, 501).args,
                 (QStringList{"bootout", "gui/501/com.example.agent"}));

        // A runtime-registered agent has no plist to bootstrap from.
        Job noPlist;
        noPlist.label = "com.example.runtime";
        QCOMPARE(ServiceToolMacOS::activeCommand(noPlist, true, false, 501).args,
                 (QStringList{"kickstart", "gui/501/com.example.runtime"}));
    }
};

QTEST_GUILESS_MAIN(TestServiceToolMacOS)
#include "test_service_tool_macos.moc"
