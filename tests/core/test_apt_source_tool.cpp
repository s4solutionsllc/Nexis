#include <QTest>
#include <QFile>
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

    // New fields: format and signedByPath
    void listLine_setsLegacyFormat();
    void listLine_extractsSignedByPath();
    void listLine_noSignedBy_emptyPath();
    void deb822_setsDeb822Format();
    void deb822_extractsSignedByPath();
    void deb822_multiSuiteExpansion();

    // SSO-3728 / FW-01: Architectures, fixture round-trip, serialize/build
    void listLine_extractsArchitectures();
    void listLine_archCommaList_normalisedToSpaces();
    void deb822_extractsArchitectures();
    void fixture_ubuntu26_04_parsedCorrectly();
    void fixture_ubuntu26_04_noopRoundTripIsByteStable();
    void fixture_ubuntu26_04_legacyListStillParsed();
    void serialize_changingComponents_preservesUnknownFields();
    void serialize_removingStanza_returnsEmpty();
    void serialize_multiSuiteEditOnlyTouchesOneSuite();
    void build_freshStanza_includesAllProvidedFields();
    void build_freshStanza_omitsEmptyOptionalFields();
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
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    APTSourcePtr src = list.first();
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
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first()->isActive, false);
}

void TestAptSourceTool::deb822_debSrcStanza()
{
    QString stanza =
        "Types: deb deb-src\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first()->isSource, true);
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
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    APTSourcePtr src = list.first();
    QCOMPARE(src->uri, QString("http://example.com/apt"));
    QCOMPARE(src->suites, QString("stable"));
}

void TestAptSourceTool::deb822_emptyInput()
{
    auto list = AptSourceTool::parseDeb822Stanza("", "deb", "deb-src");
    QVERIFY(list.isEmpty());
}

void TestAptSourceTool::deb822_wrongType()
{
    QString stanza =
        "Types: rpm\n"
        "URIs: http://example.com/apt\n"
        "Suites: stable\n"
        "Components: main\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QVERIFY(list.isEmpty());
}

void TestAptSourceTool::deb822_noComponentsField()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://example.com/apt\n"
        "Suites: jammy\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    APTSourcePtr src = list.first();
    QCOMPARE(src->components, QString(""));
    QCOMPARE(src->uri, QString("http://example.com/apt"));
}

// --- format and signedByPath ---

void TestAptSourceTool::listLine_setsLegacyFormat()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb http://archive.ubuntu.com/ubuntu jammy main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->format, APTSource::Legacy);
}

void TestAptSourceTool::listLine_extractsSignedByPath()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64 signed-by=/usr/share/keyrings/example.gpg] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->signedByPath, QString("/usr/share/keyrings/example.gpg"));
}

void TestAptSourceTool::listLine_noSignedBy_emptyPath()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->signedByPath, QString());
}

void TestAptSourceTool::deb822_setsDeb822Format()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first()->format, APTSource::Deb822);
}

void TestAptSourceTool::deb822_extractsSignedByPath()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu\n"
        "Suites: jammy\n"
        "Components: main\n"
        "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first()->signedByPath, QString("/usr/share/keyrings/ubuntu-archive-keyring.gpg"));
}

void TestAptSourceTool::deb822_multiSuiteExpansion()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://us.archive.ubuntu.com/ubuntu/\n"
        "Suites: noble noble-updates noble-backports\n"
        "Components: main restricted universe multiverse\n"
        "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 3);

    QCOMPARE(list[0]->suites, QString("noble"));
    QCOMPARE(list[1]->suites, QString("noble-updates"));
    QCOMPARE(list[2]->suites, QString("noble-backports"));

    // All share the same URI, components, signedBy, format
    for (const APTSourcePtr &src : list) {
        QCOMPARE(src->uri, QString("http://us.archive.ubuntu.com/ubuntu/"));
        QCOMPARE(src->components, QString("main restricted universe multiverse"));
        QCOMPARE(src->signedByPath, QString("/usr/share/keyrings/ubuntu-archive-keyring.gpg"));
        QCOMPARE(src->format, APTSource::Deb822);
    }
}

// --- SSO-3728 / FW-01: Architectures, fixture round-trip, serialize ---

static QString readFixture(const QString &relPath)
{
    QString full = QString(PROJECT_SOURCE_DIR) + "/" + relPath;
    QFile f(full);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

void TestAptSourceTool::listLine_extractsArchitectures()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64 signed-by=/etc/apt/keyrings/example.asc] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->architectures, QString("amd64"));
}

