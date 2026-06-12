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

    // SSO-3728: legacy arch= option (e.g. "[arch=amd64,arm64]"). APT accepts
    // a comma-separated list; we normalise to space-separated to match the
    // deb822 Architectures: convention so the edit dialog can show one form.
    QRegularExpression archRegex("arch=([^\\],\\s\\]]+(?:,[^\\],\\s\\]]+)*)");
    QRegularExpressionMatch archMatch = archRegex.match(aptSource->options);
    if (archMatch.hasMatch())
        aptSource->architectures = archMatch.captured(1).replace(',', ' ');

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
    QString architectures = fields.value("Architectures").trimmed();
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
        aptSource->architectures = architectures;
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

// SSO-3728 / FW-01: rewrites a single deb822 stanza, preserving everything we
// don't recognise. The original stanza is the source of truth for ordering,
// blank-line structure, and multi-line continuation fields (e.g. embedded
// Signed-By GPG keys). We only touch the well-known fields (Types, URIs,
// Suites, Components, Architectures, Signed-By, Enabled). Anything else
// (Languages, Targets, PDiffs, Allow-Insecure, etc.) is preserved verbatim.
QString AptSourceTool::serializeDeb822Stanza(const QString &originalStanza,
                                             const APTSourcePtr &matchEntry,
                                             const APTSourcePtr &newSource,
                                             const QString &binaryType,
                                             const QString &sourceType)
{
    QStringList entryLines = originalStanza.split('\n');
    // Drop a single trailing empty line if present (caller may have included
    // the blank-line separator); we re-add it on join.
    while (!entryLines.isEmpty() && entryLines.last().isEmpty())
        entryLines.removeLast();

    if (entryLines.isEmpty())
        return QString();

    // Build a quick field map for the match-check; the rewrite walks the
    // original line list to preserve order.
    QMap<QString, QString> originalFields;
    for (const QString &entryLine : entryLines) {
        if (entryLine.trimmed().startsWith('#'))
            continue;
        if (entryLine.startsWith(' ') || entryLine.startsWith('\t'))
            continue; // continuation line — belongs to previous field
        int sep = entryLine.indexOf(':');
        if (sep > 0) {
            QString key = entryLine.left(sep).trimmed();
            QString value = entryLine.mid(sep + 1).trimmed();
            originalFields[key] = value;
        }
    }

    bool stanzaMatches = matchEntry.isNull()
        || ((originalFields.value("URIs") == matchEntry->uri)
            && originalFields.value("Suites")
                   .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)
                   .contains(matchEntry->suites));

    if (!stanzaMatches)
        return entryLines.join('\n'); // pass through unmodified

    if (newSource.isNull())
        return QString(); // drop stanza

    // Compute the new field values from newSource. Keep a copy of the
    // originals so we can detect no-op edits and emit byte-stable output.
    QMap<QString, QString> updatedFields = originalFields;
    QString newType = newSource->isSource ? sourceType : binaryType;
    if (originalFields.value("Types").contains(binaryType)
        && originalFields.value("Types").contains(sourceType)) {
        // The stanza already declared both — keep that shape.
        updatedFields["Types"] = QString("%1 %2").arg(binaryType, sourceType);
    } else {
        updatedFields["Types"] = newType;
    }
    updatedFields["URIs"] = newSource->uri.trimmed();
    // If the original stanza had multiple suites, only the matching suite is
    // edited and the others are preserved.
    QStringList originalSuites = originalFields.value("Suites")
        .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (!matchEntry.isNull() && originalSuites.size() > 1
        && originalSuites.contains(matchEntry->suites)) {
        for (QString &s : originalSuites)
            if (s == matchEntry->suites)
                s = newSource->suites.trimmed();
        updatedFields["Suites"] = originalSuites.join(' ');
    } else {
        updatedFields["Suites"] = newSource->suites.trimmed();
    }
    updatedFields["Components"] = newSource->components.trimmed();
    if (!newSource->architectures.trimmed().isEmpty())
        updatedFields["Architectures"] = newSource->architectures.trimmed();
    else
        updatedFields.remove("Architectures");
    if (!newSource->signedByPath.trimmed().isEmpty())
        updatedFields["Signed-By"] = newSource->signedByPath.trimmed();
    // We intentionally do not remove a Signed-By that came in via the original
    // stanza; multi-line embedded keys are preserved further down.
    if (!newSource->isActive)
        updatedFields["Enabled"] = "no";
    else
        updatedFields.remove("Enabled");

    // Reconstruct the stanza line-by-line. For every original field-bearing
    // line we keep the line verbatim if the value is unchanged (byte-stable
    // round-trip), otherwise we re-emit "Key: NewValue". Continuation lines
    // for multi-line Signed-By are kept verbatim unless we're overriding the
    // value, in which case we emit a single-line replacement.
    QStringList outLines;
    QSet<QString> handled;
    bool inMultilineSignedBy = false;
    bool overrideSignedBy = !newSource->signedByPath.trimmed().isEmpty()
        && newSource->signedByPath.trimmed() != originalFields.value("Signed-By").trimmed();

    for (int i = 0; i < entryLines.size(); ++i) {
        const QString &line = entryLines[i];

        if (inMultilineSignedBy) {
            if (line.startsWith(' ') || line.startsWith('\t')) {
                if (!overrideSignedBy)
                    outLines << line;
                continue;
            }
            inMultilineSignedBy = false;
        }

        if (line.trimmed().startsWith('#') || line.trimmed().isEmpty()) {
            outLines << line;
            continue;
        }

        int sep = line.indexOf(':');
        if (sep <= 0) {
            outLines << line;
            continue;
        }

        QString key = line.left(sep).trimmed();
        QString origValue = line.mid(sep + 1).trimmed();
        if (!updatedFields.contains(key)) {
            // Field was removed (e.g. Enabled went back to yes / Architectures
            // wiped); skip the line.
            if (key == "Signed-By")
                inMultilineSignedBy = true;
            continue;
        }

        QString newValue = updatedFields.value(key);
        if (key == "Signed-By") {
            // Detect whether the original stanza used the multi-line embedded
            // key form. If so, and we're not overriding the path, copy the
            // original line verbatim and let the continuation loop copy the
            // rest. If we are overriding, emit a single-line replacement.
            inMultilineSignedBy = true;
            if (overrideSignedBy) {
                outLines << QString("Signed-By: %1").arg(newValue);
            } else {
                outLines << line;
            }
            handled.insert(key);
            continue;
        }

        if (origValue == newValue) {
            outLines << line;
        } else {
            // Preserve any leading whitespace before the key.
            int firstNonSpace = 0;
            while (firstNonSpace < line.size() && line[firstNonSpace].isSpace())
                ++firstNonSpace;
            QString indent = line.left(firstNonSpace);
            outLines << QString("%1%2: %3").arg(indent, key, newValue);
        }
        handled.insert(key);
    }

    // Append any fields we added (e.g. Signed-By or Architectures didn't exist
    // before). Keep a deterministic order matching the deb822 convention.
    const QStringList appendOrder = {
        "Types", "URIs", "Suites", "Components", "Architectures",
        "Signed-By", "Enabled"
    };
    for (const QString &key : appendOrder) {
        if (!updatedFields.contains(key) || handled.contains(key))
            continue;
        outLines << QString("%1: %2").arg(key, updatedFields.value(key));
    }

    return outLines.join('\n');
}

QString AptSourceTool::buildDeb822Stanza(const APTSourcePtr &source,
                                          const QString &binaryType,
                                          const QString &sourceType)
{
    if (source.isNull())
        return QString();

    QStringList lines;
    lines << QString("Types: %1").arg(source->isSource ? sourceType : binaryType);
    lines << QString("URIs: %1").arg(source->uri.trimmed());
    lines << QString("Suites: %1").arg(source->suites.trimmed());
    if (!source->components.trimmed().isEmpty())
        lines << QString("Components: %1").arg(source->components.trimmed());
    if (!source->architectures.trimmed().isEmpty())
        lines << QString("Architectures: %1").arg(source->architectures.trimmed());
    if (!source->signedByPath.trimmed().isEmpty())
        lines << QString("Signed-By: %1").arg(source->signedByPath.trimmed());
    if (!source->isActive)
        lines << QString("Enabled: no");

    return lines.join('\n') + '\n';
}
