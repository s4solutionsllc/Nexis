// SECURITY CRITICAL — CTO REVIEW REQUIRED (SSO-15390)
// See sparkle_signature_verifier.h for threat model.

#include "sparkle_signature_verifier.h"

#include <QByteArray>
#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

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

// ── Ed25519 via macOS Security.framework ────────────────────────────────────

// Sparkle 2.x signs with Ed25519 (RFC 8032).  The macOS Security framework
// exposes Ed25519 under kSecAttrKeyTypeEdDSA with the algorithm constant
// kSecKeyAlgorithmEdDSASignatureMessageX25519SHA512.
//
// CTO NOTE: This is the primary verification path.  The signed payload is
// the raw archive bytes; no additional hashing is applied by us because
// the Security framework performs the required internal SHA-512 digest for
// this algorithm constant.

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

    // Build CFData for key and signature
    CFDataRef cfKey = CFDataCreate(kCFAllocatorDefault,
        reinterpret_cast<const UInt8 *>(publicKeyBytes.constData()),
        static_cast<CFIndex>(publicKeyBytes.size()));

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kSecAttrKeyType,  kSecAttrKeyTypeEdDSA);
    CFDictionarySetValue(attrs, kSecAttrKeyClass, kSecAttrKeyClassPublic);
    int bitsVal = 256;
    CFNumberRef bits = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitsVal);
    CFDictionarySetValue(attrs, kSecAttrKeySizeInBits, bits);

    CFErrorRef cfError = nullptr;
    SecKeyRef pubKey = SecKeyCreateWithData(cfKey, attrs, &cfError);

    CFRelease(bits);
    CFRelease(attrs);
    CFRelease(cfKey);

    if (!pubKey) {
        if (cfError) {
            qWarning() << "sparkle_sig: SecKeyCreateWithData failed:"
                       << QString::fromCFString(CFErrorCopyDescription(cfError));
            CFRelease(cfError);
        }
        return Result::InternalError;
    }

    CFDataRef cfData = CFDataCreateWithBytesNoCopy(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8 *>(fileData.constData()),
        static_cast<CFIndex>(fileData.size()),
        kCFAllocatorNull);

    CFDataRef cfSig = CFDataCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8 *>(signatureBytes.constData()),
        static_cast<CFIndex>(signatureBytes.size()));

    CFErrorRef verifyError = nullptr;
    bool ok = SecKeyVerifySignature(
        pubKey,
        kSecKeyAlgorithmEdDSASignatureMessageX25519SHA512,
        cfData, cfSig, &verifyError);

    CFRelease(cfSig);
    CFRelease(cfData);
    CFRelease(pubKey);

    if (!ok) {
        if (verifyError) {
            qWarning() << "sparkle_sig: Ed25519 verification failed:"
                       << QString::fromCFString(CFErrorCopyDescription(verifyError));
            CFRelease(verifyError);
        }
        return Result::Invalid;
    }
    return Result::Valid;
}

// ── DSA (legacy) ─────────────────────────────────────────────────────────────
// Sparkle 1.x used DSA-SHA1.  macOS removed DSA from SecKey in 10.15+.
// We flag legacy-DSA-only updates as Invalid to avoid relying on a deprecated
// code path that may be absent at runtime.  Apps that still publish only DSA
// signatures should upgrade their appcasts to Ed25519.

static Result verifyDsa(const QByteArray & /*fileData*/,
                         const QByteArray & /*signatureBytes*/,
                         const QByteArray & /*publicKeyBytes*/)
{
    // CTO NOTE: DSA-SHA1 is intentionally not implemented.
    // macOS removed the DSA signing/verification API in 10.15.  Accepting
    // a DSA signature here would require linking a third-party crypto library
    // (e.g. libssl) and introduces additional attack surface.  The decision
    // to block DSA-only updates is a conservative security posture: callers
    // surface these as untrusted.
    qWarning() << "sparkle_sig: legacy DSA-only update rejected; "
                  "app must publish an Ed25519 signature";
    return Result::Invalid;
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

    const QByteArray pubKeyBytes = decodeBase64Strict(publicKeyB64);
    if (pubKeyBytes.isEmpty())
        return Result::DecodingError;

    if (!edSignatureB64.isEmpty()) {
        const QByteArray sigBytes = decodeBase64Strict(edSignatureB64);
        if (sigBytes.isEmpty())
            return Result::DecodingError;
        return verifyEd25519(fileData, sigBytes, pubKeyBytes);
    }

    // Legacy DSA fallback
    const QByteArray sigBytes = decodeBase64Strict(dsaSignatureB64);
    if (sigBytes.isEmpty())
        return Result::DecodingError;
    return verifyDsa(fileData, sigBytes, pubKeyBytes);
}

QString resultDescription(Result r)
{
    switch (r) {
    case Result::Valid:            return "signature verified";
    case Result::Invalid:         return "signature mismatch";
    case Result::MissingKey:      return "no public key in app bundle";
    case Result::MissingSignature:return "no signature in appcast";
    case Result::DecodingError:   return "key or signature could not be decoded";
    case Result::InternalError:   return "OS security API error";
    }
    return "unknown";
}

} // namespace SparkleSignatureVerifier
