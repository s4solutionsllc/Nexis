#include <QTest>
#include "Info/sparkle_signature_verifier.h"

using namespace SparkleSignatureVerifier;

// RFC 8032 §7.1 Ed25519 known-answer tests, plus fail-closed coverage for
// every other Result value SparkleSignatureVerifier::verify() can return.
// See sparkle_signature_verifier.h for the fail-closed contract this suite
// pins down: Result::Valid is the only "safe to install" outcome.
class TestSparkleSignatureVerifier : public QObject
{
    Q_OBJECT

private slots:
    void rfc8032Vector1_emptyMessage_valid();
    void rfc8032Vector2_oneByteMessage_valid();
    void rfc8032Vector3_twoByteMessage_valid();
    void tamperedMessage_invalid();
    void tamperedSignature_invalid();
    void wrongPublicKey_invalid();
    void missingKey_failsClosed();
    void missingSignature_failsClosed();
    void malformedBase64PublicKey_failsClosed();
    void malformedBase64Signature_failsClosed();
    void wrongPublicKeyLength_failsClosed();
    void wrongSignatureLength_failsClosed();
    void dsaOnlyAppcast_reportsUnsupportedLegacyDsa_notInvalid();
    void everyNonValidResult_isDistinctFromValid();
};

namespace {

QByteArray hexToBytes(const char *hex)
{
    return QByteArray::fromHex(QByteArray(hex));
}

QString hexToBase64(const char *hex)
{
    return QString::fromLatin1(hexToBytes(hex).toBase64());
}

// RFC 8032 §7.1 TEST 1 — public key, message, signature (secret key omitted;
// this verifier never signs).
const char *kTest1PublicKeyHex =
    "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
const char *kTest1MessageHex = ""; // zero-length message
const char *kTest1SignatureHex =
    "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901"
    "555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a"
    "100b";

// RFC 8032 §7.1 TEST 2
const char *kTest2PublicKeyHex =
    "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c";
const char *kTest2MessageHex = "72";
const char *kTest2SignatureHex =
    "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69"
    "da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb"
    "0c00";

// RFC 8032 §7.1 TEST 3
const char *kTest3PublicKeyHex =
    "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025";
const char *kTest3MessageHex = "af82";
const char *kTest3SignatureHex =
    "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3"
    "ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1e"
    "c40a";

} // namespace

void TestSparkleSignatureVerifier::rfc8032Vector1_emptyMessage_valid()
{
    const Result r = verify(hexToBytes(kTest1MessageHex),
                            hexToBase64(kTest1SignatureHex), QString(),
                            hexToBase64(kTest1PublicKeyHex));
    QCOMPARE(r, Result::Valid);
}

void TestSparkleSignatureVerifier::rfc8032Vector2_oneByteMessage_valid()
{
    const Result r = verify(hexToBytes(kTest2MessageHex),
                            hexToBase64(kTest2SignatureHex), QString(),
                            hexToBase64(kTest2PublicKeyHex));
    QCOMPARE(r, Result::Valid);
}

void TestSparkleSignatureVerifier::rfc8032Vector3_twoByteMessage_valid()
{
    const Result r = verify(hexToBytes(kTest3MessageHex),
                            hexToBase64(kTest3SignatureHex), QString(),
                            hexToBase64(kTest3PublicKeyHex));
    QCOMPARE(r, Result::Valid);
}

void TestSparkleSignatureVerifier::tamperedMessage_invalid()
{
    // TEST 3's message with one bit flipped must fail verification against
    // the unmodified TEST 3 signature/key — the exact MITM-appcast scenario
    // this verifier exists to catch.
    QByteArray tampered = hexToBytes(kTest3MessageHex);
    QVERIFY(!tampered.isEmpty());
    tampered[0] = tampered[0] ^ 0x01;

    const Result r = verify(tampered, hexToBase64(kTest3SignatureHex),
                            QString(), hexToBase64(kTest3PublicKeyHex));
    QCOMPARE(r, Result::Invalid);
}

void TestSparkleSignatureVerifier::tamperedSignature_invalid()
{
    QByteArray tamperedSig = hexToBytes(kTest3SignatureHex);
    QCOMPARE(tamperedSig.size(), 64);
    tamperedSig[63] = tamperedSig[63] ^ 0x01;

    const Result r = verify(hexToBytes(kTest3MessageHex),
                            QString::fromLatin1(tamperedSig.toBase64()),
                            QString(), hexToBase64(kTest3PublicKeyHex));
    QCOMPARE(r, Result::Invalid);
}

