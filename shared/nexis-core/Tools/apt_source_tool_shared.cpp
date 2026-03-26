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

QList<APTSourcePtr> AptSourceTool::parseDeb822Stanza(const QString &stanzaText,
                                                      const QString &binaryType,
                                                      const QString &sourceType)
{
    QList<APTSourcePtr> results;

    if (stanzaText.trimmed().isEmpty())
        return results;

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
        return results;

    QString signedBy = fields.value("Signed-By").trimmed();
    bool isSource = types.contains(sourceType);
    QString uri = fields.value("URIs");
    QString components = fields.value("Components");
    bool isActive = fields.value("Enabled", "yes").toLower() == "yes";
    QString typeStr = isSource ? sourceType : binaryType;

    // Expand multi-suite stanzas into one entry per suite
    QStringList suites = fields.value("Suites").split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (suites.isEmpty())
        suites.append("");

    for (const QString &suite : suites) {
        APTSourcePtr aptSource(new APTSource);
        aptSource->format = APTSource::Deb822;
        aptSource->signedByPath = signedBy;
        aptSource->isSource = isSource;
        aptSource->uri = uri;
        aptSource->suites = suite;
        aptSource->components = components;
        aptSource->options = "";
        aptSource->isActive = isActive;
        aptSource->source = QString("%1 %2 %3 %4")
            .arg(typeStr, uri, suite, components);
        results.append(aptSource);
    }

    return results;
}
