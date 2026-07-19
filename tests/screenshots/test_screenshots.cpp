#include <QTest>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QRegion>
#include <QSet>
#include <QStackedWidget>
#include <QScreen>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QStringList>

#include "app.h"
#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"

// Per-channel fuzz: pixels whose R/G/B/A all differ by ≤ this value count as
// equal. Tolerates anti-aliasing + minor font-rendering drift without letting
// real colour regressions slip through.
static constexpr int kChannelFuzz = 8;

// Default whole-page tolerance, measured against *unmasked* pixels. Dynamic
// regions (charts, process tables, live counters) are masked away first, so
// the remaining surface is the stable chrome — we expect near-pixel-perfect
// matches there.
static constexpr double kDefaultTolerance = 1.0;

struct CompareResult {
    bool passed = false;
    double diffPercent = 0.0;
    int diffPixels = 0;
    int comparedPixels = 0;   // total - masked
    int maskedPixels = 0;
    QImage diffImage;
};

struct PageInfo {
    QString className;
    QString screenshotName;
    // Child widget classes (matched via QObject::inherits) whose on-screen
    // rectangles are masked out before comparison.
    QStringList dynamicClassNames;
    // Child widget objectNames whose rectangles are masked out.
    QStringList dynamicObjectNames;
};

// Per-page declaration of which child widgets render live data and should be
// masked. Anything not listed here is expected to be byte-stable (modulo
// per-channel fuzz) between the reference capture and the comparison run.
//
// This is the set used by the default CI regression suite (ctest -R
// ScreenshotTests) and by scripts/update_screenshots.sh — it must stay in
// sync with tests/reference_screenshots/. Round-2/one-off capture pages that
// aren't part of the maintained baseline set go in kRound2PageMap instead
// (see buildPageMap()), so a runtime-gated page (Docker/GNOME Settings not
// available on every host) can never abort this default suite.
static const QVector<PageInfo> kBasePageMap = {
    {"DashboardPage",     "dashboard",
        {"DashboardTileWrapper", "MetricTileBase", "NetworkTile"},
        {"systemSummary", "lblFooterRight"}},
    {"HardwareInfoPage",  "hardware_info",     {}, {}},
    {"StartupAppsPage",   "startup_apps",      {"QAbstractItemView"}, {}},
    {"SystemCleanerPage", "system_cleaner",    {}, {}},
    {"SearchPage",        "search",            {}, {}},
    {"ServicesPage",      "services",          {"QAbstractItemView"}, {}},
    {"ProcessesPage",     "processes",         {"QAbstractItemView"}, {}},
    {"UninstallerPage",   "uninstaller",       {"QAbstractItemView"}, {}},
    // SSO-3737 / FW-09: DiskUsageLauncherWidget now hosts a second
    // entry-point button for the built-in treemap, and its labelling
    // already depends on which disk-usage tool the host has installed —
    // mask it the same way HistoryChart is masked.
    {"ResourcesPage",     "resources",
        {"HistoryChart", "DiskUsageLauncherWidget"}, {}},
    {"HelpersPage",       "helpers",           {}, {}},
    {"NetworkUsagePage",  "network_usage",     {"BarChartWidget"}, {}},
    {"SettingsPage",      "settings",          {}, {}},
};

// SSO-14981: Linux-only pages that are gated behind a runtime tool check
// (App::mPageSlots only registers them when ToolManager reports the backing
// tool available — see app.cpp) and therefore aren't guaranteed to exist in
// mStacked on every host. Kept out of kBasePageMap so a host missing one of
// these (e.g. no docker binary) can't abort the default regression suite;
// opt in per-page via NEXIS_SCREENSHOT_ONLY when driving a capture.
#ifndef Q_OS_MACOS
static const QVector<PageInfo> kRound2PageMap = {
    {"AptSourceManagerPage", "apt_source_manager", {"QAbstractItemView"}, {}},
    {"DockerPage",           "docker",             {"QAbstractItemView"}, {}},
    {"GnomeSettingsPage",    "gnome_settings",      {}, {}},
};
#endif

