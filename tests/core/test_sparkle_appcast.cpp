#include <QTest>
#include "Info/sparkle_appcast_parser.h"

using namespace SparkleAppcastParser;

class TestSparkleAppcast : public QObject
{
    Q_OBJECT

private slots:
    void emptyData_notOk();
    void oversizedData_rejected();
    void malformedXml_noItems_ok();
    void validFeed_parsesVersion();
    void validFeed_parsesEdSignature();
    void validFeed_parsesDsaSignature_whenNoEd();
    void validFeed_enclosureUrlParsed();
    void validFeed_multipleItems_latestPicked();
    void noSignature_signaturePresentFalse();
    void missingUrl_itemExcluded();
    void parseError_partialResultOk();
};

static const char *kMinimalFeed = R"xml(
<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0"
     xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <title>MyApp</title>
    <item>
      <title>MyApp 2.0</title>
      <enclosure
        url="https://example.com/MyApp-2.0.dmg"
        sparkle:version="2.0"
        sparkle:edSignature="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        length="1234567" />
    </item>
  </channel>
</rss>
)xml";

static const char *kLegacyDsaFeed = R"xml(
<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0"
     xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <item>
      <enclosure
        url="https://example.com/LegacyApp-1.5.zip"
        sparkle:version="1.5"
        sparkle:dsaSignature="MCwCFDSASIGNATUREBASE64ABCDEFGHIJKL"
        length="500000" />
    </item>
  </channel>
</rss>
)xml";

static const char *kMultiItemFeed = R"xml(
<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0"
     xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <item>
      <enclosure url="https://example.com/App-1.0.dmg"
                 sparkle:version="1.0"
                 sparkle:edSignature="AAAAAAAAA" length="100" />
    </item>
    <item>
      <enclosure url="https://example.com/App-3.0.dmg"
                 sparkle:version="3.0"
                 sparkle:edSignature="BBBBBBBBB" length="200" />
    </item>
    <item>
      <enclosure url="https://example.com/App-2.1.dmg"
                 sparkle:version="2.1"
                 sparkle:edSignature="CCCCCCCCC" length="150" />
    </item>
  </channel>
</rss>
)xml";

static const char *kNoSignatureFeed = R"xml(
<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0"
     xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <item>
      <enclosure url="https://example.com/UnsignedApp-1.0.dmg"
                 sparkle:version="1.0"
                 length="999" />
    </item>
  </channel>
</rss>
)xml";

void TestSparkleAppcast::emptyData_notOk()
{
    const SparkleAppcastResult r = parse(QByteArray{});
    QVERIFY(!r.ok);
}

void TestSparkleAppcast::oversizedData_rejected()
{
    QByteArray big(static_cast<int>(kMaxFeedBytes) + 1, 'x');
    const SparkleAppcastResult r = parse(big);
    QVERIFY(!r.ok);
    QVERIFY(r.errorMessage.contains("size"));
}

void TestSparkleAppcast::malformedXml_noItems_ok()
{
    // Completely invalid XML; result.ok = true (graceful) but no enclosures.
    const SparkleAppcastResult r = parse(QByteArray("<not valid xml>>>"));
    QVERIFY(r.ok);
    QVERIFY(r.enclosures.isEmpty());
}

void TestSparkleAppcast::validFeed_parsesVersion()
{
    const SparkleAppcastResult r = parse(QByteArray(kMinimalFeed));
    QVERIFY(r.ok);
    QCOMPARE(r.enclosures.size(), 1);
    QCOMPARE(r.enclosures.first().version, QString("2.0"));
}

void TestSparkleAppcast::validFeed_parsesEdSignature()
{
    const SparkleAppcastResult r = parse(QByteArray(kMinimalFeed));
    QVERIFY(!r.enclosures.first().edSignature.isEmpty());
    QVERIFY(r.enclosures.first().signaturePresent());
}

void TestSparkleAppcast::validFeed_parsesDsaSignature_whenNoEd()
{
    const SparkleAppcastResult r = parse(QByteArray(kLegacyDsaFeed));
    QVERIFY(r.ok);
    QCOMPARE(r.enclosures.size(), 1);
    QVERIFY(r.enclosures.first().edSignature.isEmpty());
    QVERIFY(!r.enclosures.first().dsaSignature.isEmpty());
    QVERIFY(r.enclosures.first().signaturePresent());
}

void TestSparkleAppcast::validFeed_enclosureUrlParsed()
{
    const SparkleAppcastResult r = parse(QByteArray(kMinimalFeed));
    QCOMPARE(r.enclosures.first().url, QString("https://example.com/MyApp-2.0.dmg"));
}

void TestSparkleAppcast::validFeed_multipleItems_latestPicked()
{
    const SparkleAppcastResult r = parse(QByteArray(kMultiItemFeed));
    QVERIFY(r.ok);
    QCOMPARE(r.enclosures.size(), 3);
    const EnclosureInfo *best = latestEnclosure(r);
    QVERIFY(best != nullptr);
    QCOMPARE(best->version, QString("3.0"));
    QCOMPARE(best->url, QString("https://example.com/App-3.0.dmg"));
}

void TestSparkleAppcast::noSignature_signaturePresentFalse()
{
    const SparkleAppcastResult r = parse(QByteArray(kNoSignatureFeed));
    QVERIFY(r.ok);
    QCOMPARE(r.enclosures.size(), 1);
    QVERIFY(!r.enclosures.first().signaturePresent());
}

void TestSparkleAppcast::missingUrl_itemExcluded()
{
    const QByteArray noUrl = R"xml(<?xml version="1.0"?>
<rss xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <item><enclosure sparkle:version="1.0" sparkle:edSignature="AAAA" length="1"/></item>
  </channel>
</rss>)xml";
    const SparkleAppcastResult r = parse(noUrl);
    QVERIFY(r.ok);
    QVERIFY(r.enclosures.isEmpty());
}

void TestSparkleAppcast::parseError_partialResultOk()
{
    // Feed with one valid item then truncated XML — should get one entry.
    const QByteArray partial =
        R"xml(<?xml version="1.0"?>
<rss xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <item>
      <enclosure url="https://example.com/x.dmg" sparkle:version="1.0"
                 sparkle:edSignature="AAAA" length="1"/>
    </item>
    <item>
      <enclosure url="https://example.com/y.dmg" sparkle:version="2.0")xml";
    const SparkleAppcastResult r = parse(partial);
    QVERIFY(r.ok);
    QCOMPARE(r.enclosures.size(), 1);
}

QTEST_MAIN(TestSparkleAppcast)
#include "test_sparkle_appcast.moc"
