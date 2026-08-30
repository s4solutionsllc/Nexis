#include "cleanerml_parser.h"

#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QLoggingCategory>

namespace CleanerML {

namespace {

// Resolves an <action command="..." search="..."/> to one of the six
// in-scope ActionTypes (or reports why it can't). Returns false — with
// `error` set — for anything the parser doesn't understand; the caller
// treats that as fatal for the enclosing <cleaner>, not the whole parse.
// command="winreg"/"winreg.value"/"winreg.key" is the one case that returns
// true without an error: it's recognized, just not representable on
// Linux/macOS, so the caller drops it from the action list instead of
// failing the cleaner.
bool resolveActionType(const QString &command, const QString &search,
                        const QString &path, bool hasRegexFilter,
                        ActionType &outType, QString &error)
{
    if (command.startsWith(QStringLiteral("winreg"))) {
        outType = ActionType::Winreg;
        return true;
    }

    // Check command support before path presence, so e.g. BleachBit's
    // path-less <action command="apt.clean"/> is reported as an unsupported
    // command rather than a misleading "missing path" error.
    if (command != QStringLiteral("truncate")
        && command != QStringLiteral("sqlite.vacuum")
        && command != QStringLiteral("delete")) {
        error = QStringLiteral("unsupported action command=\"%1\"").arg(command);
        return false;
    }

    if (command == QStringLiteral("truncate")) {
        if (path.isEmpty()) {
            error = QStringLiteral("action command=\"%1\" is missing a required path attribute").arg(command);
            return false;
        }
        outType = ActionType::Truncate;
        return true;
    }

    if (command == QStringLiteral("sqlite.vacuum")) {
        if (path.isEmpty()) {
            error = QStringLiteral("action command=\"%1\" is missing a required path attribute").arg(command);
            return false;
        }
        outType = ActionType::SqliteVacuum;
        return true;
    }

    // command == "delete" from here. search="deep" is the one mode where
    // `path` is legitimately optional in real CleanerML — a deep scan's
    // scope comes from its regex/wholeregex filter, not an explicit path
    // (see e.g. BleachBit's deepscan.xml "angular"/"pycache" options).
    if (search == QStringLiteral("deep")) {
        if (!hasRegexFilter) {
            error = QStringLiteral("delete action with search=\"deep\" requires a regex or wholeregex attribute");
            return false;
        }
        outType = ActionType::Regex;
        return true;
    }

    if (path.isEmpty()) {
        error = QStringLiteral("action command=\"%1\" is missing a required path attribute").arg(command);
        return false;
    }

    if (search.isEmpty() || search == QStringLiteral("file")) {
        outType = ActionType::Delete;
    } else if (search == QStringLiteral("glob")) {
        outType = ActionType::Glob;
    } else if (search == QStringLiteral("walk.all")) {
        outType = ActionType::Walk;
    } else if (search == QStringLiteral("walk.files")) {
        outType = hasRegexFilter ? ActionType::Regex : ActionType::Walk;
    } else {
        error = QStringLiteral("delete action has unsupported search=\"%1\"").arg(search);
        return false;
    }

    return true;
}

} // namespace

ParseResult parseXml(const QByteArray &data, const QString &source)
{
    ParseResult result;

    QXmlStreamReader xml(data);

    Cleaner cleaner;
    bool haveCleanerElement = false;
    bool cleanerFailed = false;
    QString failureReason;

    Option currentOption;
    bool inOption = false;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();

        if (xml.isStartElement()) {
            const QString name = xml.name().toString();

            if (name == QStringLiteral("cleaner")) {
                const QXmlStreamAttributes attrs = xml.attributes();
                cleaner.id = attrs.value(QStringLiteral("id")).toString();
                const QString osAttr = attrs.value(QStringLiteral("os")).toString();
                if (!osAttr.isEmpty())
                    cleaner.os = osAttr.split(QLatin1Char(','), Qt::SkipEmptyParts);
                haveCleanerElement = true;

                if (cleaner.id.isEmpty() && !cleanerFailed) {
                    cleanerFailed = true;
                    failureReason = QStringLiteral("<cleaner> element is missing a required id attribute");
                }
            } else if (name == QStringLiteral("option")) {
                currentOption = Option();
                currentOption.id = xml.attributes().value(QStringLiteral("id")).toString();
                inOption = true;

                if (currentOption.id.isEmpty() && !cleanerFailed) {
                    cleanerFailed = true;
                    failureReason = QStringLiteral("<option> element is missing a required id attribute");
                }
            } else if (name == QStringLiteral("label")) {
                const QString text = xml.readElementText(QXmlStreamReader::SkipChildElements);
                if (inOption) currentOption.label = text; else cleaner.label = text;
            } else if (name == QStringLiteral("description")) {
                const QString text = xml.readElementText(QXmlStreamReader::SkipChildElements);
                if (inOption) currentOption.description = text; else cleaner.description = text;
            } else if (name == QStringLiteral("warning")) {
                const QString text = xml.readElementText(QXmlStreamReader::SkipChildElements);
                if (inOption) currentOption.warning = text; else cleaner.warning = text;
            } else if (name == QStringLiteral("action")) {
                if (!inOption) {
                    if (!cleanerFailed) {
                        cleanerFailed = true;
                        failureReason = QStringLiteral("<action> element found outside of an <option>");
                    }
                } else {
                    const QXmlStreamAttributes attrs = xml.attributes();
                    const QString command = attrs.value(QStringLiteral("command")).toString();
                    const QString search  = attrs.value(QStringLiteral("search")).toString();
                    const QString path    = attrs.value(QStringLiteral("path")).toString();
                    QString regex         = attrs.value(QStringLiteral("regex")).toString();
                    if (regex.isEmpty())
                        regex = attrs.value(QStringLiteral("wholeregex")).toString();

                    ActionType type;
                    QString actionError;
                    if (!resolveActionType(command, search, path, !regex.isEmpty(), type, actionError)) {
                        if (!cleanerFailed) {
                            cleanerFailed = true;
                            failureReason = actionError;
                        }
                    } else if (type != ActionType::Winreg) {
                        Action action;
                        action.type = type;
                        action.path = path;
                        action.regex = regex;
                        action.command = command;
                        action.search = search;
                        currentOption.actions.append(action);
                    }
                    // type == Winreg: recognized, silently dropped (SSO-23856 AC).
                }
            }
            // <var>, <running>, and anything else are intentionally ignored —
            // variable substitution is an execution-time concern out of scope
            // for this parser.
        } else if (xml.isEndElement()) {
            if (xml.name() == QStringLiteral("option") && inOption) {
                cleaner.options.append(currentOption);
                inOption = false;
            }
        }
    }

