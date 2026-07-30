// SSO-17776 hard gate (AC4): SparkleUpdateInstaller::verifyAndInstall() is
// the real call site that turns SparkleSignatureVerifier from dead code
// into an RCE-adjacent control. This suite proves the fail-closed contract
// at THAT layer, not just inside the verifier: for every non-Valid Result,
// the injected launcher/revealer spies must be invoked exactly zero times,
// and the outcome must fall back to browser-open. Also covers the format
// scope gate (AC7) and the floor-version store (AC5).
//
// SSO-17899 (SSO-17776 follow-up): the fixtures above never construct a
// genuinely valid signature, so the success path — Valid -> Installed /
// RevealedForManualInstall, launcher/revealer called exactly once, floor
// ratcheted — had no coverage. validSignature_pkg_installsAndRatchetsFloor()
// and validSignature_zip_revealsAndRatchetsFloor() close that gap using a
// real ed25519 signature over a real pkgbuild/ditto-produced fixture.

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>

#include <cstring>

#include "Info/sparkle_update_installer.h"
#include "Info/sparkle_update_floor_store.h"
#include "Info/sparkle_update_downloader.h"
#include <Utils/command_util.h>

extern "C" {
#include "ge.h"
#include "sc.h"
#include "sha512.h"
}

using namespace SparkleSignatureVerifier;