void TestAptSourceTool::listLine_archCommaList_normalisedToSpaces()
{
    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb [arch=amd64,arm64 signed-by=/etc/apt/keyrings/example.asc] https://repo.example.com/apt stable main",
        "deb", "deb-src");
    QVERIFY(src);
    QCOMPARE(src->architectures, QString("amd64 arm64"));
}

void TestAptSourceTool::deb822_extractsArchitectures()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu/\n"
        "Suites: noble\n"
        "Components: main\n"
        "Architectures: amd64 arm64\n"
        "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n";
    auto list = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first()->architectures, QString("amd64 arm64"));
}

void TestAptSourceTool::fixture_ubuntu26_04_parsedCorrectly()
{
    QString text = readFixture("tests/fixtures/apt/ubuntu_26_04.sources");
    QVERIFY(!text.isEmpty());

    // Split into stanzas (blank-line separated), skipping leading comments
    QStringList stanzas;
    QString cur;
    for (const QString &line : text.split('\n')) {
        if (line.trimmed().isEmpty()) {
            if (!cur.trimmed().isEmpty())
                stanzas << cur;
            cur.clear();
        } else {
            cur += line + '\n';
        }
    }
    if (!cur.trimmed().isEmpty())
        stanzas << cur;

    QCOMPARE(stanzas.size(), 2);

    auto archive = AptSourceTool::parseDeb822Stanza(stanzas[0], "deb", "deb-src");
    QCOMPARE(archive.size(), 3); // noble, noble-updates, noble-backports
    QCOMPARE(archive[0]->uri, QString("http://archive.ubuntu.com/ubuntu/"));
    QCOMPARE(archive[0]->suites, QString("noble"));
    QCOMPARE(archive[1]->suites, QString("noble-updates"));
    QCOMPARE(archive[2]->suites, QString("noble-backports"));
    for (const APTSourcePtr &s : archive) {
        QCOMPARE(s->components, QString("main restricted universe multiverse"));
        QCOMPARE(s->architectures, QString("amd64"));
        QCOMPARE(s->signedByPath, QString("/usr/share/keyrings/ubuntu-archive-keyring.gpg"));
        QCOMPARE(s->format, APTSource::Deb822);
        QCOMPARE(s->isActive, true);
        QCOMPARE(s->isSource, false);
    }

    auto security = AptSourceTool::parseDeb822Stanza(stanzas[1], "deb", "deb-src");
    QCOMPARE(security.size(), 1);
    QCOMPARE(security[0]->uri, QString("http://security.ubuntu.com/ubuntu/"));
    QCOMPARE(security[0]->suites, QString("noble-security"));
    QCOMPARE(security[0]->architectures, QString("amd64"));
    QCOMPARE(security[0]->signedByPath, QString("/usr/share/keyrings/ubuntu-archive-keyring.gpg"));
}

void TestAptSourceTool::fixture_ubuntu26_04_noopRoundTripIsByteStable()
{
    QString text = readFixture("tests/fixtures/apt/ubuntu_26_04.sources");
    QVERIFY(!text.isEmpty());

    // Split into (leading-comment chunk, stanzas...) preserving the blank
    // line as the separator. We treat any chunk that doesn't contain a
    // "Types:" line as pass-through (comments-only header).
    QStringList chunks;
    QString cur;
    for (const QString &line : text.split('\n')) {
        if (line.trimmed().isEmpty()) {
            if (!cur.isEmpty())
                chunks << cur;
            cur.clear();
        } else {
            cur += line + '\n';
        }
    }
    if (!cur.isEmpty())
        chunks << cur;

    // Find the first stanza (the comments-only chunk) and the two real
    // stanzas. Re-serialize each real stanza with matchEntry=null and
    // newSource=clone-of-parsed, which should be a byte-stable no-op.
    QStringList rebuilt;
    for (const QString &chunk : chunks) {
        if (!chunk.contains("Types:")) {
            // Header comment chunk — preserved verbatim (minus the trailing \n
            // we'll re-add via join).
            QString trimmed = chunk;
            while (trimmed.endsWith('\n'))
                trimmed.chop(1);
            rebuilt << trimmed;
            continue;
        }
        auto parsed = AptSourceTool::parseDeb822Stanza(chunk, "deb", "deb-src");
        QVERIFY(!parsed.isEmpty());
        // Clone first entry as the "new" entry — fields unchanged. The
        // serializer matches on URI+suite which exists in the original.
        APTSourcePtr first = parsed.first();
        APTSourcePtr clone(new APTSource(*first));
        QString out = AptSourceTool::serializeDeb822Stanza(
            chunk, first, clone, "deb", "deb-src");
        rebuilt << out;
    }

    QString reassembled = rebuilt.join("\n\n") + "\n";
    QCOMPARE(reassembled, text);
}

