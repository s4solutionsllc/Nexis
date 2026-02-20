#include <QTest>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QStackedWidget>
#include <QScreen>

#include "app.h"
#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"

struct CompareResult {
    bool passed = false;
    double diffPercent = 0.0;
    int diffPixels = 0;
    int totalPixels = 0;
    QImage diffImage;
};

struct PageInfo {
    QString className;
    QString screenshotName;
    double extraTolerance = 0.0;
};

// Pages with live data (CPU %, memory %, process lists, network charts) need
// higher tolerance because their content changes between the reference capture
// and the comparison run.
static const QVector<PageInfo> kPageMap = {
    {"DashboardPage",     "dashboard",      5.0},
    {"HardwareInfoPage",  "hardware_info",  3.0},
    {"StartupAppsPage",   "startup_apps",   0.0},
    {"SystemCleanerPage", "system_cleaner", 0.0},
    {"SearchPage",        "search",         0.0},
    {"ServicesPage",      "services",       0.0},
    {"ProcessesPage",     "processes",      10.0},
    {"UninstallerPage",   "uninstaller",    0.0},
    {"ResourcesPage",     "resources",      5.0},
    {"HelpersPage",       "helpers",        0.0},
    {"SettingsPage",      "settings",       0.0},
};

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
    double mTolerance = 1.0;
    bool mGenerateMode = false;

    static QString platformDir()
    {
#ifdef Q_OS_MACOS
        return QStringLiteral("macos");
#else
        return QStringLiteral("linux");
#endif
    }

    static CompareResult compareImages(const QImage &actual, const QImage &reference,
                                       double tolerancePercent)
    {
        CompareResult result;

        if (actual.size() != reference.size()) {
            result.passed = false;
            result.diffPercent = 100.0;
            result.totalPixels = qMax(actual.width() * actual.height(),
                                      reference.width() * reference.height());
            result.diffPixels = result.totalPixels;
            return result;
        }

        const int w = actual.width();
        const int h = actual.height();
        result.totalPixels = w * h;
        result.diffImage = QImage(w, h, QImage::Format_ARGB32);
        result.diffImage.fill(Qt::transparent);

        QPainter painter(&result.diffImage);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (actual.pixel(x, y) != reference.pixel(x, y)) {
                    ++result.diffPixels;
                    painter.setPen(QColor(255, 0, 0, 200));
                    painter.drawPoint(x, y);
                }
            }
        }

        painter.end();

        result.diffPercent = (result.totalPixels > 0)
            ? (static_cast<double>(result.diffPixels) / result.totalPixels * 100.0)
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

        for (const auto &page : kPageMap) {
            QWidget *widget = findPageByClassName(page.className);
            if (!widget) {
                qWarning() << "Page not found:" << page.className << "— skipping";
                continue;
            }

            mStacked->setCurrentWidget(widget);
            QApplication::processEvents();
            QTest::qWait(100);

            QPixmap pixmap = mApp->grab();
            QImage captured = pixmap.toImage();

            const QString outPath = themeOutDir + "/" + page.screenshotName + ".png";
            captured.save(outPath);

            if (mGenerateMode) {
                const QString refPath = themeRefDir + "/" + page.screenshotName + ".png";
                QDir().mkpath(themeRefDir);
                captured.save(refPath);
                qInfo() << "Generated:" << refPath;
                continue;
            }

            const QString refPath = themeRefDir + "/" + page.screenshotName + ".png";
            if (!QFile::exists(refPath)) {
                qWarning() << "Reference missing:" << refPath << "— skipping comparison";
                continue;
            }

            QImage reference(refPath);
            double pageTolerance = mTolerance + page.extraTolerance;
            CompareResult cmp = compareImages(captured, reference, pageTolerance);

            if (!cmp.passed) {
                QDir().mkpath(themeFailDir);

                const QString baseName = page.screenshotName;
                captured.save(themeFailDir + "/" + baseName + "_actual.png");
                reference.save(themeFailDir + "/" + baseName + "_reference.png");
                cmp.diffImage.save(themeFailDir + "/" + baseName + "_diff.png");

                qWarning() << "MISMATCH:" << page.screenshotName
                           << "(" << theme << ")"
                           << cmp.diffPercent << "% different"
                           << "(" << cmp.diffPixels << "/" << cmp.totalPixels << "pixels)";
            }

            QVERIFY2(cmp.passed,
                qPrintable(QString("%1 (%2): %3% pixels differ (%4/%5) — see %6")
                    .arg(page.screenshotName, theme)
                    .arg(cmp.diffPercent, 0, 'f', 2)
                    .arg(cmp.diffPixels)
                    .arg(cmp.totalPixels)
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

        mGenerateMode = qEnvironmentVariableIsSet("NEXIS_GENERATE_REFS");

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

        mStacked = mApp->findChild<QStackedWidget *>();
        QVERIFY2(mStacked, "Could not find QStackedWidget in App");

        qInfo() << "Platform:" << mPlatform;
        qInfo() << "Reference dir:" << mRefDir;
        qInfo() << "Output dir:" << mOutDir;
        qInfo() << "Tolerance:" << mTolerance << "%";
        qInfo() << "Generate mode:" << mGenerateMode;
        qInfo() << "Pages in stacked widget:" << mStacked->count();
    }

    void cleanupTestCase()
    {
        delete mApp;
        mApp = nullptr;
    }

    void screenshotDarkTheme()
    {
        captureAndCompare("dark");
    }

    void screenshotLightTheme()
    {
        captureAndCompare("light");
    }
};

QTEST_MAIN(ScreenshotTests)
#include "test_screenshots.moc"