namespace {

// RFC 8032 §7.1 TEST 1 vector, reused from test_sparkle_signature_verifier.cpp
// — a well-formed (correct length) key/signature pair is what a real
// hostile appcast would actually be able to serve (garbage-length values
// are caught even earlier, by DecodingError). Using a *real* KAT vector but
// against payload bytes it was never signed over is what produces a
// genuine Result::Invalid rather than a decoding short-circuit.
const char *kKeyHex = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
const char *kSigHex =
    "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901"
    "555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a"
    "100b";

QString hexToBase64(const char *hex)
{
    return QString::fromLatin1(QByteArray::fromHex(QByteArray(hex)).toBase64());
}

// SSO-17898 regression fixture: a genuinely Ed25519-valid .zip payload
// (generated offline — this vendored verifier is verify-only, see
// sparkle_signature_verifier.cpp, so no in-test signing is possible). The
// zip contains a single top-level SpyApp.app/Contents/Info.plist with
// CFBundleShortVersionString "0.0.0", so it reaches the §7 downgrade check
// with a real (low) embedded version instead of stopping earlier at
// signature verification or version extraction like the other fixtures in
// this file.
const char *kZipFixtureBase64 =
    "UEsDBBQAAAAAAAAAIVCUj9dtPwEAAD8BAAAeAAAAU3B5QXBwLmFwcC9Db250ZW50cy9JbmZv"
    "LnBsaXN0PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiPz4KPCFET0NUWVBF"
    "IHBsaXN0IFBVQkxJQyAiLS8vQXBwbGUvL0RURCBQTElTVCAxLjAvL0VOIiAiaHR0cDovL3d3"
    "dy5hcHBsZS5jb20vRFREcy9Qcm9wZXJ0eUxpc3QtMS4wLmR0ZCI+CjxwbGlzdCB2ZXJzaW9u"
    "PSIxLjAiPgo8ZGljdD4KCTxrZXk+Q0ZCdW5kbGVTaG9ydFZlcnNpb25TdHJpbmc8L2tleT4K"
    "CTxzdHJpbmc+MC4wLjA8L3N0cmluZz4KCTxrZXk+Q0ZCdW5kbGVJZGVudGlmaWVyPC9rZXk+"
    "Cgk8c3RyaW5nPmNvbS5leGFtcGxlLnNweWFwcDwvc3RyaW5nPgo8L2RpY3Q+CjwvcGxpc3Q+"
    "ClBLAQIUAxQAAAAAAAAAIVCUj9dtPwEAAD8BAAAeAAAAAAAAAAAAAACkAQAAAABTcHlBcHAu"
    "YXBwL0NvbnRlbnRzL0luZm8ucGxpc3RQSwUGAAAAAAEAAQBMAAAAewEAAAAA";
const char *kZipFixturePublicKeyBase64 = "JbG5JzYqnK5HXvp9pbI0yg9sIRLWJruHbJqkyyi0yeM=";
const char *kZipFixtureEdSignatureBase64 =
    "m2UUF+WS4zgmLsRvwVNA562pw69Xes7bhijF1Ex+WZ589YNVaZoA8fHnMskAlEX2tgDCbFeb"
    "X12oVq4UxzkjCQ==";

UpdateEntry pkgEntry()
{
    UpdateEntry e;
    e.name = QStringLiteral("Spy App");
    e.enclosureUrl = QStringLiteral("https://example.com/updates/spyapp-2.0.0.pkg");
    e.version = QStringLiteral("2.0.0");
    e.installedVersion = QStringLiteral("1.0.0");
    e.bundleId = QStringLiteral("com.example.spyapp");
    e.signatureMetadataPresent = true;
    return e;
}

UpdateEntry zipFixtureEntry()
{
    UpdateEntry e;
    e.name = QStringLiteral("Spy App");
    e.enclosureUrl = QStringLiteral("https://example.com/updates/spyapp-0.0.0.zip");
    e.version = QStringLiteral("0.0.0");
    e.installedVersion.clear(); // the exact fail-open precondition: unreadable/absent local metadata
    e.bundleId = QStringLiteral("com.example.unknownfloor");
    e.signatureMetadataPresent = true;
    e.edSignature = QString::fromLatin1(kZipFixtureEdSignatureBase64);
    e.publicKey = QString::fromLatin1(kZipFixturePublicKeyBase64);
    return e;
}

struct LauncherSpy {
    int callCount = 0;
    SparkleUpdateInstaller::Launcher asLauncher()
    {
        return [this](const QString &) { ++callCount; return true; };
    }
};

struct RevealerSpy {
    int callCount = 0;
    SparkleUpdateInstaller::Revealer asRevealer()
    {
        return [this](const QString &) { ++callCount; return true; };
    }
};

// ── Test-only Ed25519 signing (SSO-17899) ───────────────────────────────────
//
// macos/nexis-core/Info/vendor/ed25519 is deliberately verify-only in
// production (see vendor/ed25519/UPSTREAM.md: signing/keypair/seed files are
// "intentionally not copied in, per Build vs Buy / Blast Radius") — Nexis
// itself never signs anything. But ge_scalarmult_base(), sc_reduce(),
// sc_muladd() and sha512() are already vendored and linked into this test
// binary to support ed25519_verify(), and those four primitives are the
// entire orlp/ed25519 sign.c/keypair.c algorithm. Reimplementing that
// (zlib-licensed, RFC 8032 §5.1.5/§5.1.6) algorithm here — rather than
// vendoring sign.c into production — lets this test produce a genuinely
// valid signature over a synthetic fixture without expanding the
// production verify-only boundary.
struct TestKeyPair {
    QByteArray publicKey;  // 32 bytes
    QByteArray privateKey; // 64 bytes: SHA-512(seed), clamped per RFC 8032 §5.1.5
};

TestKeyPair makeTestKeyPair(const QByteArray &seed)
{
    Q_ASSERT(seed.size() == 32);
    unsigned char priv[64];
    sha512(reinterpret_cast<const unsigned char *>(seed.constData()), 32, priv);
    priv[0] &= 248;
    priv[31] &= 63;
    priv[31] |= 64;

    ge_p3 A;
    ge_scalarmult_base(&A, priv);
    unsigned char pub[32];
    ge_p3_tobytes(pub, &A);

    TestKeyPair kp;
    kp.publicKey = QByteArray(reinterpret_cast<const char *>(pub), 32);
    kp.privateKey = QByteArray(reinterpret_cast<const char *>(priv), 64);
    return kp;
}

QByteArray signWithTestKeyPair(const QByteArray &message, const TestKeyPair &kp)
{
    const auto *priv = reinterpret_cast<const unsigned char *>(kp.privateKey.constData());
    const auto *pub  = reinterpret_cast<const unsigned char *>(kp.publicKey.constData());
    const auto *msg  = reinterpret_cast<const unsigned char *>(message.constData());
    const auto msgLen = static_cast<size_t>(message.size());

    sha512_context hash;
    unsigned char r[64];
    sha512_init(&hash);
    sha512_update(&hash, priv + 32, 32);
    sha512_update(&hash, msg, msgLen);
    sha512_final(&hash, r);
    sc_reduce(r);

    ge_p3 R;
    ge_scalarmult_base(&R, r);
    unsigned char sigR[32];
    ge_p3_tobytes(sigR, &R);

    unsigned char hram[64];
    sha512_init(&hash);
    sha512_update(&hash, sigR, 32);
    sha512_update(&hash, pub, 32);
    sha512_update(&hash, msg, msgLen);
    sha512_final(&hash, hram);
    sc_reduce(hram);

    unsigned char sigS[32];
    sc_muladd(sigS, hram, priv, r);

    QByteArray signature(64, '\0');
    std::memcpy(signature.data(), sigR, 32);
    std::memcpy(signature.data() + 32, sigS, 32);
    return signature;
}

// Builds a genuinely valid, pkgutil-expandable component package via the
// real macOS `pkgbuild` — verifyAndInstall() shells out to `pkgutil --expand`
// on whatever bytes it just verified, so only a real .pkg (not a hand-rolled
// xar) exercises that step honestly.
QByteArray buildRealPkgFixture(const QTemporaryDir &tmp, const QString &identifier,
                               const QString &embeddedVersion)
{
    const QString rootDir = tmp.path() + QStringLiteral("/payload-root");
    if (!QDir().mkpath(rootDir))
        return {};
    const QString outPkg = tmp.path() + QStringLiteral("/fixture.pkg");
    const ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("/usr/bin/pkgbuild"),
        {"--root", rootDir, "--identifier", identifier, "--version", embeddedVersion,
         "--install-location", QStringLiteral("/tmp/nexis-sso-17899-placeholder"), outPkg},
        60000);
    if (!r.ok())
        return {};
    QFile f(outPkg);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

// Builds a real .zip (via `ditto`, the same tool Sparkle release tooling
// uses) containing a single top-level <appName>.app bundle with a synthetic
// Info.plist — verifyAndInstall()'s .zip path unzips with `/usr/bin/unzip`
// and reads CFBundleShortVersionString back out with `/usr/bin/plutil`.
QByteArray buildRealZipFixture(const QTemporaryDir &tmp, const QString &appName,
                               const QString &embeddedVersion)
{
    const QString appBundle = tmp.path() + QLatin1Char('/') + appName + QStringLiteral(".app");
    if (!QDir().mkpath(appBundle + QStringLiteral("/Contents")))
        return {};
    QFile plist(appBundle + QStringLiteral("/Contents/Info.plist"));
    if (!plist.open(QIODevice::WriteOnly))
        return {};
    plist.write(QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n"
        "<key>CFBundleShortVersionString</key><string>%1</string>\n"
        "</dict></plist>\n").arg(embeddedVersion).toUtf8());
    plist.close();

