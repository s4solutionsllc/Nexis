#include <QtTest>
#include <Pages/Helpers/open_ports_widget.h>

class TestOpenPorts : public QObject
{
    Q_OBJECT

private slots:
    // --- parseLsofOutput ---

    void lsof_listeningOnly()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n"
            "rapportd  1077 luke   11u  IPv4 0x5ad90bf9dbe89be0      0t0  TCP *:60328 (LISTEN)\n"
            "ControlCe 1250 luke    9u  IPv4 0xbb8f7f4630f2a7ed      0t0  TCP *:7000 (LISTEN)\n"
            "GitHub    1693 luke   26u  IPv4 0xe3d72b15505b1c0e      0t0  TCP 127.0.0.1:49247 (LISTEN)\n"
            "jetbrains 1914 luke  155u  IPv6 0xdfcecb1f25817333      0t0  TCP 127.0.0.1:49469 (LISTEN)\n";

        auto entries = OpenPortsWidget::parseLsofOutput(output);
        QCOMPARE(entries.size(), 4);

        QCOMPARE(entries[0].processName, "rapportd");
        QCOMPARE(entries[0].pid, 1077);
        QCOMPARE(entries[0].localAddress, "*");
        QCOMPARE(entries[0].localPort, 60328);
        QCOMPARE(entries[0].remoteAddress, "*");
        QCOMPARE(entries[0].remotePort, 0);
        QCOMPARE(entries[0].state, "LISTEN");
        QCOMPARE(entries[0].protocol, "TCP");

        QCOMPARE(entries[1].processName, "ControlCe");
        QCOMPARE(entries[1].localPort, 7000);

        QCOMPARE(entries[2].processName, "GitHub");
        QCOMPARE(entries[2].localAddress, "127.0.0.1");
        QCOMPARE(entries[2].localPort, 49247);

