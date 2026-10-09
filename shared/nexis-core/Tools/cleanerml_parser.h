#ifndef CLEANERML_PARSER_H
#define CLEANERML_PARSER_H

#include <QByteArray>
#include <QString>
#include <QList>

#include "cleanerml_model.h"
#include "nexis-core_global.h"

// CleanerML (BleachBit-compatible) XML parser.
//
// Scope: <cleaner>/<option>/<action> elements only. <var>, <running>, and
// any other element are recognized-and-ignored so real-world cleaner
// definitions (which lean on them heavily) don't trip parsing — path
// variable substitution is an execution-time concern, not the parser's.
//
// Failure model (SSO-23856): a single malformed or unsupported-action
// definition fails parsing for *that one cleaner* — it's dropped and an
// entry is appended to ParseResult::errors (and logged via qWarning) — it
// never aborts parsing of the rest of a directory, and never throws.
// winreg[.*] actions are the one exception: they're recognized but silently
// skipped rather than treated as unsupported, since Linux/macOS builds have
// no registry to act on.
namespace CleanerML {

struct NEXISCORESHARED_EXPORT ParseError {
    QString source;    // file path (or caller-supplied label for parseXml)
    QString cleanerId; // best-effort; empty if the failure happened before an id was read
    QString message;
};

struct NEXISCORESHARED_EXPORT ParseResult {
    QList<Cleaner> cleaners;
    QList<ParseError> errors;
};

// Strict: an action Nexis cannot run fails its whole cleaner (the original
// SSO-23856 contract). Lenient: such an action is kept as
// ActionType::Unsupported so CleanerML::forPlatform() can decide per option,
// which is what lets a real-world cleaner with one exotic option still be
// used for its ordinary ones.
enum class ParseMode { Strict, Lenient };

// Parse a single CleanerML document already in memory. `source` is used only
// to label any ParseError produced (typically the originating file path).
NEXISCORESHARED_EXPORT ParseResult parseXml(const QByteArray &data, const QString &source = QString(),
                                            ParseMode mode = ParseMode::Strict);

// Read and parse a single .xml file.
NEXISCORESHARED_EXPORT ParseResult parseFile(const QString &filePath, ParseMode mode = ParseMode::Strict);

// Parse every *.xml file directly inside `dirPath` (non-recursive, sorted by
// name for determinism) and aggregate the results. A directory that doesn't
// exist or contains no .xml files yields an empty (not erroring) result.
NEXISCORESHARED_EXPORT ParseResult parseDirectory(const QString &dirPath, ParseMode mode = ParseMode::Strict);

} // namespace CleanerML

#endif // CLEANERML_PARSER_H