    const QString outZip = tmp.path() + QStringLiteral("/fixture.zip");
    const ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("/usr/bin/ditto"),
        {"-c", "-k", "--sequesterRsrc", "--keepParent", appBundle, outZip},
        60000);
    if (!r.ok())
        return {};
    QFile f(outZip);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

class TestSparkleUpdateInstaller : public QObject
{
    Q_OBJECT

private slots:
    void init();

    // AC4 hard gate.
    void invalidSignature_blocksAndNeverLaunches();
    void missingKey_blocksAndNeverLaunches();
    void missingSignature_blocksAndNeverLaunches();
    void malformedKey_decodingError_blocksAndNeverLaunches();
    void dsaOnly_unsupportedLegacyDsa_blocksAndNeverLaunches();
    void everyNonValidFixture_zeroLauncherInvocationsAcrossTheBoard();

    // AC7 format scope.
    void unsupportedFormat_blocksBeforeVerification_neverLaunches();

    // SSO-17899: the success path — Valid -> Installed/RevealedForManualInstall.
    void validSignature_pkg_installsAndRatchetsFloor();
    void validSignature_zip_revealsAndRatchetsFloor();

    // AC5 floor-version store (pure logic, no subprocess calls).
    void floorStore_initializesOnlyOnce();
    void floorStore_ratchetsForwardOnly();

