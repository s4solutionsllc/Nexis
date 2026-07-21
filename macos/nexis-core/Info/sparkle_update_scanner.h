#ifndef SPARKLE_UPDATE_SCANNER_H
#define SPARKLE_UPDATE_SCANNER_H

#include <QStringList>
#include <Info/update_info.h>

// Scans /Applications (and ~/Applications) for .app bundles not managed by
// Homebrew, reads SUFeedURL from each bundle's Info.plist, fetches and parses
// the Sparkle appcast, and returns UpdateEntry items with source="sparkle".
//
// Design constraints
//   - Failures for individual apps do not block scanning of others.
//   - Apps without SUFeedURL are silently excluded.
//   - Feeds larger than SparkleAppcastParser::kMaxFeedBytes are rejected.
//   - An entry is marked signatureMetadataPresent=false when no signature is
//     present in the appcast. This is metadata presence only, not
//     cryptographic verification (see SparkleSignatureVerifier / SSO-15431);
//     the UI must not claim these entries are "verified" or "trusted".
//   - This class is intentionally call-thread-agnostic: it performs blocking
//     network I/O via a local QEventLoop and must be called from a worker
//     thread, not the GUI thread.

class SparkleUpdateScanner
{
public:
    // homebrewAppNames — set of bundle names (sans .app) currently managed by
    // Homebrew casks.  Apps in this set are excluded from Sparkle scanning so
    // we don't double-report them.
    QList<UpdateEntry> scan(const QStringList &homebrewAppNames = {}) const;

    // Visible for testing: fetch raw appcast bytes for a given URL.
    // Returns empty QByteArray on network error or size-limit breach.
    static QByteArray fetchFeed(const QString &feedUrl);
};

#endif // SPARKLE_UPDATE_SCANNER_H
