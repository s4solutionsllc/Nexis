// SSO-23856: CleanerML parser & internal cleaner model.
//
// Exercises CleanerML::parseFile/parseDirectory against the real-world
// BleachBit fixtures in tests/fixtures/cleanerml/ (see PROVENANCE.md), plus
// targeted CleanerML::parseXml() snippets for edge cases the fixture corpus
// doesn't happen to cover (e.g. command="truncate", which no file in the
// current BleachBit v6.0.3 snapshot uses).

#include <QTest>
#include <QSet>

#include "Tools/cleanerml_parser.h"

using namespace CleanerML;

class TestCleanerMLParser : public QObject
{
    Q_OBJECT

private slots:
    // Real fixtures that use only in-scope commands and must fully parse.
    void parseThunderbird_succeedsWithDeleteWalkRegexVacuum();
    void parseBash_succeedsWithDeleteAndGlob();
    void parseGimp_succeedsWithWalk();
    void parseSqlite3_succeedsMinimal();
    void parseDeepscan_succeedsWithDeepRegexActions();
    void parseAdobeReader_succeedsAndSilentlySkipsWinreg();

    // Real fixtures that use out-of-scope commands and must fail cleanly.
    void parseFirefox_failsDueToUnsupportedCookieCommand();
    void parseApt_failsDueToUnsupportedAptCommand();
    void parseJournald_failsDueToUnsupportedCommand();
    void parseVlc_failsDueToUnsupportedIniCommand();

    // Malformed XML.
    void parseMalformed_reportsXmlSyntaxErrorNotCrash();

    // Directory aggregation across the whole corpus.
    void parseDirectory_aggregatesSuccessesAndErrors();

    // Edge cases not present in the current fixture corpus.
    void truncateAction_parsesAsTruncateType();
    void missingCleanerId_fails();
    void missingOptionId_fails();
    void actionMissingPath_fails();
    void deleteWithUnknownSearchValue_fails();
    void deepSearchWithoutRegex_fails();
    void actionOutsideOption_fails();
    void noCleanerRootElement_fails();
    void varAndRunningElements_areIgnored();
    void globAction_parsesAsGlobType();
};

static QString fixturePath(const QString &fileName)
{
    return QString(PROJECT_SOURCE_DIR) + "/tests/fixtures/cleanerml/" + fileName;
}

void TestCleanerMLParser::parseThunderbird_succeedsWithDeleteWalkRegexVacuum()
{
    ParseResult r = parseFile(fixturePath("thunderbird.xml"));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);

    const Cleaner &c = r.cleaners.first();
    QCOMPARE(c.id, QStringLiteral("thunderbird"));

    bool sawDelete = false, sawWalk = false, sawRegex = false, sawVacuum = false;
    for (const Option &opt : c.options) {
        for (const Action &a : opt.actions) {
            switch (a.type) {
            case ActionType::Delete: sawDelete = true; break;
            case ActionType::Walk: sawWalk = true; break;
            case ActionType::Regex: sawRegex = true; break;
            case ActionType::SqliteVacuum: sawVacuum = true; break;
            default: break;
            }
        }
    }

    QVERIFY(sawDelete);
    QVERIFY(sawWalk);
    QVERIFY(sawRegex);
    QVERIFY(sawVacuum);
}

void TestCleanerMLParser::parseBash_succeedsWithDeleteAndGlob()
{
    ParseResult r = parseFile(fixturePath("bash.xml"));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);

    const Cleaner &c = r.cleaners.first();
    QCOMPARE(c.id, QStringLiteral("bash"));
    QCOMPARE(c.os, QStringList{QStringLiteral("unix")});
    QCOMPARE(c.options.size(), 2);
    QCOMPARE(c.options[0].actions.first().type, ActionType::Delete);
    QCOMPARE(c.options[1].actions.first().type, ActionType::Glob);
}

void TestCleanerMLParser::parseGimp_succeedsWithWalk()
{
    ParseResult r = parseFile(fixturePath("gimp.xml"));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);

    const Cleaner &c = r.cleaners.first();
    QCOMPARE(c.options.size(), 1);
    QCOMPARE(c.options.first().actions.first().type, ActionType::Walk);
}

void TestCleanerMLParser::parseSqlite3_succeedsMinimal()
{
    ParseResult r = parseFile(fixturePath("sqlite3.xml"));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);
    QCOMPARE(r.cleaners.first().id, QStringLiteral("sqlite3"));
}

void TestCleanerMLParser::parseDeepscan_succeedsWithDeepRegexActions()
{
    ParseResult r = parseFile(fixturePath("deepscan.xml"));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);

    const Cleaner &c = r.cleaners.first();
    QVERIFY(c.options.size() >= 8);
    for (const Option &opt : c.options) {
        for (const Action &a : opt.actions) {
            QCOMPARE(a.type, ActionType::Regex);
            QVERIFY(!a.regex.isEmpty());
        }
    }
}

void TestCleanerMLParser::parseAdobeReader_succeedsAndSilentlySkipsWinreg()
{
    ParseResult r = parseFile(fixturePath("adobe_reader.xml"));
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);

    const Cleaner &c = r.cleaners.first();
    const Option *mru = nullptr;
    for (const Option &opt : c.options)
        if (opt.id == QStringLiteral("mru"))
            mru = &opt;
    QVERIFY(mru != nullptr);

    // The XML has 1 delete action + 7 winreg actions inside <option id="mru">;
    // all 7 winreg actions must be silently dropped, leaving just the delete.
    QCOMPARE(mru->actions.size(), 1);
    QCOMPARE(mru->actions.first().type, ActionType::Delete);
}

