#include <QTest>
#include "Tools/package_tool_shared.h"

class TestPackageTool : public QObject
{
    Q_OBJECT

private slots:
    // parseSnapListAll
    void snap_twoDisabledRevisions();
    void snap_noDisabledRevisions();
    void snap_emptyOutput();
    void snap_mixedActiveAndDisabled();

    // parseAptAutoremoveDryRun
    void apt_multipleOrphans();
    void apt_noOrphans();
    void apt_emptyOutput();

    // parsePacmanOrphans
    void pacman_multipleOrphans();
    void pacman_singleOrphan();
    void pacman_emptyOutput();

    // parseDnfAutoremoveDryRun
    void dnf_multipleOrphans();
    void dnf_noOrphans();
    void dnf_emptyOutput();
    void dnf_removingUnusedDeps();

    // parseBrewAutoremoveDryRun
    void brew_multipleOrphans();
    void brew_noOrphans();
    void brew_emptyOutput();
};

// ── snap list --all fixtures ──────────────────────────────────────────────────

static const char *kSnapTwoDisabled =
    "Name      Version    Rev   Tracking       Publisher   Notes\n"
    "core20    20240227   2182  latest/stable  canonical✓  -\n"
    "firefox   130.0      4259  latest/stable  mozilla✓    -\n"
    "firefox   129.0.1    4173  latest/stable  mozilla✓    disabled\n"
    "gnome-42  42.9       172   latest/stable  canonical✓  -\n"
    "gnome-42  42.8       168   latest/stable  canonical✓  disabled\n";

static const char *kSnapNoDisabled =
    "Name      Version    Rev   Tracking       Publisher   Notes\n"
    "core20    20240227   2182  latest/stable  canonical✓  -\n"
    "firefox   130.0      4259  latest/stable  mozilla✓    -\n";

static const char *kSnapMixed =
    "Name      Version    Rev   Tracking       Publisher   Notes\n"
    "core20    20240227   2182  latest/stable  canonical✓  -\n"
    "core20    20240101   2100  latest/stable  canonical✓  disabled\n"
    "firefox   130.0      4259  latest/stable  mozilla✓    -\n";

void TestPackageTool::snap_twoDisabledRevisions()
{
    QList<StaleSnapRevision> result = PackageTool::parseSnapListAll(kSnapTwoDisabled);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].name, "firefox");
    QCOMPARE(result[0].revision, "4173");
    QCOMPARE(result[0].filePath, "/var/lib/snapd/snaps/firefox_4173.snap");
    QCOMPARE(result[1].name, "gnome-42");
    QCOMPARE(result[1].revision, "168");
}

void TestPackageTool::snap_noDisabledRevisions()
{
    QList<StaleSnapRevision> result = PackageTool::parseSnapListAll(kSnapNoDisabled);
    QCOMPARE(result.size(), 0);
}

void TestPackageTool::snap_emptyOutput()
{
    QList<StaleSnapRevision> result = PackageTool::parseSnapListAll("");
    QCOMPARE(result.size(), 0);
}

void TestPackageTool::snap_mixedActiveAndDisabled()
{
    QList<StaleSnapRevision> result = PackageTool::parseSnapListAll(kSnapMixed);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].name, "core20");
    QCOMPARE(result[0].revision, "2100");
}

// ── apt-get autoremove --dry-run fixtures ─────────────────────────────────────

static const char *kAptMultipleOrphans =
    "Reading package lists... Done\n"
    "Building dependency tree... Done\n"
    "Reading state information... Done\n"
    "The following packages will be REMOVED:\n"
    "  libfoo-dev libbar-utils libqux\n"
    "0 upgraded, 0 newly installed, 3 to remove and 0 not upgraded.\n"
    "Remv libfoo-dev [1.2.3-1]\n"
    "Remv libbar-utils [2.0.0-1ubuntu1]\n"
    "Remv libqux [0.9.1-3]\n";

static const char *kAptNoOrphans =
    "Reading package lists... Done\n"
    "Building dependency tree... Done\n"
    "Reading state information... Done\n"
    "0 upgraded, 0 newly installed, 0 to remove and 5 not upgraded.\n";

void TestPackageTool::apt_multipleOrphans()
{
    QList<OrphanPackage> result = PackageTool::parseAptAutoremoveDryRun(kAptMultipleOrphans);
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0].name, "libfoo-dev");
    QCOMPARE(result[1].name, "libbar-utils");
    QCOMPARE(result[2].name, "libqux");
}

void TestPackageTool::apt_noOrphans()
{
    QList<OrphanPackage> result = PackageTool::parseAptAutoremoveDryRun(kAptNoOrphans);
    QCOMPARE(result.size(), 0);
}