    if (xml.hasError()) {
        result.errors.append({source, cleaner.id,
            QStringLiteral("XML parse error: %1 (line %2)").arg(xml.errorString()).arg(xml.lineNumber())});
        qWarning() << "CleanerMLParser: failed to parse" << source << ":" << xml.errorString();
        return result;
    }

    if (!haveCleanerElement) {
        result.errors.append({source, QString(), QStringLiteral("no <cleaner> root element found")});
        qWarning() << "CleanerMLParser: no <cleaner> root element in" << source;
        return result;
    }

    if (cleanerFailed) {
        result.errors.append({source, cleaner.id, failureReason});
        qWarning() << "CleanerMLParser: skipping cleaner" << cleaner.id << "from" << source << ":" << failureReason;
        return result;
    }

    result.cleaners.append(cleaner);
    return result;
}

ParseResult parseFile(const QString &filePath)
{
    ParseResult result;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        result.errors.append({filePath, QString(), QStringLiteral("cannot open file: %1").arg(f.errorString())});
        qWarning() << "CleanerMLParser: cannot open" << filePath << ":" << f.errorString();
        return result;
    }

    return parseXml(f.readAll(), filePath);
}

ParseResult parseDirectory(const QString &dirPath)
{
    ParseResult result;

    QDir dir(dirPath);
    const QStringList files = dir.entryList(QStringList{QStringLiteral("*.xml")}, QDir::Files, QDir::Name);

    for (const QString &fileName : files) {
        const ParseResult fileResult = parseFile(dir.filePath(fileName));
        result.cleaners.append(fileResult.cleaners);
        result.errors.append(fileResult.errors);
    }

    return result;
}

} // namespace CleanerML
