#ifndef SPARKLE_SIGNATURE_VERIFIER_H
#define SPARKLE_SIGNATURE_VERIFIER_H

#include <QByteArray>
#include <QString>

#include "nexis-core_global.h"

// ============================================================
// SECURITY CRITICAL — CTO REVIEW REQUIRED (see SSO-15390 AC)
//
// This module performs cryptographic signature verification
// before any downloaded Sparkle installer is executed.  Any
// change to this file MUST be reviewed by the CTO before merge.
//
// Threat model: a compromised or MITM-injected appcast can
// deliver a malicious installer.  Signature verification using
// the app-bundled public key (SUPublicEDKey) is the last line
// of defence before remote code execution.
// ============================================================

namespace SparkleSignatureVerifier {

enum class NEXISCORESHARED_EXPORT Result {
    Valid,           // cryptographic verification passed
    Invalid,         // signature did not match
    MissingKey,      // app Info.plist has no SUPublicEDKey — cannot verify
    MissingSignature,// appcast enclosure had no signature attribute
    DecodingError,   // key or signature bytes could not be base64-decoded
    InternalError,   // OS security API returned an unexpected error
};

// Verify an Ed25519 or DSA Sparkle enclosure signature.
//
// Parameters
//   fileData      — full bytes of the downloaded installer archive
//   edSignatureB64 — base64 sparkle:edSignature from the appcast (preferred)
//   dsaSignatureB64 — base64 sparkle:dsaSignature (legacy; checked when
//                     edSignature is absent)
//   publicKeyB64  — base64 SUPublicEDKey from the app's Info.plist
//
// Returns Result::Valid only when the cryptographic check succeeds.
// Any other return value MUST block installation.
NEXISCORESHARED_EXPORT Result verify(
    const QByteArray &fileData,
    const QString    &edSignatureB64,
    const QString    &dsaSignatureB64,
    const QString    &publicKeyB64);

// Human-readable description of a Result (for log messages / UI labels).
NEXISCORESHARED_EXPORT QString resultDescription(Result r);

} // namespace SparkleSignatureVerifier

#endif // SPARKLE_SIGNATURE_VERIFIER_H