void TestPackageTool::apt_emptyOutput()
{
    QList<OrphanPackage> result = PackageTool::parseAptAutoremoveDryRun("");
    QCOMPARE(result.size(), 0);
}

// ── pacman -Qdtq fixtures ────────────────────────────────────────────────────

static const char *kPacmanMultipleOrphans =
    "lib32-gcc-libs\n"
    "python-setuptools\n"
    "xorg-xrandr\n";

void TestPackageTool::pacman_multipleOrphans()
{
    QList<OrphanPackage> result = PackageTool::parsePacmanOrphans(kPacmanMultipleOrphans);
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0].name, "lib32-gcc-libs");
    QCOMPARE(result[1].name, "python-setuptools");
    QCOMPARE(result[2].name, "xorg-xrandr");
}

void TestPackageTool::pacman_singleOrphan()
{
    QList<OrphanPackage> result = PackageTool::parsePacmanOrphans("unused-pkg");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].name, "unused-pkg");
}

void TestPackageTool::pacman_emptyOutput()
{
    QList<OrphanPackage> result = PackageTool::parsePacmanOrphans("");
    QCOMPARE(result.size(), 0);
}

// ── dnf autoremove --assumeno fixtures ────────────────────────────────────────

static const char *kDnfMultipleOrphans =
    "Dependencies resolved.\n"
    "================================================================================\n"
    " Package              Arch       Version              Repository           Size\n"
    "================================================================================\n"
    "Removing:\n"
    " libfoo               x86_64     1.0-1.fc39           @updates             100 k\n"
    " libbar               x86_64     2.0-1.fc39           @fedora               50 k\n"
    "\n"
    "Transaction Summary\n"
    "================================================================================\n"
    "Remove  2 Packages\n"
    "\n"
    "Operation aborted.\n";

static const char *kDnfNoOrphans =
    "Dependencies resolved.\n"
    "Nothing to do.\n";

static const char *kDnfUnusedDeps =
    "Dependencies resolved.\n"
    "================================================================================\n"
    " Package              Arch       Version              Repository           Size\n"
    "================================================================================\n"
    "Removing unused dependencies:\n"
    " orphan-lib           x86_64     3.0-1.fc39           @updates             200 k\n"
    " orphan-utils         noarch     1.2-1.fc39           @fedora               30 k\n"
    "\n"
    "Transaction Summary\n"
    "================================================================================\n"
    "Remove  2 Packages\n"
    "\n"
    "Operation aborted.\n";

void TestPackageTool::dnf_multipleOrphans()
{
    QList<OrphanPackage> result = PackageTool::parseDnfAutoremoveDryRun(kDnfMultipleOrphans);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].name, "libfoo");
    QCOMPARE(result[1].name, "libbar");
}

void TestPackageTool::dnf_noOrphans()
{
    QList<OrphanPackage> result = PackageTool::parseDnfAutoremoveDryRun(kDnfNoOrphans);
    QCOMPARE(result.size(), 0);
}

void TestPackageTool::dnf_emptyOutput()
{
    QList<OrphanPackage> result = PackageTool::parseDnfAutoremoveDryRun("");
    QCOMPARE(result.size(), 0);
}

void TestPackageTool::dnf_removingUnusedDeps()
{
    QList<OrphanPackage> result = PackageTool::parseDnfAutoremoveDryRun(kDnfUnusedDeps);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].name, "orphan-lib");
    QCOMPARE(result[1].name, "orphan-utils");
}

// ── brew autoremove --dry-run fixtures ────────────────────────────────────────

static const char *kBrewMultipleOrphans =
    "==> Would uninstall:\n"
    "libpng\n"
    "jpeg-turbo\n"
    "pcre2\n";

static const char *kBrewNoOrphans =
    "==> No unused formulae to uninstall.\n";

void TestPackageTool::brew_multipleOrphans()
{
    QList<OrphanPackage> result = PackageTool::parseBrewAutoremoveDryRun(kBrewMultipleOrphans);
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0].name, "libpng");
    QCOMPARE(result[1].name, "jpeg-turbo");
    QCOMPARE(result[2].name, "pcre2");
}

void TestPackageTool::brew_noOrphans()
{
    QList<OrphanPackage> result = PackageTool::parseBrewAutoremoveDryRun(kBrewNoOrphans);
    QCOMPARE(result.size(), 0);
}

void TestPackageTool::brew_emptyOutput()
{
    QList<OrphanPackage> result = PackageTool::parseBrewAutoremoveDryRun("");
    QCOMPARE(result.size(), 0);
}

QTEST_MAIN(TestPackageTool)
#include "test_package_tool.moc"