class ScreenshotTests : public QObject
{
    Q_OBJECT

private:
    App *mApp = nullptr;
    QStackedWidget *mStacked = nullptr;
    QString mPlatform;
    QString mRefDir;
    QString mOutDir;
    QString mFailDir;
    double mTolerance = kDefaultTolerance;
    int mChannelFuzz = kChannelFuzz;
    bool mGenerateMode = false;
    // SSO-14981: when set, generate mode only writes captured PNGs to
    // mOutDir (the build-tree scratch dir) and skips the ref-tree copy —
    // used for one-off/round-2 captures that must not touch
    // tests/reference_screenshots/.
    bool mGenerateOutputOnly = false;
    QVector<PageInfo> mPages;
    // SSO-14981: NEXIS_SCREENSHOT_ONLY restricts the capture loop to this
    // set of screenshotName values. Filtering happens before the
    // "widget not found" QVERIFY2 below, so a page missing from
    // mStacked on this host (e.g. Docker/GNOME Settings absent because
    // the backing tool isn't installed) can't abort a run driving a
    // *different* page — each round-2 page is captured as its own
    // isolated process invocation (CAPTURE_NOTES.md gotcha #2).
    QSet<QString> mOnlyFilter;

    static QVector<PageInfo> buildPageMap()
    {
        QVector<PageInfo> pages = kBasePageMap;
#ifndef Q_OS_MACOS
        if (qEnvironmentVariableIsSet("NEXIS_INCLUDE_ROUND2_PAGES"))
            pages += kRound2PageMap;
#endif
        return pages;
    }

    static QString platformDir()
    {
#ifdef Q_OS_MACOS
        return QStringLiteral("macos");
#else
        return QStringLiteral("linux");
#endif
    }

    // Build a mask region (in `root` coordinates) covering every child of
    // `page` that matches one of the declared dynamic class/object names.
    static QRegion buildMaskRegion(QWidget *page, QWidget *root, const PageInfo &info)
    {
        QRegion mask;
        if (!page || !root) return mask;

        const QList<QWidget *> allChildren = page->findChildren<QWidget *>();
        const QSet<QString> nameFilter(info.dynamicObjectNames.begin(),
                                       info.dynamicObjectNames.end());

        auto addWidgetRect = [&](QWidget *w) {
            if (!w || !w->isVisible()) return;
            const QSize size = w->size();
            if (size.isEmpty()) return;
            const QPoint topLeft = w->mapTo(root, QPoint(0, 0));
            mask = mask.united(QRect(topLeft, size));
        };

        for (QWidget *w : allChildren) {
            if (!nameFilter.isEmpty() && nameFilter.contains(w->objectName())) {
                addWidgetRect(w);
                continue;
            }
            for (const QString &cls : info.dynamicClassNames) {
                if (w->inherits(cls.toLatin1().constData())) {
                    addWidgetRect(w);
                    break;
                }
            }
        }
        return mask;
    }

    static QImage maskBufferFor(const QRegion &mask, int w, int h)
    {
        QImage buf(w, h, QImage::Format_Grayscale8);
        buf.fill(0);
        if (mask.isEmpty()) return buf;
        QPainter p(&buf);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        for (const QRect &r : mask) p.drawRect(r);
        p.end();
        return buf;
    }

