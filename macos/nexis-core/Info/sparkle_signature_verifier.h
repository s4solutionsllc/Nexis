#ifndef SPARKLE_SIGNATURE_VERIFIER_H
#define SPARKLE_SIGNATURE_VERIFIER_H

#include <QByteArray>
#include <QString>

#include "nexis-core_global.h"

// ============================================================
// SECURITY CRITICAL — CTO REVIEW REQUIRED (see SSO-15390 AC,
// reworked under SSO-15431)
//
// This module performs cryptographic signature verification
// before any downloaded Sparkle installer is executed.  Any
// change to this file MUST be reviewed by the CTO before merge.
//
// Threat model: a compromised or MITM-injected appcast can
// deliver a malicious installer.  Signature verification using
// the app-bundled public key (SUPublicEDKey) is the last line
// of defence before remote code execution.
//
// Fail-closed contract: Result::Valid is the ONLY result that
// means "safe to install". Every other value — including
// InternalError — MUST be treated by callers as a verification
// failure and MUST block installation.  Do not special-case any
// non-Valid result as "probably fine."
// ============================================================

namespace SparkleSignatureVerifier {

enum class NEXISCORESHARED_EXPORT Result {
    Valid,                  // cryptographic verification passed
    Invalid,                // signature did not match
    MissingKey,             // app Info.plist has no SUPublicEDKey — cannot verify
    MissingSignature,       // appcast enclosure had no signature attribute
    DecodingError,          // key or signature bytes could not be base64-decoded
    InternalError,          // reserved for defensive/unexpected failure modes;
                            // unused by the current implementation but callers
                            // must still fail closed on it (see contract above)
    UnsupportedLegacyDsa,   // appcast only offers a legacy DSA-SHA1 signature;
                            // publisher must upgrade to Ed25519 (not a sign of
                            // tampering — distinct from Invalid for UI/telemetry)
};

// Verify an Ed25519 Sparkle enclosure signature. DSA-only appcasts are
// rejected with Result::UnsupportedLegacyDsa (see enum).
//
// Parameters
//   fileData      — full bytes of the downloaded installer archive
//   edSignatureB64 — base64 sparkle:edSignature from the appcast (preferred)
//   dsaSignatureB64 — base64 sparkle:dsaSignature (legacy; checked only when
//                     edSignature is absent, and only to distinguish
//                     UnsupportedLegacyDsa from MissingSignature)
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
