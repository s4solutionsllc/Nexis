#include <QTest>

#include "network_info_linux.h"

// SSO-351: regression coverage for the IPv4-default-route parser used by
// NetworkInfoLinux to choose which interface's bytes feed the live-rate
// displays. Wi-Fi (wlp*) used to be invisible because the constructor cached
// "first non-loopback up+running iface" exactly once, silently shadowing the
// real default behind docker0 / virbr0 / etc. The /proc/net/route lookup
// fixes that — these tests pin the parser's contract.
class TestNetworkInfo : public QObject
{
    Q_OBJECT

private slots:
    void parsesWifiDefaultRoute();
    void parsesEthernetDefaultRoute();
    void ignoresNonDefaultRows();
    void returnsEmptyWhenNoDefault();
    void returnsEmptyOnEmptyInput();
    void handlesMissingTrailingNewline();
};

void TestNetworkInfo::parsesWifiDefaultRoute()
{
    // Real-world /proc/net/route on a Wi-Fi-only host (the SSO-351 reporter's
    // configuration). Columns are tab-separated.
    const QByteArray content =
        "Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\tMTU\tWindow\tIRTT\n"
        "wlp0s20f3\t00000000\t0101A8C0\t0003\t0\t0\t600\t00000000\t0\t0\t0\n"
        "wlp0s20f3\t0001A8C0\t00000000\t0001\t0\t0\t600\t00FFFFFF\t0\t0\t0\n";

    QCOMPARE(NetworkInfoLinux::parseDefaultRouteIface(content),
             QString("wlp0s20f3"));
}

void TestNetworkInfo::parsesEthernetDefaultRoute()
{
    const QByteArray content =
        "Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\n"
        "docker0\t000011AC\t00000000\t0001\t0\t0\t0\t0000FFFF\n"
        "enp3s0\t00000000\t0101A8C0\t0003\t0\t0\t100\t00000000\n";

    QCOMPARE(NetworkInfoLinux::parseDefaultRouteIface(content),
             QString("enp3s0"));
}

void TestNetworkInfo::ignoresNonDefaultRows()
{
    // docker0 has a /16 route but is NOT the default — selecting it was the
    // pre-fix bug. Make sure the parser walks past it.
    const QByteArray content =
        "Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\n"
        "docker0\t000011AC\t00000000\t0001\t0\t0\t0\t0000FFFF\n"
        "virbr0\t007BA8C0\t00000000\t0001\t0\t0\t0\t00FFFFFF\n"
        "wlp2s0\t00000000\t0101A8C0\t0003\t0\t0\t600\t00000000\n";

    QCOMPARE(NetworkInfoLinux::parseDefaultRouteIface(content),
             QString("wlp2s0"));
}

void TestNetworkInfo::returnsEmptyWhenNoDefault()
{
    // No row with Destination 00000000 — host has interfaces but no default
    // route (e.g. boot before DHCP).
    const QByteArray content =
        "Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\n"
        "docker0\t000011AC\t00000000\t0001\t0\t0\t0\t0000FFFF\n";

    QCOMPARE(NetworkInfoLinux::parseDefaultRouteIface(content),
             QString());
}

void TestNetworkInfo::returnsEmptyOnEmptyInput()
{
    QCOMPARE(NetworkInfoLinux::parseDefaultRouteIface(QByteArray()),
             QString());
}

void TestNetworkInfo::handlesMissingTrailingNewline()
{
    const QByteArray content =
        "Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\n"
        "eth0\t00000000\t0101A8C0\t0003\t0\t0\t100\t00000000";

    QCOMPARE(NetworkInfoLinux::parseDefaultRouteIface(content),
             QString("eth0"));
}

QTEST_MAIN(TestNetworkInfo)
#include "test_network_info.moc"