void TestSparkleSignatureVerifier::wrongPublicKey_invalid()
{
    // TEST 2's valid signature checked against TEST 1's public key must fail.
    const Result r = verify(hexToBytes(kTest2MessageHex),
                            hexToBase64(kTest2SignatureHex), QString(),
                            hexToBase64(kTest1PublicKeyHex));
    QCOMPARE(r, Result::Invalid);
}

void TestSparkleSignatureVerifier::missingKey_failsClosed()
{
    const Result r = verify(hexToBytes(kTest1MessageHex),
                            hexToBase64(kTest1SignatureHex), QString(),
                            QString());
    QCOMPARE(r, Result::MissingKey);
}

void TestSparkleSignatureVerifier::missingSignature_failsClosed()
{
    const Result r = verify(hexToBytes(kTest1MessageHex), QString(),
                            QString(), hexToBase64(kTest1PublicKeyHex));
    QCOMPARE(r, Result::MissingSignature);
}

void TestSparkleSignatureVerifier::malformedBase64PublicKey_failsClosed()
{
    const Result r = verify(hexToBytes(kTest1MessageHex),
                            hexToBase64(kTest1SignatureHex), QString(),
                            QStringLiteral("not-valid-base64!!!"));
    QCOMPARE(r, Result::DecodingError);
}

void TestSparkleSignatureVerifier::malformedBase64Signature_failsClosed()
{
    const Result r = verify(hexToBytes(kTest1MessageHex),
                            QStringLiteral("not-valid-base64!!!"), QString(),
                            hexToBase64(kTest1PublicKeyHex));
    QCOMPARE(r, Result::DecodingError);
}

void TestSparkleSignatureVerifier::wrongPublicKeyLength_failsClosed()
{
    QByteArray shortKey = hexToBytes(kTest1PublicKeyHex);
    shortKey.chop(1); // 31 bytes, not 32
    const Result r = verify(hexToBytes(kTest1MessageHex),
                            hexToBase64(kTest1SignatureHex), QString(),
                            QString::fromLatin1(shortKey.toBase64()));
    QCOMPARE(r, Result::DecodingError);
}

void TestSparkleSignatureVerifier::wrongSignatureLength_failsClosed()
{
    QByteArray shortSig = hexToBytes(kTest1SignatureHex);
    shortSig.chop(1); // 63 bytes, not 64
    const Result r = verify(hexToBytes(kTest1MessageHex),
                            QString::fromLatin1(shortSig.toBase64()),
                            QString(), hexToBase64(kTest1PublicKeyHex));
    QCOMPARE(r, Result::DecodingError);
}

void TestSparkleSignatureVerifier::dsaOnlyAppcast_reportsUnsupportedLegacyDsa_notInvalid()
{
    // A DSA-only appcast is a publisher who hasn't upgraded, not necessarily
    // tampering — must be distinguishable from Invalid, and must still fail
    // closed (i.e. never Valid).
    const Result r = verify(hexToBytes(kTest1MessageHex), QString(),
                            QStringLiteral("MCwCFDSASIGNATUREBASE64ABCDEFGHIJKL"),
                            hexToBase64(kTest1PublicKeyHex));
    QCOMPARE(r, Result::UnsupportedLegacyDsa);
    QVERIFY(r != Result::Invalid);
    QVERIFY(r != Result::Valid);
}

void TestSparkleSignatureVerifier::everyNonValidResult_isDistinctFromValid()
{
    // Pins the fail-closed contract from the header: every Result other than
    // Valid must compare unequal to Valid, so a caller that only special-cases
    // Valid (and treats everything else as failure) is correct by construction.
    const QList<Result> nonValid = {
        Result::Invalid,       Result::MissingKey,      Result::MissingSignature,
        Result::DecodingError, Result::InternalError,   Result::UnsupportedLegacyDsa,
    };
    for (Result r : nonValid)
        QVERIFY(r != Result::Valid);
}

QTEST_MAIN(TestSparkleSignatureVerifier)
#include "test_sparkle_signature_verifier.moc"
