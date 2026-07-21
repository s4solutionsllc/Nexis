#ifndef SPARKLE_APPCAST_PARSER_H
#define SPARKLE_APPCAST_PARSER_H

#include <QByteArray>
#include <QString>
#include <QList>

#include "nexis-core_global.h"

// Defensive Sparkle appcast XML parser.
//
// Handles both modern Ed25519 (sparkle:edSignature) and legacy DSA
// (sparkle:dsaSignature) enclosure attributes.  A missing or malformed
// feed never throws; callers inspect SparkleAppcastResult::ok.
//
// SIZE GUARD: feeds larger than kMaxFeedBytes are rejected outright to
// prevent denial-of-service from malicious or runaway HTTP servers.
//
// NOTE (security): this parser extracts the signature value and enclosure
// URL from the feed — the actual cryptographic verification happens in
// SparkleSignatureVerifier at download time.  An entry without any
// signature value is flagged signaturePresent=false; callers MUST treat
// such entries as untrusted and must not auto-install them.

namespace SparkleAppcastParser {

static constexpr qint64 kMaxFeedBytes = 2 * 1024 * 1024; // 2 MiB

struct NEXISCORESHARED_EXPORT EnclosureInfo {
    QString url;
    QString version;        // sparkle:version (short) or CFBundleShortVersionString
    QString edSignature;    // sparkle:edSignature (preferred)
    QString dsaSignature;   // sparkle:dsaSignature (legacy fallback)
    qint64  length = 0;

    bool signaturePresent() const {
        return !edSignature.isEmpty() || !dsaSignature.isEmpty();
    }
};

struct NEXISCORESHARED_EXPORT SparkleAppcastResult {
    bool ok = false;
    QString errorMessage;
    QList<EnclosureInfo> enclosures; // latest-first
};

// Parse raw appcast XML bytes.  Never throws; always returns a valid result.
NEXISCORESHARED_EXPORT SparkleAppcastResult parse(const QByteArray &data);

// Pick the best (newest by version string) enclosure from a parsed result.
// Returns nullptr if the list is empty.
NEXISCORESHARED_EXPORT const EnclosureInfo *latestEnclosure(
    const SparkleAppcastResult &result);

} // namespace SparkleAppcastParser

#endif // SPARKLE_APPCAST_PARSER_H
