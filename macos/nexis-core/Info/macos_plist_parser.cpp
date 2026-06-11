#include "macos_plist_parser.h"

#include <QStringList>
#include <QXmlStreamReader>

namespace MacosPlistParser {

QMap<QString, QVariant> parse(const QByteArray &data)
{
    QMap<QString, QVariant> result;
    QXmlStreamReader xml(data);

    QString currentKey;
    bool inSmartDict = false;
    int dictDepth = 0;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("dict")) {
                dictDepth++;
                if (currentKey == "SMARTDeviceSpecificKeysMayVaryNotGuaranteed") {
                    inSmartDict = true;
                    currentKey.clear();
                }
            }
            else if (xml.name() == QStringLiteral("key")) {
                QString key = xml.readElementText();
                if (inSmartDict)
                    currentKey = "SMART." + key;
                else
                    currentKey = key;
            }
            else if (xml.name() == QStringLiteral("string")) {
                result[currentKey] = xml.readElementText();
            }
            else if (xml.name() == QStringLiteral("integer")) {
                result[currentKey] = xml.readElementText().toLongLong();
            }
            else if (xml.name() == QStringLiteral("true")) {
                result[currentKey] = true;
            }
            else if (xml.name() == QStringLiteral("false")) {
                result[currentKey] = false;
            }
            else if (xml.name() == QStringLiteral("array")) {
                QStringList items;
                while (!xml.atEnd()) {
                    xml.readNext();
                    if (xml.isStartElement() && xml.name() == QStringLiteral("string"))
                        items.append(xml.readElementText());
                    else if (xml.isEndElement() && xml.name() == QStringLiteral("array"))
                        break;
                }
                result[currentKey] = items;
            }
        }
        else if (xml.isEndElement() && xml.name() == QStringLiteral("dict")) {
            dictDepth--;
            if (inSmartDict && dictDepth <= 1)
                inSmartDict = false;
        }
    }

    return result;
}

} // namespace MacosPlistParser
