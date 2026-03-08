#include <QtTest>
#include <Pages/Helpers/network_diag_widget.h>

class TestNetworkDiag : public QObject
{
    Q_OBJECT

private slots:
    // --- parseGatewayFromRoute (macOS) ---

    void route_normalOutput()
    {
        const QString output =
            "   route to: default\n"
            "destination: default\n"
            "       mask: default\n"
            "    gateway: 192.168.1.1\n"
            "  interface: en0\n"
            "      flags: <UP,GATEWAY,DONE,STATIC,PRCLONING,AUTOCONF>\n"
            " recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount\n"
            "       0         0         0         0         0         0\n";
        QCOMPARE(NetworkDiagWidget::parseGatewayFromRoute(output), "192.168.1.1");
    }

    void route_noGateway()
    {
        const QString output =
            "   route to: default\n"
            "destination: default\n"
            "  interface: lo0\n";
        QCOMPARE(NetworkDiagWidget::parseGatewayFromRoute(output), "");
    }

    void route_emptyOutput()
    {
        QCOMPARE(NetworkDiagWidget::parseGatewayFromRoute(""), "");
    }

    void route_differentGateway()
    {
        const QString output = "    gateway: 10.0.0.1\n  interface: en1\n";
        QCOMPARE(NetworkDiagWidget::parseGatewayFromRoute(output), "10.0.0.1");
    }

    // --- parseGatewayFromIpRoute (Linux) ---

    void ipRoute_normalOutput()
    {
        const QString output =
            "default via 192.168.1.1 dev wlp2s0 proto dhcp metric 600\n"
            "172.17.0.0/16 dev docker0 proto kernel scope link src 172.17.0.1\n"
            "192.168.1.0/24 dev wlp2s0 proto kernel scope link src 192.168.1.42\n";
        QCOMPARE(NetworkDiagWidget::parseGatewayFromIpRoute(output), "192.168.1.1");
    }

    void ipRoute_noDefault()
    {
        const QString output =
            "172.17.0.0/16 dev docker0 proto kernel scope link src 172.17.0.1\n"
            "192.168.1.0/24 dev wlp2s0 proto kernel scope link src 192.168.1.42\n";
        QCOMPARE(NetworkDiagWidget::parseGatewayFromIpRoute(output), "");
    }

    void ipRoute_emptyOutput()
    {
        QCOMPARE(NetworkDiagWidget::parseGatewayFromIpRoute(""), "");
    }

    void ipRoute_multipleDefaults()
    {
        const QString output =
            "default via 10.0.0.1 dev eth0 metric 100\n"
            "default via 192.168.1.1 dev wlan0 metric 600\n";
        QCOMPARE(NetworkDiagWidget::parseGatewayFromIpRoute(output), "10.0.0.1");
    }

    // --- parsePingLatency ---

    void ping_normalOutput()
    {
        const QString output =
            "PING 1.1.1.1 (1.1.1.1): 56 data bytes\n"
            "64 bytes from 1.1.1.1: icmp_seq=0 ttl=55 time=14.326 ms\n"
            "\n"
            "--- 1.1.1.1 ping statistics ---\n"
            "1 packets transmitted, 1 packets received, 0.0% packet loss\n";
        QCOMPARE(NetworkDiagWidget::parsePingLatency(output), 14.326);
    }

    void ping_subMillisecond()
    {
        const QString output =
            "64 bytes from 192.168.1.1: icmp_seq=0 ttl=64 time=0.534 ms\n";
        QCOMPARE(NetworkDiagWidget::parsePingLatency(output), 0.534);
    }

    void ping_timeout()
    {
        const QString output =
            "PING 10.255.255.1 (10.255.255.1): 56 data bytes\n"
            "\n"
            "--- 10.255.255.1 ping statistics ---\n"
            "1 packets transmitted, 0 packets received, 100.0% packet loss\n";
        QCOMPARE(NetworkDiagWidget::parsePingLatency(output), -1.0);
    }

    void ping_emptyOutput()
    {
        QCOMPARE(NetworkDiagWidget::parsePingLatency(""), -1.0);
    }