    // SSO-17898: an unknown/unparseable floor must not skip the §7
    // downgrade/replay check.
    void unknownFloor_defaultsToZero_stillBlocksLowEmbeddedVersion();

    // AC2 (https-only), testable synchronously since the scheme check runs
    // before any network request is issued.
    void downloader_rejectsNonHttpsSynchronously();
};

void TestSparkleUpdateInstaller::init()
{
    // Every test gets an isolated floor-version backing file so runs don't
    // leak state into each other or touch the real Nexis app-support dir.
    static QTemporaryDir *dir = nullptr;
    delete dir;
    dir = new QTemporaryDir();
    QVERIFY(dir->isValid());
    SparkleUpdateFloorStore::setBackingFilePathForTesting(dir->path() + "/floors.ini");
}

void TestSparkleUpdateInstaller::invalidSignature_blocksAndNeverLaunches()
{
    UpdateEntry entry = pkgEntry();
    entry.edSignature = hexToBase64(kSigHex);
    entry.publicKey = hexToBase64(kKeyHex);

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, QByteArrayLiteral("payload bytes the signature was never computed over"),
        launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::Invalid);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedVerificationFailed);
    QVERIFY(result.shouldFallBackToBrowserOpen());
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::missingKey_blocksAndNeverLaunches()
{
    UpdateEntry entry = pkgEntry();
    entry.edSignature = hexToBase64(kSigHex);
    entry.publicKey.clear();

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, QByteArrayLiteral("bytes"), launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::MissingKey);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedVerificationFailed);
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::missingSignature_blocksAndNeverLaunches()
{
    UpdateEntry entry = pkgEntry();
    entry.edSignature.clear();
    entry.dsaSignature.clear();
    entry.publicKey = hexToBase64(kKeyHex);

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, QByteArrayLiteral("bytes"), launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::MissingSignature);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedVerificationFailed);
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::malformedKey_decodingError_blocksAndNeverLaunches()
{
    UpdateEntry entry = pkgEntry();
    entry.edSignature = hexToBase64(kSigHex);
    entry.publicKey = QStringLiteral("not-valid-base64!!!");

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, QByteArrayLiteral("bytes"), launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::DecodingError);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedVerificationFailed);
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::dsaOnly_unsupportedLegacyDsa_blocksAndNeverLaunches()
{
    UpdateEntry entry = pkgEntry();
    entry.edSignature.clear();
    entry.dsaSignature = QStringLiteral("MCwCFDSASIGNATUREBASE64ABCDEFGHIJKL");
    entry.publicKey = hexToBase64(kKeyHex);

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, QByteArrayLiteral("bytes"), launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::UnsupportedLegacyDsa);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedVerificationFailed);
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::everyNonValidFixture_zeroLauncherInvocationsAcrossTheBoard()
{
    // The hard-gate assertion from AC4, stated as a single sweep: across
    // every non-Valid fixture, cumulative launcher/revealer invocations
    // must be exactly zero and every one of them must report a fall-back
    // disposition.
    struct Fixture { QString ed, dsa, key; };
    const QList<Fixture> fixtures = {
        {hexToBase64(kSigHex), {}, {}},                              // MissingKey
        {{}, {}, hexToBase64(kKeyHex)},                               // MissingSignature
        {QStringLiteral("bad!!!"), {}, hexToBase64(kKeyHex)},         // DecodingError
        {hexToBase64(kSigHex), {}, hexToBase64(kKeyHex)},             // Invalid (unsigned payload)
        {{}, QStringLiteral("MCwCFDSASIGNATURE"), hexToBase64(kKeyHex)}, // UnsupportedLegacyDsa
    };

    LauncherSpy launcher;
    RevealerSpy revealer;
    int blockedCount = 0;
    for (const Fixture &fx : fixtures) {
        UpdateEntry entry = pkgEntry();
        entry.edSignature = fx.ed;
        entry.dsaSignature = fx.dsa;
        entry.publicKey = fx.key;
        const auto result = SparkleUpdateInstaller::verifyAndInstall(
            entry, QByteArrayLiteral("arbitrary undownloaded payload"),
            launcher.asLauncher(), revealer.asRevealer());
        QVERIFY(result.verifyResult != Result::Valid);
        QVERIFY(result.shouldFallBackToBrowserOpen());
        ++blockedCount;
    }

    QCOMPARE(blockedCount, fixtures.size());
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::unsupportedFormat_blocksBeforeVerification_neverLaunches()
{
    UpdateEntry entry = pkgEntry();
    entry.enclosureUrl = QStringLiteral("https://example.com/updates/app-2.0.0.dmg");
    // Even a syntactically valid-looking signature/key must not matter —
    // the format gate runs first (§8 gate 7).
    entry.edSignature = hexToBase64(kSigHex);
    entry.publicKey = hexToBase64(kKeyHex);

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, QByteArrayLiteral("bytes"), launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedUnsupportedFormat);
    QVERIFY(result.shouldFallBackToBrowserOpen());
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::validSignature_pkg_installsAndRatchetsFloor()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const TestKeyPair kp = makeTestKeyPair(
        QCryptographicHash::hash(QByteArrayLiteral("SSO-17899-pkg-success-seed"),
                                  QCryptographicHash::Sha256));

    const QString bundleId = QStringLiteral("com.example.spyapp.pkgsuccess");
    const QString embeddedVersion = QStringLiteral("2.0.0");
    const QByteArray pkgBytes = buildRealPkgFixture(tmp, bundleId, embeddedVersion);
    QVERIFY2(!pkgBytes.isEmpty(),
             "pkgbuild fixture failed to build — this suite requires the "
             "macOS command line tools (pkgbuild)");

    const QByteArray signature = signWithTestKeyPair(pkgBytes, kp);

    UpdateEntry entry = pkgEntry();
    entry.bundleId = bundleId;
    entry.installedVersion = QStringLiteral("1.0.0");
    entry.edSignature = QString::fromLatin1(signature.toBase64());
    entry.publicKey = QString::fromLatin1(kp.publicKey.toBase64());

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, pkgBytes, launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::Valid);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::Installed);
    QVERIFY(!result.shouldFallBackToBrowserOpen());
    QCOMPARE(launcher.callCount, 1);
    QCOMPARE(revealer.callCount, 0);
    QCOMPARE(SparkleUpdateFloorStore::floorVersion(bundleId), embeddedVersion);
}