    static CompareResult compareImages(const QImage &actual,
                                       const QImage &reference,
                                       const QRegion &mask,
                                       double tolerancePercent,
                                       int channelFuzz)
    {
        CompareResult result;

        if (actual.size() != reference.size()) {
            result.passed = false;
            result.diffPercent = 100.0;
            result.comparedPixels = qMax(actual.width() * actual.height(),
                                         reference.width() * reference.height());
            result.diffPixels = result.comparedPixels;
            return result;
        }

        const QImage a = actual.convertToFormat(QImage::Format_ARGB32);
        const QImage b = reference.convertToFormat(QImage::Format_ARGB32);

        const int w = a.width();
        const int h = a.height();
        const QImage maskBuf = maskBufferFor(mask, w, h);

        result.diffImage = QImage(w, h, QImage::Format_ARGB32);
        result.diffImage.fill(Qt::transparent);

        // Faint blue overlay for the masked region so failure artifacts make
        // it obvious which pixels were ignored.
        if (!mask.isEmpty()) {
            QPainter overlay(&result.diffImage);
            overlay.setPen(Qt::NoPen);
            overlay.setBrush(QColor(0, 128, 255, 40));
            for (const QRect &r : mask) overlay.drawRect(r);
        }

        QPainter diffPainter(&result.diffImage);
        diffPainter.setPen(QColor(255, 0, 0, 220));

        for (int y = 0; y < h; ++y) {
            const QRgb *ar = reinterpret_cast<const QRgb *>(a.constScanLine(y));
            const QRgb *br = reinterpret_cast<const QRgb *>(b.constScanLine(y));
            const uchar *mr = maskBuf.constScanLine(y);
            for (int x = 0; x < w; ++x) {
                if (mr[x]) {
                    ++result.maskedPixels;
                    continue;
                }
                ++result.comparedPixels;
                const QRgb ap = ar[x];
                const QRgb bp = br[x];
                const int dr = qAbs(qRed(ap)   - qRed(bp));
                const int dg = qAbs(qGreen(ap) - qGreen(bp));
                const int db = qAbs(qBlue(ap)  - qBlue(bp));
                const int da = qAbs(qAlpha(ap) - qAlpha(bp));
                if (dr > channelFuzz || dg > channelFuzz ||
                    db > channelFuzz || da > channelFuzz) {
                    ++result.diffPixels;
                    diffPainter.drawPoint(x, y);
                }
            }
        }

        diffPainter.end();

        result.diffPercent = (result.comparedPixels > 0)
            ? (static_cast<double>(result.diffPixels) / result.comparedPixels * 100.0)
            : 0.0;
        result.passed = result.diffPercent <= tolerancePercent;
        return result;
    }

    QWidget *findPageByClassName(const QString &className) const
    {
        if (!mStacked) return nullptr;
        for (int i = 0; i < mStacked->count(); ++i) {
            QWidget *w = mStacked->widget(i);
            if (w && QString::fromLatin1(w->metaObject()->className()) == className)
                return w;
        }
        return nullptr;
    }

