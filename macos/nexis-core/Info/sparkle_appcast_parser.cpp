#include "sparkle_appcast_parser.h"

#include <QXmlStreamReader>
#include <QVersionNumber>

namespace SparkleAppcastParser {

SparkleAppcastResult parse(const QByteArray &data)
{
    SparkleAppcastResult result;

    if (data.isEmpty()) {
        result.errorMessage = "empty feed";
        return result;
    }
    if (data.size() > kMaxFeedBytes) {
        result.errorMessage = "feed exceeds size limit";
        return result;
    }

    QXmlStreamReader xml(data);

    // Walk the XML.  We collect <item> entries; within each <item> we look
    // for <enclosure> elements that carry the Sparkle extension attributes.
    // All other elements are skipped so malformed or extended feeds don't
    // crash parsing.

    bool inItem = false;
    EnclosureInfo pending;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.hasError()) {
            // Surface the first XML error but keep whatever we parsed so far.
            if (result.enclosures.isEmpty())
                result.errorMessage = xml.errorString();
            break;
        }

        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("item")) {
                inItem = true;
                pending = EnclosureInfo{};
            } else if (inItem && xml.name() == QStringLiteral("enclosure")) {
                const QXmlStreamAttributes attrs = xml.attributes();

                // url — required
                pending.url = attrs.value(QStringLiteral("url")).toString();

                // Sparkle version — prefer sparkle:version over sparkle:shortVersionString
                pending.version = attrs.value(
                    QStringLiteral("http://www.andymatuschak.org/xml-namespaces/sparkle"),
                    QStringLiteral("version")).toString();
                if (pending.version.isEmpty())
                    pending.version = attrs.value(
                        QStringLiteral("http://www.andymatuschak.org/xml-namespaces/sparkle"),
                        QStringLiteral("shortVersionString")).toString();

                // Ed25519 signature (preferred)
                pending.edSignature = attrs.value(
                    QStringLiteral("http://www.andymatuschak.org/xml-namespaces/sparkle"),
                    QStringLiteral("edSignature")).toString();

                // Legacy DSA signature (fallback)
                pending.dsaSignature = attrs.value(
                    QStringLiteral("http://www.andymatuschak.org/xml-namespaces/sparkle"),
                    QStringLiteral("dsaSignature")).toString();

                // Enclosure byte length
                const QString lenStr = attrs.value(QStringLiteral("length")).toString();
                if (!lenStr.isEmpty())
                    pending.length = lenStr.toLongLong();
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == QStringLiteral("item") && inItem) {
                if (!pending.url.isEmpty())
                    result.enclosures.append(pending);
                inItem = false;
                pending = EnclosureInfo{};
            }
        }
    }

    result.ok = true;
    return result;
}

const EnclosureInfo *latestEnclosure(const SparkleAppcastResult &result)
{
    if (result.enclosures.isEmpty())
        return nullptr;

    const EnclosureInfo *best = &result.enclosures.first();
    for (const EnclosureInfo &enc : result.enclosures) {
        QVersionNumber current = QVersionNumber::fromString(enc.version);
        QVersionNumber bestVer = QVersionNumber::fromString(best->version);
        if (current > bestVer)
            best = &enc;
    }
    return best;
}

} // namespace SparkleAppcastParser