void TestCleanerMLParser::parseFirefox_failsDueToUnsupportedCookieCommand()
{
    ParseResult r = parseFile(fixturePath("firefox.xml"));
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QCOMPARE(r.errors.first().cleanerId, QStringLiteral("firefox"));
    QVERIFY(r.errors.first().message.contains(QStringLiteral("cookie")));
}

void TestCleanerMLParser::parseApt_failsDueToUnsupportedAptCommand()
{
    ParseResult r = parseFile(fixturePath("apt.xml"));
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QCOMPARE(r.errors.first().cleanerId, QStringLiteral("apt"));
    QVERIFY(r.errors.first().message.contains(QStringLiteral("apt.")));
}

void TestCleanerMLParser::parseJournald_failsDueToUnsupportedCommand()
{
    ParseResult r = parseFile(fixturePath("journald.xml"));
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("journald.clean")));
}

void TestCleanerMLParser::parseVlc_failsDueToUnsupportedIniCommand()
{
    ParseResult r = parseFile(fixturePath("vlc.xml"));
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("ini")));
}

void TestCleanerMLParser::parseMalformed_reportsXmlSyntaxErrorNotCrash()
{
    ParseResult r = parseFile(fixturePath("malformed.xml"));
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("XML parse error")));
}

void TestCleanerMLParser::parseDirectory_aggregatesSuccessesAndErrors()
{
    ParseResult r = parseDirectory(QString(PROJECT_SOURCE_DIR) + "/tests/fixtures/cleanerml");

    // 6 real fixtures parse cleanly; 4 real fixtures + malformed.xml fail.
    QCOMPARE(r.cleaners.size(), 6);
    QCOMPARE(r.errors.size(), 5);

    QSet<QString> cleanerIds;
    for (const Cleaner &c : r.cleaners)
        cleanerIds.insert(c.id);
    const QSet<QString> expected = {
        QStringLiteral("thunderbird"), QStringLiteral("bash"), QStringLiteral("gimp"),
        QStringLiteral("sqlite3"), QStringLiteral("deepscan"), QStringLiteral("adobe_reader")
    };
    QVERIFY(cleanerIds == expected);
}

void TestCleanerMLParser::truncateAction_parsesAsTruncateType()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="nexis_truncate_example">
  <label>Truncate example</label>
  <option id="log">
    <label>Log</label>
    <description>Truncate a log file instead of deleting it</description>
    <action command="truncate" path="~/.xsession-errors"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);
    QCOMPARE(r.cleaners.first().options.first().actions.first().type, ActionType::Truncate);
    QCOMPARE(r.cleaners.first().options.first().actions.first().path, QStringLiteral("~/.xsession-errors"));
}

void TestCleanerMLParser::missingCleanerId_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner>
  <label>No id</label>
  <option id="opt">
    <action command="delete" path="~/.example"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("id")));
}

void TestCleanerMLParser::missingOptionId_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test">
  <option>
    <action command="delete" path="~/.example"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QCOMPARE(r.errors.first().cleanerId, QStringLiteral("test"));
}

void TestCleanerMLParser::actionMissingPath_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test">
  <option id="opt">
    <action command="delete"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("path")));
}

void TestCleanerMLParser::deleteWithUnknownSearchValue_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test">
  <option id="opt">
    <action command="delete" search="bogus" path="~/.example"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("bogus")));
}

void TestCleanerMLParser::deepSearchWithoutRegex_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test">
  <option id="opt">
    <action command="delete" search="deep" path="/"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("deep")));
}

void TestCleanerMLParser::actionOutsideOption_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test">
  <action command="delete" path="~/.example"/>
  <option id="opt">
    <action command="delete" path="~/.example"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("outside")));
}

void TestCleanerMLParser::noCleanerRootElement_fails()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<notacleaner id="test"/>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.cleaners.isEmpty());
    QCOMPARE(r.errors.size(), 1);
    QVERIFY(r.errors.first().message.contains(QStringLiteral("<cleaner>")));
}

void TestCleanerMLParser::varAndRunningElements_areIgnored()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test" os="linux,macos">
  <label>Test</label>
  <running type="exe" os="linux" same_user="true">test</running>
  <var name="base">
    <value os="linux">~/.test</value>
  </var>
  <option id="opt">
    <label>Opt</label>
    <action command="delete" path="$$base$$/cache"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.size(), 1);
    QCOMPARE(r.cleaners.first().os, (QStringList{QStringLiteral("linux"), QStringLiteral("macos")}));
    QCOMPARE(r.cleaners.first().options.first().actions.first().path, QStringLiteral("$$base$$/cache"));
}

void TestCleanerMLParser::globAction_parsesAsGlobType()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="test">
  <option id="opt">
    <action command="delete" search="glob" path="~/.cache/*.tmp"/>
  </option>
</cleaner>)";

    ParseResult r = parseXml(xml);
    QVERIFY(r.errors.isEmpty());
    QCOMPARE(r.cleaners.first().options.first().actions.first().type, ActionType::Glob);
}

QTEST_MAIN(TestCleanerMLParser)
#include "test_cleanerml_parser.moc"