    void captureAndCompare(const QString &theme)
    {
        SettingManager::ins()->setColorScheme(theme == "dark" ? "dark" : "light");
        AppManager::ins()->updateStylesheet();
        QApplication::processEvents();
        QTest::qWait(300);

        const QString themeOutDir = mOutDir + "/" + theme;
        QDir().mkpath(themeOutDir);

        const QString themeRefDir = mRefDir + "/" + theme;
        const QString themeFailDir = mFailDir + "/" + theme;

        // Explicit-gap handling: if the platform/theme has no committed
        // baseline PNGs at all, skip with a loud message instead of QFAILing
        // every page. Once baselines are generated and committed, missing
        // *individual* PNGs become a hard failure below.
        if (!mGenerateMode) {
            QDir refQDir(themeRefDir);
            const QStringList existing = refQDir.exists()
                ? refQDir.entryList({"*.png"}, QDir::Files)
                : QStringList();
            if (existing.isEmpty()) {
                QSKIP(qPrintable(QString("No reference baselines for %1/%2 — "
                                         "regenerate via scripts/update_screenshots.sh "
                                         "or the Regenerate Screenshot Baselines workflow")
                                 .arg(mPlatform, theme)));
            }
        }

        for (const auto &page : mPages) {
            if (!mOnlyFilter.isEmpty() && !mOnlyFilter.contains(page.screenshotName))
                continue;

            QWidget *widget = findPageByClassName(page.className);
            QVERIFY2(widget, qPrintable(QString("Page widget '%1' not found in stacked widget "
                                                "— check kBasePageMap/kRound2PageMap and "
                                                "App::ensureAllPages()")
                                        .arg(page.className)));

            mStacked->setCurrentWidget(widget);
            QApplication::processEvents();
            QTest::qWait(100);

            QPixmap pixmap = mApp->grab();
            QImage captured = pixmap.toImage();

            const QString outPath = themeOutDir + "/" + page.screenshotName + ".png";
            captured.save(outPath);

            if (mGenerateMode) {
                if (mGenerateOutputOnly) {
                    qInfo() << "Generated (output-only):" << outPath;
                    continue;
                }
                const QString refPath = themeRefDir + "/" + page.screenshotName + ".png";
                QDir().mkpath(themeRefDir);
                captured.save(refPath);
                qInfo() << "Generated:" << refPath;
                continue;
            }

            const QString refPath = themeRefDir + "/" + page.screenshotName + ".png";
            QVERIFY2(QFile::exists(refPath),
                qPrintable(QString("Reference missing: %1 — the baseline set is out of sync "
                                   "with kBasePageMap. Regenerate with scripts/update_screenshots.sh.")
                           .arg(refPath)));

            QImage reference(refPath);
            QRegion mask = buildMaskRegion(widget, mApp, page);
            CompareResult cmp = compareImages(captured, reference, mask,
                                              mTolerance, mChannelFuzz);

            if (!cmp.passed) {
                QDir().mkpath(themeFailDir);

                const QString baseName = page.screenshotName;
                captured.save(themeFailDir + "/" + baseName + "_actual.png");
                reference.save(themeFailDir + "/" + baseName + "_reference.png");
                cmp.diffImage.save(themeFailDir + "/" + baseName + "_diff.png");

                qWarning() << "MISMATCH:" << page.screenshotName
                           << "(" << theme << ")"
                           << cmp.diffPercent << "% of unmasked pixels differ"
                           << "(" << cmp.diffPixels << "/" << cmp.comparedPixels << ");"
                           << cmp.maskedPixels << "pixels masked";
            }

            QVERIFY2(cmp.passed,
                qPrintable(QString("%1 (%2): %3% of unmasked pixels differ "
                                   "(%4/%5; %6 masked) — see %7")
                    .arg(page.screenshotName, theme)
                    .arg(cmp.diffPercent, 0, 'f', 3)
                    .arg(cmp.diffPixels)
                    .arg(cmp.comparedPixels)
                    .arg(cmp.maskedPixels)
                    .arg(themeFailDir + "/" + page.screenshotName + "_diff.png")));
        }
    }

private slots:
    void initTestCase()
    {
        mPlatform = platformDir();

        QString srcDir = QString::fromLatin1(PROJECT_SOURCE_DIR);
        mRefDir = srcDir + "/tests/reference_screenshots/" + mPlatform;

        mOutDir = QDir::currentPath() + "/test_screenshots/" + mPlatform;
        QDir().mkpath(mOutDir);

        mFailDir = QDir::currentPath() + "/test_screenshots/failures/" + mPlatform;

        QByteArray tolEnv = qgetenv("NEXIS_SCREENSHOT_TOLERANCE");
        if (!tolEnv.isEmpty()) {
            bool ok;
            double val = tolEnv.toDouble(&ok);
            if (ok && val >= 0.0)
                mTolerance = val;
        }

        QByteArray fuzzEnv = qgetenv("NEXIS_SCREENSHOT_CHANNEL_FUZZ");
        if (!fuzzEnv.isEmpty()) {
            bool ok;
            int val = fuzzEnv.toInt(&ok);
            if (ok && val >= 0)
                mChannelFuzz = val;
        }

        mGenerateMode = qEnvironmentVariableIsSet("NEXIS_GENERATE_REFS");
        mGenerateOutputOnly = qEnvironmentVariableIsSet("NEXIS_GENERATE_OUTPUT_ONLY");
        mPages = buildPageMap();

        QByteArray onlyEnv = qgetenv("NEXIS_SCREENSHOT_ONLY");
        if (!onlyEnv.isEmpty()) {
            const QStringList names = QString::fromUtf8(onlyEnv).split(',', Qt::SkipEmptyParts);
            for (const QString &name : names)
                mOnlyFilter.insert(name.trimmed());
        }

        qApp->setApplicationName("nexis");
        qApp->setApplicationDisplayName("Nexis");
        qApp->setWindowIcon(QIcon(":/static/logo.svg"));
        qApp->setDesktopFileName("nexis");

#ifdef Q_OS_MAC
        QStringList paths = QIcon::themeSearchPaths();
        paths << "/opt/homebrew/share/icons" << "/usr/local/share/icons";
        QIcon::setThemeSearchPaths(paths);
#endif
        if (QIcon::themeName().isEmpty())
            QIcon::setThemeName("Adwaita");
        QIcon::setFallbackThemeName("Adwaita");

        QFontDatabase::addApplicationFont(":/static/font/Ubuntu-R.ttf");

        mApp = new App();
        mApp->resize(1024, 768);
        mApp->show();
        QApplication::processEvents();
        QTest::qWait(500);

        // FR-97: pages are constructed lazily on first navigation. The
        // screenshot suite needs every page present in the stacked widget,
        // so force-construct here.
        mApp->ensureAllPages();
        QApplication::processEvents();
        QTest::qWait(200);

        mStacked = mApp->findChild<QStackedWidget *>();
        QVERIFY2(mStacked, "Could not find QStackedWidget in App");

        qInfo() << "Platform:" << mPlatform;
        qInfo() << "Reference dir:" << mRefDir;
        qInfo() << "Output dir:" << mOutDir;
        qInfo() << "Tolerance:" << mTolerance << "% of unmasked pixels";
        qInfo() << "Per-channel fuzz:" << mChannelFuzz;
        qInfo() << "Generate mode:" << mGenerateMode;
        qInfo() << "Pages in stacked widget:" << mStacked->count();
    }

