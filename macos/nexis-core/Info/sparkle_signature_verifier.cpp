// SECURITY CRITICAL — CTO REVIEW REQUIRED (SSO-15390, reworked SSO-15431)
// See sparkle_signature_verifier.h for threat model and fail-closed contract.

#include "sparkle_signature_verifier.h"

#include <QByteArray>
#include <QDebug>

extern "C" {
#include "vendor/ed25519/ed25519.h"
}

namespace SparkleSignatureVerifier {

// ── helpers ─────────────────────────────────────────────────────────────────

static QByteArray decodeBase64Strict(const QString &s)
{
    // Qt's fromBase64 ignores bad chars by default; use StrictMode so
    // corrupted encoded data fails loudly instead of silently producing
    // wrong bytes and passing a verification that should have failed.
    const auto result = QByteArray::fromBase64Encoding(
        s.toLatin1(), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (result) return result.decoded;
    return QByteArray{};
}

// ── Ed25519 (RFC 8032) ──────────────────────────────────────────────────────
//
// macOS SecKey has no EdDSA support at all (SecKeyAlgorithm only covers
// RSA/ECDSA/ECDH) — the previous implementation of this function referenced
// kSecAttrKeyTypeEdDSA / kSecKeyAlgorithmEdDSASignatureMessageX25519SHA512,
// neither of which exists in any Apple SDK, so it could never compile. A
// CryptoKit Swift shim was considered per SSO-15431 but rejected: this build
// has no existing Swift/CMake language integration, and there is no way to
// build- or test-verify one in this environment. Verification instead uses a
// vendored, verify-only subset of orlp/ed25519 (zlib license) — the same
// implementation Sparkle itself uses — pinned at a specific upstream commit;
// see vendor/ed25519/UPSTREAM.md for the manifest and checksums.

static Result verifyEd25519(const QByteArray &fileData,
                            const QByteArray &signatureBytes,
                            const QByteArray &publicKeyBytes)
{
    if (publicKeyBytes.size() != 32) {
        qWarning() << "sparkle_sig: Ed25519 public key must be 32 bytes, got"
                   << publicKeyBytes.size();
        return Result::DecodingError;
    }
    if (signatureBytes.size() != 64) {
        qWarning() << "sparkle_sig: Ed25519 signature must be 64 bytes, got"
                   << signatureBytes.size();
        return Result::DecodingError;
    }

    const int ok = ed25519_verify(
        reinterpret_cast<const unsigned char *>(signatureBytes.constData()),
        reinterpret_cast<const unsigned char *>(fileData.constData()),
        static_cast<size_t>(fileData.size()),
        reinterpret_cast<const unsigned char *>(publicKeyBytes.constData()));

    if (!ok) {
        qWarning() << "sparkle_sig: Ed25519 verification failed";
        return Result::Invalid;
    }
    return Result::Valid;
}

// ── DSA (legacy, unsupported) ─────────────────────────────────────────────────
// Sparkle 1.x used DSA-SHA1. macOS removed DSA from SecKey in 10.15+, and we
// deliberately do not vendor a DSA implementation to accept it: that would
// require trusting a deprecated, weaker algorithm and would expand the
// verifier's attack surface for a signature scheme we want publishers to
// retire. DSA-only appcasts are reported as UnsupportedLegacyDsa (not
// Invalid) so UI/telemetry can say "publisher must upgrade to Ed25519"
// instead of implying the appcast was tampered with. The public key is not
// passed in — this path never verifies anything, so nothing here should be
// able to imply it checked a DSA signature against an Ed25519 key.

static Result verifyDsa(const QByteArray & /*fileData*/,
                         const QByteArray & /*signatureBytes*/)
{
    qWarning() << "sparkle_sig: legacy DSA-only update; "
                  "app must publish an Ed25519 signature";
    return Result::UnsupportedLegacyDsa;
}

// ── Public API ───────────────────────────────────────────────────────────────

Result verify(const QByteArray &fileData,
              const QString    &edSignatureB64,
              const QString    &dsaSignatureB64,
              const QString    &publicKeyB64)
{
    if (publicKeyB64.isEmpty())
        return Result::MissingKey;
    if (edSignatureB64.isEmpty() && dsaSignatureB64.isEmpty())
        return Result::MissingSignature;

    if (!edSignatureB64.isEmpty()) {
        const QByteArray pubKeyBytes = decodeBase64Strict(publicKeyB64);
        if (pubKeyBytes.isEmpty())
            return Result::DecodingError;
        const QByteArray sigBytes = decodeBase64Strict(edSignatureB64);
        if (sigBytes.isEmpty())
            return Result::DecodingError;
        return verifyEd25519(fileData, sigBytes, pubKeyBytes);
    }

    // Legacy DSA fallback — never verified, see verifyDsa() above.
    const QByteArray sigBytes = decodeBase64Strict(dsaSignatureB64);
    if (sigBytes.isEmpty())
        return Result::DecodingError;
    return verifyDsa(fileData, sigBytes);
}

QString resultDescription(Result r)
{
    switch (r) {
    case Result::Valid:                return "signature verified";
    case Result::Invalid:              return "signature mismatch";
    case Result::MissingKey:           return "no public key in app bundle";
    case Result::MissingSignature:     return "no signature in appcast";
    case Result::DecodingError:        return "key or signature could not be decoded";
    case Result::InternalError:        return "internal verifier error";
    case Result::UnsupportedLegacyDsa: return "publisher must upgrade to EdDSA";
    }
    return "unknown";
}

} // namespace SparkleSignatureVerifier
