#ifndef CRUMBS_SCANNER_H
#define CRUMBS_SCANNER_H

#include <QList>
#include <QString>
#include <QStringList>

#include "nexis-core_global.h"

// FR-123: find residual files left behind in ~/Library/* after an app
// (.app bundle or Homebrew cask) has been uninstalled. macOS only.
//
// Matching policy is conservative: bundle-id prefix match against the
// leaf filename/dirname. Deep content inspection is intentionally out
// of scope to avoid false positives.
namespace CrumbsScanner {

struct NEXISCORESHARED_EXPORT CrumbCandidate {
    QString path;
    qint64  sizeBytes = 0;
    QString matchedBundleId;   // which id in the input produced this hit
};

// Walks the standard ~/Library/* roots and returns every entry whose
// leaf name begins with any of the supplied bundle ids. Results are
// returned sorted by sizeBytes desc so the UI can show the largest
// crumbs first.
NEXISCORESHARED_EXPORT QList<CrumbCandidate> scanCrumbs(const QStringList &bundleIds);

} // namespace CrumbsScanner

#endif // CRUMBS_SCANNER_H
