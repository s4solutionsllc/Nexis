#ifndef UPDATE_INFO_H
#define UPDATE_INFO_H

#include <QDateTime>
#include <QList>
#include <QString>

#include "nexis-core_global.h"

struct NEXISCORESHARED_EXPORT UpdateEntry {
    QString source;
    QString name;
    QString version;

    // Sparkle-only fields (empty for brew/system sources).
    // signatureMetadataPresent is set when the appcast carries a signature
    // AND the app bundle contains a matching public key. This is metadata
    // presence only — it does NOT mean the signature has been cryptographically
    // verified (SparkleSignatureVerifier is not yet invoked on downloaded
    // bytes; see SSO-15431). Do not read this as "trusted" or "safe to install".
    QString enclosureUrl;
    QString edSignature;
    QString dsaSignature;
    QString publicKey;
    bool    signatureMetadataPresent = false;
};

struct NEXISCORESHARED_EXPORT UpdateCheckResult {
    int totalCount = 0;
    QList<UpdateEntry> entries;
    QDateTime checkTime;
    bool success = false;
    QString errorMessage;
};

class NEXISCORESHARED_EXPORT UpdateInfo
{
public:
    virtual ~UpdateInfo() = default;
    virtual UpdateCheckResult checkForUpdates() = 0;
    virtual QStringList availableSources() const = 0;
};

#endif // UPDATE_INFO_H
