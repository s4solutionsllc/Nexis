#include <QtTest>
#include <QDir>

#include <Tools/cleanerml_parser.h>
#include <Tools/cleanerml_platform.h>

// SSO-25782: lenient parsing (vars, os=, running, unsupported actions kept)
// and CleanerML::forPlatform(), which decides what a cleaner can really do on
// one platform. Also the guard that every shipped cleaner definition is at
// least well-formed.

using namespace CleanerML;

class TestCleanerMLPlatform : public QObject
{
    Q_OBJECT

private:
    static Cleaner parseOne(const QByteArray &xml)
    {
        const ParseResult r = parseXml(xml, QStringLiteral("inline"), ParseMode::Lenient);
        if (!r.errors.isEmpty() || r.cleaners.size() != 1)
            return Cleaner();
        return r.cleaners.first();
    }

    static QByteArray sampleXml()
    {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="sample">
  <label>Sample</label>
  <running type="exe" os="windows">sample.exe</running>
  <running type="exe" os="linux">sample</running>
  <var name="Profile">
    <value os="windows">%AppData%\Sample</value>
    <value os="linux">$XDG_CONFIG_HOME/sample</value>
    <value os="macos">~/Library/Application Support/Sample</value>
    <value search="glob" os="linux">~/.sample/Profile *</value>
  </var>
  <var name="winonly">
    <value os="windows">%LocalAppData%\Sample</value>
  </var>
  <option id="cache">
    <label>Cache</label>
    <action command="delete" search="walk.files" path="$$Profile$$/cache"/>
    <action command="delete" search="file" path="%Temp%\sample.log"/>
    <action command="delete" search="file" path="$$winonly$$\x.dat"/>
    <action command="delete" search="file" os="windows" path="C:\sample.tmp"/>
  </option>
  <option id="mru">
    <label>Recent</label>
    <action command="ini" search="file" path="$$Profile$$/sample.conf" section="Recent"/>
    <action command="delete" search="file" path="$$Profile$$/recent"/>
  </option>
  <option id="winmru">
    <label>Windows-only exotic action</label>
    <action command="delete" search="file" path="~/.sample/history"/>
    <action command="json" os="windows" search="file" path="%AppData%\Sample\prefs.json"/>
  </option>
  <option id="deep">
    <label>Deep</label>
    <action command="delete" search="deep" regex="\.sample-tmp$"/>
  </option>
  <option id="macthing" os="macos">
    <label>Mac only</label>
    <action command="delete" search="file" path="~/Library/Sample/x"/>
  </option>
</cleaner>)";
    }

private slots:
    void osMatches_rules()
    {
        QVERIFY(osMatches({}, "linux"));
        QVERIFY(osMatches({"linux"}, "linux"));
        QVERIFY(!osMatches({"windows"}, "linux"));
        QVERIFY(osMatches({"unix"}, "linux"));
        QVERIFY(osMatches({"unix"}, "macos"));
        QVERIFY(osMatches({"darwin"}, "macos"));
        QVERIFY(!osMatches({"freebsd", "openbsd"}, "linux"));
        QVERIFY(osMatches({"windows", " Linux "}, "linux"));
    }

    void lenient_keepsUnsupportedActions_strictStillFailsTheCleaner()
    {
        const ParseResult strict = parseXml(sampleXml());
        QVERIFY(strict.cleaners.isEmpty());
        QCOMPARE(strict.errors.size(), 1);

        const Cleaner c = parseOne(sampleXml());
        QCOMPARE(c.id, QStringLiteral("sample"));
        QCOMPARE(c.options.size(), 5);
        QCOMPARE(c.options.at(1).actions.first().type, ActionType::Unsupported);
        QCOMPARE(c.vars.value("profile").size(), 4);
        QVERIFY(c.vars.value("profile").at(3).glob);
        QCOMPARE(c.running.size(), 2);
        QCOMPARE(c.options.at(4).os, QStringList{"macos"});
    }

    void forPlatform_linux()
    {
        const Cleaner c = forPlatform(parseOne(sampleXml()), "linux");

        // Windows values and the Windows-only var are gone.
        QCOMPARE(c.vars.value("profile").size(), 2);
        QVERIFY(!c.vars.contains("winonly"));
        QCOMPARE(c.running.size(), 1);
        QCOMPARE(c.running.first().value, QStringLiteral("sample"));

        QStringList ids;
        for (const Option &o : c.options)
            ids << o.id;
        // "mru" still needs an ini edit on Linux, "deep" walks the whole home,
        // "macthing" is for macOS: all dropped. "winmru"'s exotic action is
        // Windows-only, so the option survives.
        QCOMPARE(ids, (QStringList{"cache", "winmru"}));

        // Of "cache"'s four actions only the one with a Linux path remains.
        QCOMPARE(c.options.first().actions.size(), 1);
        QCOMPARE(c.options.first().actions.first().path, QStringLiteral("$$Profile$$/cache"));
    }

    void forPlatform_macos()
    {
        const Cleaner c = forPlatform(parseOne(sampleXml()), "macos");
        QCOMPARE(c.vars.value("profile").size(), 1);
        QVERIFY(c.running.isEmpty());

        QStringList ids;
        for (const Option &o : c.options)
            ids << o.id;
        QCOMPARE(ids, (QStringList{"cache", "winmru", "macthing"}));
    }

    void forPlatform_cleanerForAnotherOs_hasNoOptions()
    {
        const Cleaner c = parseOne(R"(<cleaner id="w" os="windows"><label>W</label>
            <option id="o"><label>O</label><action command="delete" search="file" path="~/x"/></option>
            </cleaner>)");
        QCOMPARE(c.options.size(), 1);
        QVERIFY(forPlatform(c, "linux").options.isEmpty());
    }

    void pathIsForeign_rules()
    {
        QHash<QString, QList<VarValue>> vars;
        vars.insert("profile", {VarValue{"~/.x", {}, false}});

        QVERIFY(!pathIsForeign("~/.cache/x", vars));
        QVERIFY(!pathIsForeign("$$home$$/x", vars));
        QVERIFY(!pathIsForeign("$$Profile$$/x", vars));
        QVERIFY(pathIsForeign("$$base$$/x", vars));
        QVERIFY(pathIsForeign("%Temp%\\x", vars));
        QVERIFY(pathIsForeign("HKCU\\Software\\X", vars));
    }

    // Every definition Nexis ships must at least be well-formed XML with a
    // cleaner id; how much of it is usable is decided per platform.
    void bundledCorpus_parsesLenientlyWithoutErrors()
    {
        const QString dir = QString(PROJECT_SOURCE_DIR) + "/shared/nexis/cleaners.d";
        const ParseResult r = parseDirectory(dir, ParseMode::Lenient);

        QStringList messages;
        for (const ParseError &e : r.errors)
            messages << e.source + ": " + e.message;
        QVERIFY2(r.errors.isEmpty(), qPrintable(messages.join('\n')));
        QCOMPARE(r.cleaners.size(), QDir(dir).entryList({"*.xml"}, QDir::Files).size());

        for (const char *os : {"linux", "macos"}) {
            int cleaners = 0, options = 0;
            for (const Cleaner &c : r.cleaners) {
                const Cleaner p = forPlatform(c, os);
                if (!p.options.isEmpty())
                    cleaners++;
                options += p.options.size();
            }
            qInfo() << os << "usable cleaners" << cleaners << "options" << options;
            QVERIFY(cleaners > 0);
        }
    }
};

QTEST_GUILESS_MAIN(TestCleanerMLPlatform)
#include "test_cleanerml_platform.moc"