        QCOMPARE(entries[3].processName, "jetbrains");
        QCOMPARE(entries[3].pid, 1914);
        QCOMPARE(entries[3].localPort, 49469);
    }

    void lsof_mixedStates()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n"
            "sshd      1234 root    3u  IPv4 0xabc123      0t0  TCP *:22 (LISTEN)\n"
            "firefox   2345 luke   87u  IPv4 0xdef456      0t0  TCP 192.168.1.50:44556->142.250.80.46:443 (ESTABLISHED)\n"
            "curl      3456 luke    5u  IPv4 0x789abc      0t0  TCP 10.0.0.5:58300->93.184.216.34:80 (CLOSE_WAIT)\n";

        auto entries = OpenPortsWidget::parseLsofOutput(output);
        QCOMPARE(entries.size(), 3);

        QCOMPARE(entries[0].state, "LISTEN");
        QCOMPARE(entries[0].localPort, 22);
        QCOMPARE(entries[0].remoteAddress, "*");

        QCOMPARE(entries[1].state, "ESTABLISHED");
        QCOMPARE(entries[1].localAddress, "192.168.1.50");
        QCOMPARE(entries[1].localPort, 44556);
        QCOMPARE(entries[1].remoteAddress, "142.250.80.46");
        QCOMPARE(entries[1].remotePort, 443);
        QCOMPARE(entries[1].processName, "firefox");
        QCOMPARE(entries[1].pid, 2345);

        QCOMPARE(entries[2].state, "CLOSE_WAIT");
        QCOMPARE(entries[2].remoteAddress, "93.184.216.34");
        QCOMPARE(entries[2].remotePort, 80);
    }

    void lsof_ipv6()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n"
            "node      5678 luke   22u  IPv6 0xabc123      0t0  TCP [::1]:7679 (LISTEN)\n";

        auto entries = OpenPortsWidget::parseLsofOutput(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].localAddress, "[::1]");
        QCOMPARE(entries[0].localPort, 7679);
        QCOMPARE(entries[0].state, "LISTEN");
    }

    void lsof_wildcard()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n"
            "rapportd  1077 luke   11u  IPv4 0x5ad90bf9dbe89be0      0t0  TCP *:60328 (LISTEN)\n";

        auto entries = OpenPortsWidget::parseLsofOutput(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].localAddress, "*");
        QCOMPARE(entries[0].localPort, 60328);
    }

    void lsof_emptyOutput()
    {
        QCOMPARE(OpenPortsWidget::parseLsofOutput("").size(), 0);
    }

    void lsof_headerOnly()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n";
        QCOMPARE(OpenPortsWidget::parseLsofOutput(output).size(), 0);
    }

    void lsof_truncatedCommand()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n"
            "ControlCe 1250 luke    9u  IPv4 0xbb8f7f4630f2a7ed      0t0  TCP *:7000 (LISTEN)\n";

        auto entries = OpenPortsWidget::parseLsofOutput(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].processName, "ControlCe");
    }

    void lsof_timeWait()
    {
        const QString output =
            "COMMAND    PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME\n"
            "curl      9999 luke    5u  IPv4 0xabc123      0t0  TCP 10.0.0.5:55555->8.8.8.8:53 (TIME_WAIT)\n";

        auto entries = OpenPortsWidget::parseLsofOutput(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].state, "TIME_WAIT");
    }

    // --- parseSsOutput ---

    void ss_listeningOnly()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "LISTEN  0       128     0.0.0.0:22           0.0.0.0:*          users:((\"sshd\",pid=1234,fd=3))\n"
            "LISTEN  0       511     *:80                 *:*                users:((\"nginx\",pid=5678,fd=6))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 2);

        QCOMPARE(entries[0].state, "LISTEN");
        QCOMPARE(entries[0].localAddress, "0.0.0.0");
        QCOMPARE(entries[0].localPort, 22);
        QCOMPARE(entries[0].remoteAddress, "*");
        QCOMPARE(entries[0].remotePort, 0);
        QCOMPARE(entries[0].processName, "sshd");
        QCOMPARE(entries[0].pid, 1234);
        QCOMPARE(entries[0].protocol, "TCP");

        QCOMPARE(entries[1].localAddress, "*");
        QCOMPARE(entries[1].localPort, 80);
        QCOMPARE(entries[1].processName, "nginx");
        QCOMPARE(entries[1].pid, 5678);
    }

    void ss_established()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port   Process\n"
            "ESTAB   0       0       192.168.1.50:44556   142.250.80.46:443   users:((\"firefox\",pid=2345,fd=87))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].state, "ESTAB");
        QCOMPARE(entries[0].localAddress, "192.168.1.50");
        QCOMPARE(entries[0].localPort, 44556);
        QCOMPARE(entries[0].remoteAddress, "142.250.80.46");
        QCOMPARE(entries[0].remotePort, 443);
        QCOMPARE(entries[0].processName, "firefox");
        QCOMPARE(entries[0].pid, 2345);
    }

    void ss_ipv6()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "LISTEN  0       128     [::]:22              [::]:*             users:((\"sshd\",pid=1234,fd=4))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].localAddress, "[::]");
        QCOMPARE(entries[0].localPort, 22);
        QCOMPARE(entries[0].remoteAddress, "*");
    }

    void ss_interfaceScoped()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "LISTEN  0       4096    127.0.0.53%lo:53     0.0.0.0:*          users:((\"systemd-resolve\",pid=867,fd=14))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].localAddress, "127.0.0.53");
        QCOMPARE(entries[0].localPort, 53);
        QCOMPARE(entries[0].processName, "systemd-resolve");
    }

    void ss_noProcessInfo()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "LISTEN  0       128     0.0.0.0:22           0.0.0.0:*\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].localPort, 22);
        QCOMPARE(entries[0].processName, "");
        QCOMPARE(entries[0].pid, -1);
    }

    void ss_emptyOutput()
    {
        QCOMPARE(OpenPortsWidget::parseSsOutput("").size(), 0);
    }

    void ss_headerOnly()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n";
        QCOMPARE(OpenPortsWidget::parseSsOutput(output).size(), 0);
    }

    void ss_wildcardPort()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "LISTEN  0       128     0.0.0.0:8080         *:*                users:((\"httpd\",pid=999,fd=5))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].remoteAddress, "*");
        QCOMPARE(entries[0].remotePort, 0);
    }

    void ss_protocolParameter()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "UNCONN  0       0       0.0.0.0:5353         0.0.0.0:*          users:((\"avahi-daemon\",pid=500,fd=12))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "UDP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].protocol, "UDP");
        QCOMPARE(entries[0].state, "UNCONN");
    }

    void ss_dottedProcessName()
    {
        const QString output =
            "State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process\n"
            "LISTEN  0       128     0.0.0.0:3306         0.0.0.0:*          users:((\"mysqld.safe\",pid=1111,fd=3))\n";

        auto entries = OpenPortsWidget::parseSsOutput(output, "TCP");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].processName, "mysqld.safe");
        QCOMPARE(entries[0].pid, 1111);
    }
};

QTEST_MAIN(TestOpenPorts)
#include "test_open_ports.moc"
