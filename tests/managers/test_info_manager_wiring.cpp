// WI-27 / SSO-3389: ensure InfoManager exposes platform-agnostic providers
// for every Info subclass we wire through it. Pages should reach these via
// the facade — never by stack-constructing *Linux / *MacOS subclasses.
//
// This test is intentionally minimal: it only asserts that the providers
// the facade newly owns (BootAnalysisInfo, StartupInfo) return non-null
// from the matching getter. We don't exercise the analyze() / getStartupApps()
// paths here — those have their own per-provider behavior tests and would
// require sandbox-friendly fixtures to run reliably in CI.

#include <QtTest/QtTest>
#include <QCoreApplication>

#include "Managers/info_manager.h"
#include <Info/boot_analysis_info.h>
#include <Info/startup_info.h>

class TestInfoManagerWiring : public QObject
{
    Q_OBJECT

private slots:
    void bootAnalysisInfoProviderIsWired()
    {
        InfoManager *im = InfoManager::ins();
        QVERIFY2(im != nullptr, "InfoManager::ins() returned null");

        BootAnalysisInfo *bai = im->bootAnalysisInfo();
        QVERIFY2(bai != nullptr,
                 "InfoManager::bootAnalysisInfo() must return a non-null provider");
    }

    void startupInfoProviderIsWired()
    {
        InfoManager *im = InfoManager::ins();
        QVERIFY2(im != nullptr, "InfoManager::ins() returned null");

        StartupInfo *sui = im->startupInfo();
        QVERIFY2(sui != nullptr,
                 "InfoManager::startupInfo() must return a non-null provider");
    }

    void singletonReturnsStableProviders()
    {
        // Repeated calls should hand back the same provider instances —
        // they're owned by the singleton and pages may keep raw pointers.
        InfoManager *im = InfoManager::ins();
        QCOMPARE(im->bootAnalysisInfo(), im->bootAnalysisInfo());
        QCOMPARE(im->startupInfo(),      im->startupInfo());
    }
};

QTEST_MAIN(TestInfoManagerWiring)
#include "test_info_manager_wiring.moc"