    void ping_windowsStyleLessThan()
    {
        const QString output = "Reply from 192.168.1.1: bytes=32 time<1ms TTL=64\n";
        double lat = NetworkDiagWidget::parsePingLatency(output);
        QCOMPARE(lat, 1.0);
    }

    // --- parseDnsFromScutilDns (macOS) ---

    void scutil_multipleNameservers()
    {
        const QString output =
            "DNS configuration\n"
            "\n"
            "resolver #1\n"
            "  nameserver[0] : 8.8.8.8\n"
            "  nameserver[1] : 8.8.4.4\n"
            "  if_index : 6 (en0)\n"
            "\n"
            "resolver #2\n"
            "  nameserver[0] : 1.1.1.1\n"
            "  domain   : local\n";
        QStringList expected = {"8.8.8.8", "8.8.4.4", "1.1.1.1"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromScutilDns(output), expected);
    }

    void scutil_noNameservers()
    {
        const QString output =
            "DNS configuration\n"
            "\n"
            "resolver #1\n"
            "  domain   : local\n";
        QCOMPARE(NetworkDiagWidget::parseDnsFromScutilDns(output), QStringList());
    }

    void scutil_ipv6Mixed()
    {
        const QString output =
            "resolver #1\n"
            "  nameserver[0] : 8.8.8.8\n"
            "  nameserver[1] : 2001:4860:4860::8888\n";
        QStringList expected = {"8.8.8.8", "2001:4860:4860::8888"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromScutilDns(output), expected);
    }

    void scutil_duplicates()
    {
        const QString output =
            "resolver #1\n"
            "  nameserver[0] : 8.8.8.8\n"
            "resolver #2\n"
            "  nameserver[0] : 8.8.8.8\n";
        QStringList expected = {"8.8.8.8"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromScutilDns(output), expected);
    }

    // --- parseDnsFromResolvectl (Linux) ---

    void resolvectl_normalOutput()
    {
        const QString output =
            "Global\n"
            "       Protocols: -LLMNR -mDNS -DNSOverTLS DNSSEC=no/unsupported\n"
            "resolv.conf mode: stub\n"
            "\n"
            "Link 2 (enp0s3)\n"
            "    Current Scopes: DNS\n"
            "         Protocols: +DefaultRoute +LLMNR -mDNS -DNSOverTLS DNSSEC=no/unsupported\n"
            "Current DNS Server: 8.8.8.8\n"
            "       DNS Servers: 8.8.8.8 8.8.4.4\n";
        QStringList expected = {"8.8.8.8", "8.8.4.4"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvectl(output), expected);
    }

    void resolvectl_multipleInterfaces()
    {
        const QString output =
            "Link 2 (enp0s3)\n"
            "       DNS Servers: 8.8.8.8\n"
            "\n"
            "Link 3 (wlp2s0)\n"
            "       DNS Servers: 1.1.1.1\n";
        QStringList expected = {"8.8.8.8", "1.1.1.1"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvectl(output), expected);
    }

    void resolvectl_emptyOutput()
    {
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvectl(""), QStringList());
    }

    // --- parseDnsFromResolvConf (Linux) ---

    void resolvConf_standardFile()
    {
        const QString output =
            "# Generated by NetworkManager\n"
            "nameserver 8.8.8.8\n"
            "nameserver 8.8.4.4\n"
            "search home.lan\n";
        QStringList expected = {"8.8.8.8", "8.8.4.4"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvConf(output), expected);
    }

    void resolvConf_stubResolver()
    {
        const QString output =
            "# This is /run/systemd/resolve/stub-resolv.conf\n"
            "nameserver 127.0.0.53\n"
            "options edns0 trust-ad\n"
            "search .\n";
        QStringList expected = {"127.0.0.53"};
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvConf(output), expected);
    }

    void resolvConf_emptyFile()
    {
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvConf(""), QStringList());
    }

    void resolvConf_commentsOnly()
    {
        const QString output =
            "# This file is empty\n"
            "# nameserver 8.8.8.8\n";
        QCOMPARE(NetworkDiagWidget::parseDnsFromResolvConf(output), QStringList());
    }
};

QTEST_MAIN(TestNetworkDiag)
#include "test_network_diag.moc"
