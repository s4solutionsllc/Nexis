#include <QtTest>
#include <Pages/Helpers/firewall_widget.h>
#include <Utils/command_util.h>

class TestFirewall : public QObject
{
    Q_OBJECT

private slots:
    // --- parseMacFirewallOutput ---

    void mac_firewallEnabled()
    {
        auto s = FirewallWidget::parseMacFirewallOutput(
            "Firewall is enabled. (State = 1)",
            "Firewall stealth mode is off",
            "Firewall has block all state set to disabled.",
            "Total number of apps = 7");

        QVERIFY(s.available);
        QVERIFY(s.enabled);
        QVERIFY(!s.stealthMode);
        QVERIFY(!s.blockAll);
        QCOMPARE(s.appRuleCount, 7);
        QCOMPARE(s.backend, "macOS Application Firewall");
    }

    void mac_firewallDisabled()
    {
        auto s = FirewallWidget::parseMacFirewallOutput(
            "Firewall is disabled. (State = 0)",
            "Firewall stealth mode is off",
            "Firewall has block all state set to disabled.",
            "Total number of apps = 0");

        QVERIFY(s.available);
        QVERIFY(!s.enabled);
        QCOMPARE(s.appRuleCount, 0);
    }

    void mac_stealthOn()
    {
        auto s = FirewallWidget::parseMacFirewallOutput(
            "Firewall is enabled. (State = 1)",
            "Firewall stealth mode is on",
            "Firewall has block all state set to disabled.",
            "Total number of apps = 3");

        QVERIFY(s.stealthMode);
    }

    void mac_blockAllEnabled()
    {
        auto s = FirewallWidget::parseMacFirewallOutput(
            "Firewall is enabled. (State = 1)",
            "Firewall stealth mode is off",
            "Firewall has block all state set to enabled.",
            "Total number of apps = 0");

        QVERIFY(s.blockAll);
    }

    void mac_allFeaturesOn()
    {
        auto s = FirewallWidget::parseMacFirewallOutput(
            "Firewall is enabled. (State = 1)",
            "Firewall stealth mode is on",
            "Firewall has block all state set to enabled.",
            "Total number of apps = 12");

        QVERIFY(s.enabled);
        QVERIFY(s.stealthMode);
        QVERIFY(s.blockAll);
        QCOMPARE(s.appRuleCount, 12);
    }

    void mac_emptyOutput()
    {
        auto s = FirewallWidget::parseMacFirewallOutput("", "", "", "");

        QVERIFY(s.available);
        QVERIFY(!s.enabled);
        QVERIFY(!s.stealthMode);
        QVERIFY(!s.blockAll);
        QCOMPARE(s.appRuleCount, -1);
    }

    void mac_listAppsWithEntries()
    {
        const QString listApps =
            "Total number of apps = 3 \n"
            "1 : /usr/libexec/remoted \n"
            "             (Allow incoming connections)\n"
            "2 : /usr/bin/python3 \n"
            "             (Allow incoming connections)\n"
            "3 : /usr/sbin/cupsd \n"
            "             (Allow incoming connections)\n";

        auto s = FirewallWidget::parseMacFirewallOutput(
            "Firewall is enabled. (State = 1)",
            "Firewall stealth mode is off",
            "Firewall has block all state set to disabled.",
            listApps);

        QCOMPARE(s.appRuleCount, 3);
    }

    // --- parseUfwOutput ---

    void ufw_active()
    {
        auto s = FirewallWidget::parseUfwOutput("Status: active\n");

        QVERIFY(s.available);
        QVERIFY(s.enabled);
        QCOMPARE(s.backend, "ufw");
    }

    void ufw_inactive()
    {
        auto s = FirewallWidget::parseUfwOutput("Status: inactive\n");

        QVERIFY(s.available);
        QVERIFY(!s.enabled);
        QCOMPARE(s.backend, "ufw");
    }

    void ufw_activeWithRules()
    {
        const QString output =
            "Status: active\n\n"
            "To                         Action      From\n"
            "--                         ------      ----\n"
            "22/tcp                     ALLOW       Anywhere\n"
            "80/tcp                     ALLOW       Anywhere\n";

        auto s = FirewallWidget::parseUfwOutput(output);
        QVERIFY(s.enabled);
    }

    void ufw_emptyOutput()
    {
        auto s = FirewallWidget::parseUfwOutput("");
        QVERIFY(!s.available);
    }

    // --- parseFirewalldOutput ---

    void firewalld_running()
    {
        auto s = FirewallWidget::parseFirewalldOutput("running\n");

        QVERIFY(s.available);
        QVERIFY(s.enabled);
        QCOMPARE(s.backend, "firewalld");
    }

    void firewalld_notRunning()
    {
        auto s = FirewallWidget::parseFirewalldOutput("not running\n");

        QVERIFY(s.available);
        QVERIFY(!s.enabled);
        QCOMPARE(s.backend, "firewalld");
    }

    void firewalld_emptyOutput()
    {
        auto s = FirewallWidget::parseFirewalldOutput("");
        QVERIFY(!s.available);
    }

    // --- describeExecFailure (SSO-3469: toggleFirewall's sudoExecWithStatus
    // migration — ExecResult::ok() is now authoritative, no re-read needed) ---

    void describeExecFailure_emptyOnSuccess()
    {
        ExecResult r;
        r.exitCode = 0;
        QVERIFY(r.ok());
        QVERIFY(FirewallWidget::describeExecFailure(r).isEmpty());
    }

    void describeExecFailure_usesStderrWhenPresent()
    {
        ExecResult r;
        r.exitCode = 1;
        r.error = "Operation not permitted";
        QCOMPARE(FirewallWidget::describeExecFailure(r), QString("Operation not permitted"));
    }

    void describeExecFailure_fallsBackToExitCodeWhenNoStderr()
    {
        ExecResult r;
        r.exitCode = 127;
        const QString msg = FirewallWidget::describeExecFailure(r);
        QVERIFY(!msg.isEmpty());
        QVERIFY(msg.contains("127"));
    }
};

QTEST_MAIN(TestFirewall)
#include "test_firewall.moc"