    void cleanupTestCase()
    {
        DataRefreshService::ins()->stop();
        if (mApp) {
            mApp->hide();
            mApp = nullptr;
        }
        QString testConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir(testConfig).removeRecursively();
    }

    // ── Self-tests for the comparison harness ────────────────────────────
    // These validate that compareImages() behaves correctly under the
    // mask + fuzz contract, so a green ScreenshotTests run on a clean
    // baseline can't also be hiding a broken comparison routine.

    void selfTest_identicalImagesPass()
    {
        QImage img(64, 48, QImage::Format_ARGB32);
        img.fill(QColor(40, 80, 160));
        CompareResult r = compareImages(img, img, QRegion(), 0.0, 0);
        QVERIFY(r.passed);
        QCOMPARE(r.diffPixels, 0);
    }

    void selfTest_perChannelFuzzAccepted()
    {
        QImage a(32, 32, QImage::Format_ARGB32);
        QImage b(32, 32, QImage::Format_ARGB32);
        a.fill(QColor(100, 100, 100, 255));
        // All channels differ by +5 — within ≤ 8 fuzz, must count as equal.
        b.fill(QColor(105, 105, 105, 255));
        CompareResult r = compareImages(a, b, QRegion(), 0.0, kChannelFuzz);
        QVERIFY2(r.passed,
            qPrintable(QString("expected fuzz pass; diff=%1%, pixels=%2/%3")
                .arg(r.diffPercent).arg(r.diffPixels).arg(r.comparedPixels)));
        QCOMPARE(r.diffPixels, 0);
    }

