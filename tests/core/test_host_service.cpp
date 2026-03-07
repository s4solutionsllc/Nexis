#include <QTest>
#include "Services/host_service.h"

class TestHostService : public QObject
{
    Q_OBJECT

private slots:
    // isValidIP
    void validIP_ipv4();
    void validIP_ipv6();
    void validIP_ipv6Loopback();
    void validIP_invalidOctets();
    void validIP_empty();
    void validIP_hostname();

    // isValidHostname
    void hostname_simple();
    void hostname_fqdn();
    void hostname_singleChar();
    void hostname_tooLong();
    void hostname_empty();
    void hostname_invalidChars();
    void hostname_leadingHyphen();
    void hostname_trailingHyphen();
    void hostname_emptyLabel();

    // parseHostEntries
    void parse_basicFile();
    void parse_tabSeparated();
    void parse_commentLines();
    void parse_emptyFile();
    void parse_mixedContent();
    void parse_lineWithAliases();
    void parse_lineIndices();
    void parse_inlineComments();
};

void TestHostService::validIP_ipv4()
{
    QVERIFY(HostService::isValidIP("192.168.1.1"));
    QVERIFY(HostService::isValidIP("0.0.0.0"));
    QVERIFY(HostService::isValidIP("255.255.255.255"));
    QVERIFY(HostService::isValidIP("127.0.0.1"));
}

void TestHostService::validIP_ipv6()
{
    QVERIFY(HostService::isValidIP("fe80::1"));
    QVERIFY(HostService::isValidIP("2001:db8::1"));
}

void TestHostService::validIP_ipv6Loopback()
{
    QVERIFY(HostService::isValidIP("::1"));
}

void TestHostService::validIP_invalidOctets()
{
    QVERIFY(!HostService::isValidIP("999.999.999.999"));
    QVERIFY(!HostService::isValidIP("256.0.0.1"));
}

void TestHostService::validIP_empty()
{
    QVERIFY(!HostService::isValidIP(""));
}

void TestHostService::validIP_hostname()
{
    QVERIFY(!HostService::isValidIP("localhost"));
    QVERIFY(!HostService::isValidIP("example.com"));
}

void TestHostService::hostname_simple()
{
    QVERIFY(HostService::isValidHostname("localhost"));
    QVERIFY(HostService::isValidHostname("myhost"));
}

void TestHostService::hostname_fqdn()
{
    QVERIFY(HostService::isValidHostname("host.example.com"));
    QVERIFY(HostService::isValidHostname("sub.domain.example.org"));
}

void TestHostService::hostname_singleChar()
{
    QVERIFY(HostService::isValidHostname("a"));
    QVERIFY(HostService::isValidHostname("1"));
}

void TestHostService::hostname_tooLong()
{
    // 254 chars exceeds limit of 253
    QString longHost = QString("a").repeated(254);
    QVERIFY(!HostService::isValidHostname(longHost));
}

void TestHostService::hostname_empty()
{
    QVERIFY(!HostService::isValidHostname(""));
}

void TestHostService::hostname_invalidChars()
{
    QVERIFY(!HostService::isValidHostname("host name"));
    QVERIFY(!HostService::isValidHostname("host!name"));
}

void TestHostService::hostname_leadingHyphen()
{
    QVERIFY(!HostService::isValidHostname("-hostname"));
}

void TestHostService::hostname_trailingHyphen()
{
    QVERIFY(!HostService::isValidHostname("hostname-"));
}

void TestHostService::hostname_emptyLabel()
{
    QVERIFY(!HostService::isValidHostname("host..com"));
}

void TestHostService::parse_basicFile()
{
    QStringList content = {
        "127.0.0.1   localhost",
        "::1         localhost",
        "192.168.1.100  myhost.local  myhost"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 3);
}

void TestHostService::parse_tabSeparated()
{
    QStringList content = {
        "127.0.0.1\tlocalhost"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].ip, QString("127.0.0.1"));
    QCOMPARE(entries[0].fullQualified, QString("localhost"));
}

void TestHostService::parse_commentLines()
{
    QStringList content = {
        "# This is a comment",
        "127.0.0.1 localhost",
        "# Another comment",
        "  # Indented comment"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[1].ip, QString("127.0.0.1"));
}

void TestHostService::parse_emptyFile()
{
    QStringList content;
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 0);
}

void TestHostService::parse_mixedContent()
{
    QStringList content = {
        "# /etc/hosts",
        "",
        "127.0.0.1   localhost",
        "",
        "# Custom entries",
        "10.0.0.1    server1.local  server1",
        "10.0.0.2    server2.local"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 3);
}

void TestHostService::parse_lineWithAliases()
{
    QStringList content = {
        "192.168.1.100  myhost.example.com  myhost  alias1  alias2"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].ip, QString("192.168.1.100"));
    QCOMPARE(entries[0].fullQualified, QString("myhost.example.com"));
    QCOMPARE(entries[0].aliases, QString("myhost alias1 alias2"));
}

void TestHostService::parse_lineIndices()
{
    // Verify that the map key is the original line index, not a sequential counter
    QStringList content = {
        "# Comment on line 0",
        "127.0.0.1 localhost",
        "# Comment on line 2",
        "10.0.0.1 server"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 2);
    // Line 0 is a comment, so key 0 should not exist
    QVERIFY(!entries.contains(0));
    // Line 1 should be the first entry
    QVERIFY(entries.contains(1));
    QCOMPARE(entries[1].ip, QString("127.0.0.1"));
    // Line 3 should be the second entry
    QVERIFY(entries.contains(3));
    QCOMPARE(entries[3].ip, QString("10.0.0.1"));
}

void TestHostService::parse_inlineComments()
{
    QStringList content = {
        "127.0.0.1 localhost # loopback",
        "192.168.1.1  myhost.local  myhost # office server",
        "10.0.0.1 onlycomment #nothing"
    };
    QMap<int, HostEntry> entries = HostService::parseHostEntries(content);
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].ip, QString("127.0.0.1"));
    QCOMPARE(entries[0].fullQualified, QString("localhost"));
    QCOMPARE(entries[0].aliases, QString(""));
    QCOMPARE(entries[1].aliases, QString("myhost"));
    QCOMPARE(entries[2].fullQualified, QString("onlycomment"));
    QCOMPARE(entries[2].aliases, QString(""));
}

QTEST_MAIN(TestHostService)
#include "test_host_service.moc"
