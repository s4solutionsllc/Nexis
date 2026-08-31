// SSO-23866: unit tests for CacheRebuildWidget's pure command-construction
// and macOS-version gating logic. No live macOS rebuild required — these
// exercise only the static, platform-independent functions.

#include <QtTest>
#include <Pages/Helpers/cache_rebuild_widget.h>

using Action = CacheRebuildWidget::Action;

namespace {
QOperatingSystemVersion macOs(int major, int minor)
{
    return QOperatingSystemVersion(QOperatingSystemVersion::MacOS, major, minor);
}
} // namespace

class TestCacheRebuildWidget : public QObject
{
    Q_OBJECT

private slots:
    // --- commandsFor ---

    void commands_dyldSharedCache()
    {
        const auto steps = CacheRebuildWidget::commandsFor(Action::DyldSharedCache);
        QCOMPARE(steps.size(), 1);
        QCOMPARE(steps[0].cmd, QStringLiteral("update_dyld_shared_cache"));
        QCOMPARE(steps[0].args, QStringList{"-force"});
        QVERIFY(steps[0].needsSudo);
    }

    void commands_xpcCache()
    {
        const auto steps = CacheRebuildWidget::commandsFor(Action::XpcCache);
        QCOMPARE(steps.size(), 1);
        QCOMPARE(steps[0].cmd, QStringLiteral("rm"));
        QCOMPARE(steps[0].args, (QStringList{"-f", "/var/db/xpcd/xpcd_cache.dylib"}));
        QVERIFY(steps[0].needsSudo);
    }

    void commands_fontCache()
    {
        const auto steps = CacheRebuildWidget::commandsFor(Action::FontCache);
        QCOMPARE(steps.size(), 3);

        QCOMPARE(steps[0].cmd, QStringLiteral("atsutil"));
        QCOMPARE(steps[0].args, (QStringList{"databases", "-remove"}));
        QVERIFY(steps[0].needsSudo);

        QCOMPARE(steps[1].cmd, QStringLiteral("atsutil"));
        QCOMPARE(steps[1].args, (QStringList{"server", "-shutdown"}));
        QVERIFY(!steps[1].needsSudo);

        QCOMPARE(steps[2].cmd, QStringLiteral("atsutil"));
        QCOMPARE(steps[2].args, (QStringList{"server", "-ping"}));
        QVERIFY(!steps[2].needsSudo);
    }

    void commands_launchpadReset()
    {
        const auto steps = CacheRebuildWidget::commandsFor(Action::LaunchpadReset);
        QCOMPARE(steps.size(), 2);

        QCOMPARE(steps[0].cmd, QStringLiteral("defaults"));
        QCOMPARE(steps[0].args,
                 (QStringList{"write", "com.apple.dock", "ResetLaunchPad", "-bool", "true"}));
        QVERIFY(!steps[0].needsSudo);

        QCOMPARE(steps[1].cmd, QStringLiteral("killall"));
        QCOMPARE(steps[1].args, QStringList{"Dock"});
        QVERIFY(!steps[1].needsSudo);
    }

    void commands_allStepsHaveDescriptions()
    {
        for (Action a : {Action::DyldSharedCache, Action::XpcCache,
                         Action::FontCache, Action::LaunchpadReset}) {
            for (const auto &step : CacheRebuildWidget::commandsFor(a))
                QVERIFY(!step.description.isEmpty());
        }
    }

    // --- supportInfo: dyld shared cache ---
    // Unsupported from Big Sur (11.0) on — sealed system volume removed the
    // update_dyld_shared_cache binary and the manual rebuild path.

    void support_dyld_catalina_available()
    {
        QVERIFY(CacheRebuildWidget::supportInfo(Action::DyldSharedCache, macOs(10, 15)).available);
    }

    void support_dyld_bigSur_unavailable()
    {
        const auto info = CacheRebuildWidget::supportInfo(Action::DyldSharedCache, macOs(11, 0));
        QVERIFY(!info.available);
        QVERIFY(!info.reason.isEmpty());
    }

    void support_dyld_sonoma_unavailable()
    {
        QVERIFY(!CacheRebuildWidget::supportInfo(Action::DyldSharedCache, macOs(14, 0)).available);
    }

    // --- supportInfo: XPC cache ---
    // Unsupported from Yosemite (10.10) on — the on-disk xpcd cache was
    // retired and macOS resolves services directly.

    void support_xpc_mavericks_available()
    {
        QVERIFY(CacheRebuildWidget::supportInfo(Action::XpcCache, macOs(10, 9)).available);
    }

    void support_xpc_yosemite_unavailable()
    {
        const auto info = CacheRebuildWidget::supportInfo(Action::XpcCache, macOs(10, 10));
        QVERIFY(!info.available);
        QVERIFY(!info.reason.isEmpty());
    }

    void support_xpc_bigSur_unavailable()
    {
        QVERIFY(!CacheRebuildWidget::supportInfo(Action::XpcCache, macOs(11, 0)).available);
    }

    // --- supportInfo: font cache + Launchpad reset are supported everywhere ---

    void support_fontCache_alwaysAvailable()
    {
        QVERIFY(CacheRebuildWidget::supportInfo(Action::FontCache, macOs(10, 9)).available);
        QVERIFY(CacheRebuildWidget::supportInfo(Action::FontCache, macOs(11, 0)).available);
        QVERIFY(CacheRebuildWidget::supportInfo(Action::FontCache, macOs(14, 0)).available);
    }

    void support_launchpadReset_alwaysAvailable()
    {
        QVERIFY(CacheRebuildWidget::supportInfo(Action::LaunchpadReset, macOs(10, 9)).available);
        QVERIFY(CacheRebuildWidget::supportInfo(Action::LaunchpadReset, macOs(11, 0)).available);
        QVERIFY(CacheRebuildWidget::supportInfo(Action::LaunchpadReset, macOs(14, 0)).available);
    }

    // --- display text ---

    void displayText_nonEmptyForAllActions()
    {
        for (Action a : {Action::DyldSharedCache, Action::XpcCache,
                         Action::FontCache, Action::LaunchpadReset}) {
            QVERIFY(!CacheRebuildWidget::actionTitle(a).isEmpty());
            QVERIFY(!CacheRebuildWidget::actionDescription(a).isEmpty());
            QVERIFY(!CacheRebuildWidget::confirmText(a).isEmpty());
        }
    }
};

QTEST_MAIN(TestCacheRebuildWidget)
#include "test_cache_rebuild_widget.moc"