void TestSparkleUpdateInstaller::validSignature_zip_revealsAndRatchetsFloor()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const TestKeyPair kp = makeTestKeyPair(
        QCryptographicHash::hash(QByteArrayLiteral("SSO-17899-zip-success-seed"),
                                  QCryptographicHash::Sha256));

    const QString bundleId = QStringLiteral("com.example.spyapp.zipsuccess");
    const QString embeddedVersion = QStringLiteral("3.0.0");
    const QByteArray zipBytes = buildRealZipFixture(tmp, QStringLiteral("SpyApp"), embeddedVersion);
    QVERIFY2(!zipBytes.isEmpty(),
             "ditto fixture failed to build — this suite requires macOS `ditto`");

    const QByteArray signature = signWithTestKeyPair(zipBytes, kp);

    UpdateEntry entry = pkgEntry();
    entry.enclosureUrl = QStringLiteral("https://example.com/updates/spyapp-3.0.0.zip");
    entry.bundleId = bundleId;
    entry.installedVersion = QStringLiteral("1.0.0");
    entry.edSignature = QString::fromLatin1(signature.toBase64());
    entry.publicKey = QString::fromLatin1(kp.publicKey.toBase64());

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, zipBytes, launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::Valid);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::RevealedForManualInstall);
    QVERIFY(!result.shouldFallBackToBrowserOpen());
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 1);
    QCOMPARE(SparkleUpdateFloorStore::floorVersion(bundleId), embeddedVersion);
}

