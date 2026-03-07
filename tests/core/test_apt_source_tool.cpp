#include <QTest>
#include "Tools/apt_source_tool.h"

class TestAptSourceTool : public QObject
{
    Q_OBJECT

private slots:
    // parseSourceListLine
    void listLine_activeDeb();
    void listLine_commentedDeb();
    void listLine_debSrc();
    void listLine_withOptions();
    void listLine_minimalFields();
    void listLine_emptyInput();
    void listLine_wrongType();

    // parseDeb822Stanza
    void deb822_standardStanza();
    void deb822_disabledStanza();
    void deb822_debSrcStanza();
    void deb822_commentedLines();
    void deb822_emptyInput();
    void deb822_wrongType();
    void deb822_noComponentsField();
};

// --- parseSourceListLine ---

void TestAptSourceTool::listLine_activeDeb()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb http://archive.ubuntu.com/ubuntu jammy main restricted",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isActive, true);
    QCOMPARE(src->isSource, false);
    QCOMPARE(src->uri, QString("http://archive.ubuntu.com/ubuntu"));
    QCOMPARE(src->suites, QString("jammy"));
    QCOMPARE(src->components, QString("main restricted"));
}

void TestAptSourceTool::listLine_commentedDeb()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "# deb http://archive.ubuntu.com/ubuntu jammy main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isActive, false);
    QCOMPARE(src->uri, QString("http://archive.ubuntu.com/ubuntu"));
    QCOMPARE(src->suites, QString("jammy"));
}

void TestAptSourceTool::listLine_debSrc()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb-src http://archive.ubuntu.com/ubuntu jammy main universe",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isSource, true);
    QCOMPARE(src->uri, QString("http://archive.ubuntu.com/ubuntu"));
}

void TestAptSourceTool::listLine_withOptions()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64 signed-by=/usr/share/keyrings/example.gpg] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isActive, true);
    QCOMPARE(src->options, QString("[arch=amd64 signed-by=/usr/share/keyrings/example.gpg]"));
    QCOMPARE(src->uri, QString("https://repo.example.com/apt"));
    QCOMPARE(src->suites, QString("stable"));
    QCOMPARE(src->components, QString("main"));
}

void TestAptSourceTool::listLine_minimalFields()
{
    // Only type, uri, suite — no components
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb http://ppa.example.com/ubuntu jammy",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->uri, QString("http://ppa.example.com/ubuntu"));
    QCOMPARE(src->suites, QString("jammy"));
    QCOMPARE(src->components, QString(""));
}

void TestAptSourceTool::listLine_emptyInput()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine("", "deb", "deb-src");
    QVERIFY(!src);
}

void TestAptSourceTool::listLine_wrongType()
{
    // rpm type when we're looking for deb
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "rpm http://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(!src);
}

// --- parseDeb822Stanza ---

void TestAptSourceTool::deb822_standardStanza()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main restricted universe\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isActive, true);
    QCOMPARE(src->isSource, false);
    QCOMPARE(src->uri, QString("http://archive.ubuntu.com/ubuntu"));
    QCOMPARE(src->suites, QString("jammy"));
    QCOMPARE(src->components, QString("main restricted universe"));
}

void TestAptSourceTool::deb822_disabledStanza()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n"
        "Enabled: no\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isActive, false);
}

void TestAptSourceTool::deb822_debSrcStanza()
{
    QString stanza =
        "Types: deb deb-src\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->isSource, true);
}

void TestAptSourceTool::deb822_commentedLines()
{
    QString stanza =
        "# This is a comment\n"
        "Types: deb\n"
        "URIs: http://example.com/apt\n"
        "# Another comment\n"
        "Suites: stable\n"
        "Components: main\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->uri, QString("http://example.com/apt"));
    QCOMPARE(src->suites, QString("stable"));
}

void TestAptSourceTool::deb822_emptyInput()
{
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza("", "deb", "deb-src");
    QVERIFY(!src);
}

void TestAptSourceTool::deb822_wrongType()
{
    QString stanza =
        "Types: rpm\n"
        "URIs: http://example.com/apt\n"
        "Suites: stable\n"
        "Components: main\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(!src);
}

void TestAptSourceTool::deb822_noComponentsField()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://example.com/apt\n"
        "Suites: jammy\n";
    APTSourcePtr src = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->components, QString(""));
    QCOMPARE(src->uri, QString("http://example.com/apt"));
}

QTEST_MAIN(TestAptSourceTool)
#include "test_apt_source_tool.moc"