    void selfTest_perChannelFuzzExceededFails()
    {
        QImage a(32, 32, QImage::Format_ARGB32);
        QImage b(32, 32, QImage::Format_ARGB32);
        a.fill(QColor(100, 100, 100, 255));
        // Delta of 10 per channel exceeds fuzz=8 — every pixel must register
        // as different.
        b.fill(QColor(110, 110, 110, 255));
        CompareResult r = compareImages(a, b, QRegion(), 0.0, kChannelFuzz);
        QVERIFY(!r.passed);
        QCOMPARE(r.diffPixels, 32 * 32);
    }

    void selfTest_unmaskedDifferenceDetected()
    {
        QImage ref(100, 100, QImage::Format_ARGB32);
        ref.fill(QColor(0, 0, 0, 255));
        QImage actual = ref.copy();
        // Paint a clearly-different 20×20 block at (60, 60) — outside any
        // mask. The comparator must flag this as a regression.
        QPainter p(&actual);
        p.fillRect(60, 60, 20, 20, QColor(255, 0, 0, 255));
        p.end();

        // 400 differing pixels out of 10000 = 4% > 1% tolerance → fails.
        CompareResult r = compareImages(actual, ref, QRegion(), 1.0, kChannelFuzz);
        QVERIFY2(!r.passed,
            qPrintable(QString("expected detection; diff=%1%, diffPixels=%2")
                .arg(r.diffPercent).arg(r.diffPixels)));
        QCOMPARE(r.diffPixels, 400);
        QVERIFY(r.diffPercent > 1.0);
    }

    void selfTest_maskedDifferenceIgnored()
    {
        QImage ref(100, 100, QImage::Format_ARGB32);
        ref.fill(QColor(0, 0, 0, 255));
        QImage actual = ref.copy();
        // Same alteration, but this time inside the masked rectangle —
        // comparator must ignore it.
        QPainter p(&actual);
        p.fillRect(60, 60, 20, 20, QColor(255, 0, 0, 255));
        p.end();

        QRegion mask(QRect(55, 55, 30, 30));
        CompareResult r = compareImages(actual, ref, mask, 1.0, kChannelFuzz);
        QVERIFY2(r.passed,
            qPrintable(QString("mask should hide alteration; diff=%1%, diffPixels=%2, masked=%3")
                .arg(r.diffPercent).arg(r.diffPixels).arg(r.maskedPixels)));
        QCOMPARE(r.diffPixels, 0);
        QVERIFY(r.maskedPixels >= 20 * 20);
    }

    void selfTest_maskDoesNotHideAdjacentRegression()
    {
        // Mask covers (55..85, 55..85). Paint a regression at (10, 10) just
        // outside the mask — must still fire.
        QImage ref(100, 100, QImage::Format_ARGB32);
        ref.fill(QColor(0, 0, 0, 255));
        QImage actual = ref.copy();
        QPainter p(&actual);
        p.fillRect(10, 10, 20, 20, QColor(255, 0, 0, 255));
        p.end();

        QRegion mask(QRect(55, 55, 30, 30));
        CompareResult r = compareImages(actual, ref, mask, 1.0, kChannelFuzz);
        QVERIFY(!r.passed);
        QCOMPARE(r.diffPixels, 400);
    }

    void selfTest_sizeMismatchFails()
    {
        QImage a(64, 64, QImage::Format_ARGB32);
        QImage b(100, 100, QImage::Format_ARGB32);
        a.fill(Qt::black);
        b.fill(Qt::black);
        CompareResult r = compareImages(a, b, QRegion(), 99.0, 64);
        QVERIFY(!r.passed);
        QCOMPARE(r.diffPercent, 100.0);
    }

    // ── Actual screenshot comparison slots ───────────────────────────────

    void screenshotDarkTheme()
    {
        captureAndCompare("dark");
    }

    void screenshotLightTheme()
    {
        captureAndCompare("light");
    }
};

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(static);
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    QStandardPaths::setTestModeEnabled(true);
    ScreenshotTests tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_screenshots.moc"
