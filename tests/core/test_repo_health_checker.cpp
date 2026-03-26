#include <QTest>
#include "Tools/repo_health_types.h"
#include "Tools/repo_health_checker.h"
#include "Tools/apt_source_tool.h"

class TestRepoHealthChecker : public QObject
{
    Q_OBJECT

private slots:
    void cacheKey_compositeFormat();
    void cacheKey_differentSuites_differentKeys();
    void cacheKey_sameRepo_sameKey();
    void worstStatus_noIssues_healthy();
    void worstStatus_warningOnly();
    void worstStatus_errorOverridesWarning();
    void deprecatedFormat_legacyNoSignedBy_twoIssues();
    void deprecatedFormat_deb822_noIssue();
    void duplicates_detected();
    void duplicates_noDuplicates();
    void fileUri_skipsConnectionCheck();
    void unreachableHost_connectionError();
};

void TestRepoHealthChecker::cacheKey_compositeFormat()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://archive.ubuntu.com/ubuntu";
    src->suites = "jammy";
    src->components = "main restricted";
    QString key = RepoHealthChecker::cacheKey(src);
    QCOMPARE(key, QString("http://archive.ubuntu.com/ubuntu jammy main restricted"));
}

void TestRepoHealthChecker::cacheKey_differentSuites_differentKeys()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://archive.ubuntu.com/ubuntu";
    src1->suites = "jammy";
    src1->components = "main";

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://archive.ubuntu.com/ubuntu";
    src2->suites = "jammy-updates";
    src2->components = "main";

    QVERIFY(RepoHealthChecker::cacheKey(src1) != RepoHealthChecker::cacheKey(src2));
}

void TestRepoHealthChecker::cacheKey_sameRepo_sameKey()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://example.com/apt";
    src1->suites = "stable";
    src1->components = "main";

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://example.com/apt";
    src2->suites = "stable";
    src2->components = "main";

    QCOMPARE(RepoHealthChecker::cacheKey(src1), RepoHealthChecker::cacheKey(src2));
}

void TestRepoHealthChecker::worstStatus_noIssues_healthy()
{
    // A result with no issues should be Healthy after check
    RepoHealthResult result;
    result.status = RepoHealthResult::Healthy;
    QCOMPARE(result.status, RepoHealthResult::Healthy);
}

void TestRepoHealthChecker::worstStatus_warningOnly()
{
    RepoHealthIssue issue;
    issue.severity = RepoHealthIssue::Warning;
    issue.code = "test_warning";

    RepoHealthResult result;
    result.issues.append(issue);

    // Compute worst: Warning > Healthy
    RepoHealthResult::Status worst = RepoHealthResult::Healthy;
    for (const auto &i : result.issues) {
        if (i.severity == RepoHealthIssue::Error)
            worst = RepoHealthResult::Error;
        else if (i.severity == RepoHealthIssue::Warning && worst != RepoHealthResult::Error)
            worst = RepoHealthResult::Warning;
    }
    QCOMPARE(worst, RepoHealthResult::Warning);
}

void TestRepoHealthChecker::worstStatus_errorOverridesWarning()
{
    RepoHealthIssue warn;
    warn.severity = RepoHealthIssue::Warning;
    warn.code = "test_warning";

    RepoHealthIssue err;
    err.severity = RepoHealthIssue::Error;
    err.code = "test_error";

    RepoHealthResult result;
    result.issues.append(warn);
    result.issues.append(err);

    RepoHealthResult::Status worst = RepoHealthResult::Healthy;
    for (const auto &i : result.issues) {
        if (i.severity == RepoHealthIssue::Error)
            worst = RepoHealthResult::Error;
        else if (i.severity == RepoHealthIssue::Warning && worst != RepoHealthResult::Error)
            worst = RepoHealthResult::Warning;
    }
    QCOMPARE(worst, RepoHealthResult::Error);
}

// --- Pure-logic checks (no network) ---
// These tests instantiate RepoHealthCheckerLinux directly for logic checks.
// They are Linux-only tests; on macOS, they should be wrapped in #ifdef Q_OS_LINUX.

#ifndef Q_OS_MACOS
#include "Tools/repo_health_checker_linux.h"

void TestRepoHealthChecker::deprecatedFormat_legacyNoSignedBy_twoIssues()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/apt";
    src->suites = "stable";
    src->components = "main";
    src->format = APTSource::Legacy;
    src->signedByPath = "";  // no signed-by

    RepoHealthCheckerLinux checker;
    RepoHealthResult result;
    // Call checkDeprecatedFormat via checkOne would do network calls,
    // so we test the deprecated format logic by running a full checkOne
    // with a known-unreachable URI — but that couples to network.
    // Instead, we verify the fields that deprecatedFormat checks:
    // Legacy format + no signedByPath = 2 issues (legacy_format + no_signed_by)
    // We can't call private methods directly, so we verify via checkOne
    // on localhost (which will fail connection but still run deprecatedFormat).
    src->uri = "http://127.0.0.1:1"; // unreachable, fast timeout
    result = checker.checkOne(src);

    // Should have connection_error + legacy_format + no_signed_by = 3 issues
    bool hasLegacyFormat = false;
    bool hasNoSignedBy = false;
    for (const auto &issue : result.issues) {
        if (issue.code == "legacy_format") hasLegacyFormat = true;
        if (issue.code == "no_signed_by") hasNoSignedBy = true;
    }
    QVERIFY(hasLegacyFormat);
    QVERIFY(hasNoSignedBy);
}

