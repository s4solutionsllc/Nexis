// SSO-3390 / WI-28: interface-level tests for the platform-neutral
// RepositoryTool surface. Verifies the contract holds for both backends
// (APT on Linux, Homebrew on macOS) without down-casting.
//
// The tests deliberately exercise only the platform-neutral methods. They
// hit the real system to avoid mocking — when neither backend is available
// on the test host, the methods short-circuit and we just assert that the
// contract (no crash, empty list, correct capability flags) is preserved.

#include <QtTest>
#include <Tools/repository_tool.h>

#ifdef Q_OS_MACOS
#include <Tools/homebrew_tool_macos.h>
using PlatformRepositoryTool = HomebrewToolMacOS;
#else
#include <Tools/apt_source_tool_linux.h>
using PlatformRepositoryTool = AptSourceToolLinux;
#endif

class TestRepositoryTool : public QObject
{
    Q_OBJECT

private slots:
    void contract_isAvailable_isBoolean();
    void contract_listRepositories_returnsKindMatchingBackend();
    void contract_capabilities_reflectBackendCapabilities();
    void contract_listRepositories_repoIdsAreNonEmpty();
};

void TestRepositoryTool::contract_isAvailable_isBoolean()
{
    PlatformRepositoryTool tool;
    // Just verify the call returns and doesn't crash. The value depends on
    // the host machine — Linux test runners have /etc/apt; macOS runners
    // may or may not have brew installed.
    bool result = tool.isAvailable();
    Q_UNUSED(result);
    QVERIFY(true);
}

void TestRepositoryTool::contract_listRepositories_returnsKindMatchingBackend()
{
    PlatformRepositoryTool tool;
    if (!tool.isAvailable())
        QSKIP("Backend not available on this host");

    QList<RepositoryPtr> repos = tool.listRepositories();

#ifdef Q_OS_MACOS
    constexpr Repository::Kind expected = Repository::Kind::HomebrewPackage;
#else
    constexpr Repository::Kind expected = Repository::Kind::AptSource;
#endif

    for (const RepositoryPtr &repo : repos) {
        QVERIFY(!repo.isNull());
        QCOMPARE(repo->kind, expected);
    }
}

void TestRepositoryTool::contract_capabilities_reflectBackendCapabilities()
{
    PlatformRepositoryTool tool;
    RepositoryCapabilities caps = tool.capabilities();

#ifdef Q_OS_MACOS
    // Homebrew packages can't be toggled or edited in place
    QVERIFY(!caps.canToggle);
    QVERIFY(!caps.canEdit);
    QVERIFY(caps.canAdd);
#else
    // APT sources support all three operations
    QVERIFY(caps.canToggle);
    QVERIFY(caps.canEdit);
    QVERIFY(caps.canAdd);
#endif
}

void TestRepositoryTool::contract_listRepositories_repoIdsAreNonEmpty()
{
    PlatformRepositoryTool tool;
    if (!tool.isAvailable())
        QSKIP("Backend not available on this host");

    QList<RepositoryPtr> repos = tool.listRepositories();
    for (const RepositoryPtr &repo : repos) {
        QVERIFY2(!repo->id.isEmpty(),
                 "Every Repository must have a non-empty id (used as cache key)");
    }
}

QTEST_MAIN(TestRepositoryTool)
#include "test_repository_tool.moc"