void TestAptSourceTool::fixture_ubuntu26_04_legacyListStillParsed()
{
    QString text = readFixture("tests/fixtures/apt/sources.list");
    QVERIFY(!text.isEmpty());

    int activeCount = 0;
    int commentedCount = 0;
    for (const QString &line : text.split('\n')) {
        APTSourcePtr s = AptSourceTool::parseSourceListLine(line, "deb", "deb-src");
        if (!s)
            continue;
        QCOMPARE(s->format, APTSource::Legacy);
        if (s->isActive) ++activeCount; else ++commentedCount;
    }
    QCOMPARE(activeCount, 6);
    QCOMPARE(commentedCount, 1);
}

void TestAptSourceTool::serialize_changingComponents_preservesUnknownFields()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu/\n"
        "Suites: noble\n"
        "Components: main\n"
        "Architectures: amd64\n"
        "Languages: en\n"
        "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n";

    auto parsed = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(parsed.size(), 1);
    APTSourcePtr first = parsed.first();
    APTSourcePtr updated(new APTSource(*first));
    updated->components = "main universe";

    QString out = AptSourceTool::serializeDeb822Stanza(
        stanza, first, updated, "deb", "deb-src");

    // Components updated
    QVERIFY(out.contains("Components: main universe"));
    // Unknown Languages field preserved untouched
    QVERIFY(out.contains("Languages: en"));
    // Signed-By kept verbatim
    QVERIFY(out.contains("Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg"));
    // Architectures unchanged
    QVERIFY(out.contains("Architectures: amd64"));
}

void TestAptSourceTool::serialize_removingStanza_returnsEmpty()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu/\n"
        "Suites: noble\n"
        "Components: main\n";

    auto parsed = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(parsed.size(), 1);
    QString out = AptSourceTool::serializeDeb822Stanza(
        stanza, parsed.first(), APTSourcePtr(), "deb", "deb-src");
    QCOMPARE(out, QString());
}

void TestAptSourceTool::serialize_multiSuiteEditOnlyTouchesOneSuite()
{
    QString stanza =
        "Types: deb\n"
        "URIs: http://archive.ubuntu.com/ubuntu/\n"
        "Suites: noble noble-updates noble-backports\n"
        "Components: main\n"
        "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n";

    auto parsed = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(parsed.size(), 3);
    APTSourcePtr match = parsed[1]; // noble-updates
    APTSourcePtr updated(new APTSource(*match));
    updated->suites = "noble-proposed"; // rename just this one

    QString out = AptSourceTool::serializeDeb822Stanza(
        stanza, match, updated, "deb", "deb-src");

    QVERIFY(out.contains("Suites: noble noble-proposed noble-backports"));
    QVERIFY(!out.contains("noble-updates"));
}

void TestAptSourceTool::build_freshStanza_includesAllProvidedFields()
{
    APTSourcePtr s(new APTSource);
    s->isSource = false;
    s->isActive = true;
    s->uri = "https://download.docker.com/linux/ubuntu";
    s->suites = "noble";
    s->components = "stable";
    s->architectures = "amd64";
    s->signedByPath = "/etc/apt/keyrings/docker.asc";

    QString out = AptSourceTool::buildDeb822Stanza(s, "deb", "deb-src");

    QCOMPARE(out,
        QStringLiteral(
            "Types: deb\n"
            "URIs: https://download.docker.com/linux/ubuntu\n"
            "Suites: noble\n"
            "Components: stable\n"
            "Architectures: amd64\n"
            "Signed-By: /etc/apt/keyrings/docker.asc\n"));
}

void TestAptSourceTool::build_freshStanza_omitsEmptyOptionalFields()
{
    APTSourcePtr s(new APTSource);
    s->isSource = false;
    s->isActive = true;
    s->uri = "http://ppa.example.com/ubuntu";
    s->suites = "noble";

    QString out = AptSourceTool::buildDeb822Stanza(s, "deb", "deb-src");

    QVERIFY(!out.contains("Components:"));
    QVERIFY(!out.contains("Architectures:"));
    QVERIFY(!out.contains("Signed-By:"));
    QVERIFY(!out.contains("Enabled:"));
    QCOMPARE(out,
        QStringLiteral(
            "Types: deb\n"
            "URIs: http://ppa.example.com/ubuntu\n"
            "Suites: noble\n"));
}

QTEST_MAIN(TestAptSourceTool)
#include "test_apt_source_tool.moc"
