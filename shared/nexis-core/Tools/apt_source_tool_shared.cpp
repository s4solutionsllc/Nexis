// Shared AptSourceTool static parsing methods — platform-independent.
// Platform-specific implementations live in
// linux/nexis-core/Tools/apt_source_tool.cpp and macos/nexis-core/Tools/apt_source_tool.cpp.

#include "apt_source_tool.h"
#include <QRegularExpression>

APTSourcePtr AptSourceTool::parseSourceListLine(const QString &line,
                                                 const QString &binaryType,
                                                 const QString &sourceType)
{
    QString _line = line.trimmed();
    if (_line.isEmpty())
        return nullptr;

    APTSourcePtr aptSource(new APTSource);
    aptSource->format = APTSource::Legacy;
    aptSource->isActive = !_line.startsWith(QChar('#'));

    _line.remove('#');

    QRegularExpression regexOption("(\\s[\\[]+.*[\\]]+)");
    QRegularExpressionMatch optMatch = regexOption.match(_line);
    if (optMatch.hasMatch())
        aptSource->options = optMatch.captured().trimmed();

    // Extract signed-by path from options
    QRegularExpression signedByRegex("signed-by=([^\\],\\s\\]]+)");
    QRegularExpressionMatch sbMatch = signedByRegex.match(aptSource->options);
    if (sbMatch.hasMatch())
        aptSource->signedByPath = sbMatch.captured(1);

    _line.remove(regexOption);

    QStringList sourceColumns = _line.trimmed().split(QRegularExpression("\\s+"));
    if (sourceColumns.isEmpty())
        return nullptr;

    bool isBinary = sourceColumns.first() == binaryType;
    bool isSource = sourceColumns.first() == sourceType;

    if ((isBinary || isSource) && sourceColumns.count() > 2) {
        aptSource->isSource = isSource;
        aptSource->uri = sourceColumns.at(1);
        aptSource->suites = sourceColumns.at(2);
        aptSource->components = sourceColumns.mid(3).join(' ');
        aptSource->source = line.trimmed().remove('#').trimmed();
        return aptSource;
    }

    return nullptr;
}

APTSourcePtr AptSourceTool::parseDeb822Stanza(const QString &stanzaText,
                                               const QString &binaryType,
                                               const QString &sourceType)
{
    if (stanzaText.trimmed().isEmpty())
        return nullptr;

    QMap<QString, QString> fields;
    for (const QString &entryLine : stanzaText.split('\n')) {
        if (entryLine.trimmed().startsWith('#'))
            continue;
        int sep = entryLine.indexOf(':');
        if (sep > 0) {
            QString key = entryLine.left(sep).trimmed();
            QString value = entryLine.mid(sep + 1).trimmed();
            fields[key] = value;
        }
    }

    QString types = fields.value("Types");
    if (!types.contains(binaryType))
        return nullptr;

    APTSourcePtr aptSource(new APTSource);
    aptSource->format = APTSource::Deb822;
    aptSource->signedByPath = fields.value("Signed-By").trimmed();
    aptSource->isSource = types.contains(sourceType);
    aptSource->uri = fields.value("URIs");
    aptSource->suites = fields.value("Suites");
    aptSource->components = fields.value("Components");
    aptSource->options = "";
    aptSource->isActive = fields.value("Enabled", "yes").toLower() == "yes";

    QString typeStr = aptSource->isSource ? sourceType : binaryType;
    aptSource->source = QString("%1 %2 %3 %4")
        .arg(typeStr).arg(aptSource->uri)
        .arg(aptSource->suites).arg(aptSource->components);

    return aptSource;
}