void TestSparkleUpdateInstaller::floorStore_initializesOnlyOnce()
{
    const QString bundleId = QStringLiteral("com.example.floorstore");
    QVERIFY(SparkleUpdateFloorStore::floorVersion(bundleId).isEmpty());

    SparkleUpdateFloorStore::initializeIfAbsent(bundleId, QStringLiteral("1.0.0"));
    QCOMPARE(SparkleUpdateFloorStore::floorVersion(bundleId), QStringLiteral("1.0.0"));

    // Second call must not overwrite an already-recorded floor.
    SparkleUpdateFloorStore::initializeIfAbsent(bundleId, QStringLiteral("9.9.9"));
    QCOMPARE(SparkleUpdateFloorStore::floorVersion(bundleId), QStringLiteral("1.0.0"));
}

void TestSparkleUpdateInstaller::floorStore_ratchetsForwardOnly()
{
    const QString bundleId = QStringLiteral("com.example.ratchet");
    SparkleUpdateFloorStore::initializeIfAbsent(bundleId, QStringLiteral("1.0.0"));

    // A lower "verified" version must never move the floor backward — this
    // is the exact self-inflicted-DoS scenario design doc §7.3 warns about
    // if ratcheting were driven by anything other than a real verified
    // install of a higher version.
    SparkleUpdateFloorStore::ratchetAfterVerifiedInstall(bundleId, QStringLiteral("0.5.0"));
    QCOMPARE(SparkleUpdateFloorStore::floorVersion(bundleId), QStringLiteral("1.0.0"));

    SparkleUpdateFloorStore::ratchetAfterVerifiedInstall(bundleId, QStringLiteral("2.0.0"));
    QCOMPARE(SparkleUpdateFloorStore::floorVersion(bundleId), QStringLiteral("2.0.0"));
}

void TestSparkleUpdateInstaller::unknownFloor_defaultsToZero_stillBlocksLowEmbeddedVersion()
{
    // Reproduces the SSO-17898 gap: entry.installedVersion is empty (as it
    // would be if Nexis couldn't read the installed app's
    // CFBundleShortVersionString), so initializeIfAbsent() never seeds a
    // floor and floorVersion() stays empty — QVersionNumber::fromString("")
    // and QVersionNumber::fromString(<garbage>) both produce the same null
    // QVersionNumber, so this exercises the same defaulting branch an
    // unparseable stored floor would. Before the fix, the null floor made
    // the downgrade check skip itself entirely (BlockedDowngrade never
    // fired, and the .zip was revealed to the user); after the fix, the
    // unknown floor defaults to version 0 and this low (but real, verified)
    // embedded version ("0.0.0") is still caught.
    UpdateEntry entry = zipFixtureEntry();
    const QByteArray zipBytes = QByteArray::fromBase64(QByteArray(kZipFixtureBase64));
    QVERIFY(!zipBytes.isEmpty());
    QVERIFY(SparkleUpdateFloorStore::floorVersion(entry.bundleId).isEmpty());

    LauncherSpy launcher;
    RevealerSpy revealer;
    const auto result = SparkleUpdateInstaller::verifyAndInstall(
        entry, zipBytes, launcher.asLauncher(), revealer.asRevealer());

    QCOMPARE(result.verifyResult, Result::Valid);
    QCOMPARE(result.outcome, SparkleUpdateInstaller::Outcome::BlockedDowngrade);
    QVERIFY(result.shouldFallBackToBrowserOpen());
    QCOMPARE(launcher.callCount, 0);
    QCOMPARE(revealer.callCount, 0);
}

void TestSparkleUpdateInstaller::downloader_rejectsNonHttpsSynchronously()
{
    SparkleUpdateDownloader downloader;
    SparkleUpdateDownloader::Result captured;
    bool gotSignal = false;
    connect(&downloader, &SparkleUpdateDownloader::finished, this,
            [&](SparkleUpdateDownloader::Result r) { captured = r; gotSignal = true; });

    downloader.start(QStringLiteral("http://example.com/insecure-update.pkg"));

    // The scheme check runs before any request, so finished() has already
    // fired synchronously (direct connection, same thread) by this point.
    QVERIFY(gotSignal);
    QVERIFY(!captured.ok);
    QVERIFY(!captured.cancelled);
    QVERIFY(captured.data.isEmpty());
}

QTEST_MAIN(TestSparkleUpdateInstaller)
#include "test_sparkle_update_installer.moc"