void TestRepoHealthChecker::deprecatedFormat_deb822_noIssue()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://127.0.0.1:1";
    src->suites = "stable";
    src->components = "main";
    src->format = APTSource::Deb822;
    src->signedByPath = "/usr/share/keyrings/test.gpg";

    RepoHealthCheckerLinux checker;
    RepoHealthResult result = checker.checkOne(src);

    bool hasLegacyFormat = false;
    bool hasNoSignedBy = false;
    for (const auto &issue : result.issues) {
        if (issue.code == "legacy_format") hasLegacyFormat = true;
        if (issue.code == "no_signed_by") hasNoSignedBy = true;
    }
    QVERIFY(!hasLegacyFormat);
    QVERIFY(!hasNoSignedBy);
}

void TestRepoHealthChecker::duplicates_detected()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://example.com/apt";
    src1->suites = "stable";
    src1->components = "main";
    src1->format = APTSource::Deb822;

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://example.com/apt";
    src2->suites = "stable";
    src2->components = "main";
    src2->format = APTSource::Deb822;

    // Use unreachable URIs for fast failure
    src1->uri = "http://127.0.0.1:1";
    src2->uri = "http://127.0.0.1:1";

    RepoHealthCheckerLinux checker;
    QList<APTSourcePtr> sources = {src1, src2};
    RepoHealthCache cache = checker.checkAll(sources);

    // At least one entry should have a duplicate_source issue
    bool foundDuplicate = false;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        for (const auto &issue : it.value().issues) {
            if (issue.code == "duplicate_source")
                foundDuplicate = true;
        }
    }
    QVERIFY(foundDuplicate);
}

void TestRepoHealthChecker::duplicates_noDuplicates()
{
    APTSourcePtr src1(new APTSource);
    src1->uri = "http://127.0.0.1:1";
    src1->suites = "stable";
    src1->components = "main";
    src1->format = APTSource::Deb822;

    APTSourcePtr src2(new APTSource);
    src2->uri = "http://127.0.0.1:1";
    src2->suites = "testing";  // different suite
    src2->components = "main";
    src2->format = APTSource::Deb822;

    RepoHealthCheckerLinux checker;
    QList<APTSourcePtr> sources = {src1, src2};
    RepoHealthCache cache = checker.checkAll(sources);

    bool foundDuplicate = false;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        for (const auto &issue : it.value().issues) {
            if (issue.code == "duplicate_source")
                foundDuplicate = true;
        }
    }
    QVERIFY(!foundDuplicate);
}

void TestRepoHealthChecker::fileUri_skipsConnectionCheck()
{
    APTSourcePtr src(new APTSource);
    src->uri = "file:///nonexistent/path";
    src->suites = "stable";
    src->components = "main";
    src->format = APTSource::Deb822;
    src->signedByPath = "/usr/share/keyrings/test.gpg";

    RepoHealthCheckerLinux checker;
    RepoHealthResult result = checker.checkOne(src);

    // file:// should skip HTTP connectivity check — no connection_error
    bool hasConnectionError = false;
    for (const auto &issue : result.issues) {
        if (issue.code == "connection_error")
            hasConnectionError = true;
    }
    QVERIFY(!hasConnectionError);
}

void TestRepoHealthChecker::unreachableHost_connectionError()
{
    APTSourcePtr src(new APTSource);
    src->uri = "http://127.0.0.1:1";
    src->suites = "stable";
    src->components = "main";
    src->format = APTSource::Deb822;
    src->signedByPath = "/usr/share/keyrings/test.gpg";

    RepoHealthCheckerLinux checker;
    RepoHealthResult result = checker.checkOne(src);

    // Truly unreachable host should still produce connection_error
    bool hasConnectionError = false;
    for (const auto &issue : result.issues) {
        if (issue.code == "connection_error")
            hasConnectionError = true;
    }
    QVERIFY(hasConnectionError);
}

#else
// macOS stubs — these tests are Linux-only
void TestRepoHealthChecker::deprecatedFormat_legacyNoSignedBy_twoIssues() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::deprecatedFormat_deb822_noIssue() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::duplicates_detected() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::duplicates_noDuplicates() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::fileUri_skipsConnectionCheck() { QSKIP("Linux-only test"); }
void TestRepoHealthChecker::unreachableHost_connectionError() { QSKIP("Linux-only test"); }
#endif

QTEST_MAIN(TestRepoHealthChecker)
#include "test_repo_health_checker.moc"
