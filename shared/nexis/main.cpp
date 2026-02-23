
#include <QApplication>
#include <QStyleFactory>
#include <QSplashScreen>
#include <QDebug>
#include <QFontDatabase>
#include <QIcon>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>

#include "app.h"
#include <Managers/setting_manager.h>
#include <Managers/cleaner_service.h>
#include <Managers/schedule_manager.h>
#include <Utils/format_util.h>

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context)

    QString level;

    switch (type) {
    case QtDebugMsg:
        level = "DEBUG"; break;
    case QtInfoMsg:
        level = "INFO"; break;
    case QtWarningMsg:
        level = "WARNING"; break;
    case QtCriticalMsg:
        level = "CRITICAL"; break;
    case QtFatalMsg:
        level = "FATAL"; break;
    default:
        level = "UNDEFINED"; break;
    }

    // In release builds, suppress Qt warnings (noisy but harmless).
    // In debug builds, let them through — they often indicate real problems.
#ifdef QT_NO_DEBUG
    if (type == QtWarningMsg)
        return;
#endif

    QString text = QString("[%1] [%2] %3")
                            .arg(QDateTime::currentDateTime().toString("dd-MM-yyyy hh:mm:ss"))
                            .arg(level)
                            .arg(message);

    static QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

    QFile file(logPath + "/nexis.log");

    QIODevice::OpenMode openMode = file.size() > (1L << 20) ? QIODevice::Truncate : QIODevice::Append;

    if (file.open(QIODevice::WriteOnly | openMode)) {
        QTextStream stream(&file);
        stream << text << Qt::endl;

        file.close();
    }
}

static void showNotificationAndExit(QApplication &app, const QString &title, const QString &message)
{
    if (!SettingManager::ins()->getCleaningNotificationsEnabled()) {
        return;
    }

    QSystemTrayIcon *tray = new QSystemTrayIcon(QIcon(":/static/logo.svg"), &app);
    tray->show();
    tray->showMessage(title, message, QSystemTrayIcon::Information, 5000);

    // Allow notification to display before exiting
    QTimer::singleShot(5000, &app, &QApplication::quit);
    app.exec();
}

int main(int argc, char *argv[])
{
    // Force-include the Qt Resource data from the nexis-gui static library.
    // Without this, the linker dead-strips qrc_static.cpp.o and all :/ paths fail.
    Q_INIT_RESOURCE(static);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setQuitOnLastWindowClosed(false);

    qApp->setApplicationName("nexis");
    qApp->setApplicationDisplayName("Nexis");
    qApp->setApplicationVersion(APP_VERSION);
    qApp->setWindowIcon(QIcon(":/static/logo.svg"));
    qApp->setDesktopFileName("nexis");

    // ── Headless mode detection (before QLockFile) ──────────────────────
    // Parse --clean <schedule-id> and --check-threshold before the
    // single-instance lock so that OS-scheduled headless invocations
    // don't conflict with a running GUI instance.

    QString cleanScheduleId;
    bool checkThreshold = false;

    for (int i = 1; i < argc; ++i) {
        QString arg(argv[i]);
        if (arg == "--clean" && i + 1 < argc) {
            cleanScheduleId = QString(argv[++i]);
        } else if (arg == "--check-threshold") {
            checkThreshold = true;
        }
    }

    bool headless = !cleanScheduleId.isEmpty() || checkThreshold;

    // ── Headless --clean ────────────────────────────────────────────────
    if (!cleanScheduleId.isEmpty()) {
        qInstallMessageHandler(messageHandler);

        CleanerService::CleanResult result =
            CleanerService::ins()->cleanSchedule(cleanScheduleId);

        if (result.totalBytesFreed > 0 || !result.scheduleName.isEmpty()) {
            QString msg = QObject::tr("Cleaned %1 (%2)")
                .arg(FormatUtil::formatBytes(result.totalBytesFreed))
                .arg(result.scheduleName);
            showNotificationAndExit(app, "Nexis", msg);
        }

        return 0;
    }

    // ── Headless --check-threshold ─────────────────────────────────────
    if (checkThreshold) {
        qInstallMessageHandler(messageHandler);

        if (!SettingManager::ins()->getThresholdAlertEnabled()) {
            return 0;
        }

        CleanerService::ScanResult result =
            CleanerService::ins()->scan(CleanerService::allCategories());

        quint64 thresholdBytes =
            static_cast<quint64>(SettingManager::ins()->getThresholdGB()) * 1073741824ULL;

        if (result.totalSize >= thresholdBytes) {
            QString msg = QObject::tr("Nexis found %1 of cleanable files. Open Nexis to review.")
                .arg(FormatUtil::formatBytes(result.totalSize));
            showNotificationAndExit(app, "Nexis", msg);
        }

        return 0;
    }

    // ── GUI mode ────────────────────────────────────────────────────────

    // Single-instance enforcement (BUG-03 / FR-02)
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/nexis.lock";
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0);

    if (!lockFile.tryLock(100)) {
        QMessageBox::warning(nullptr, "Nexis",
            QObject::tr("Another instance of Nexis is already running."));
        return 1;
    }

    {
       QCommandLineOption hideOption("hide", "Hide Nexis while launching.");
       QCommandLineOption noSplashOption("nosplash", "Hide splash screen while launching.");
        QCommandLineParser parser;
        parser.addVersionOption();
        parser.addHelpOption();
	    parser.addOption(hideOption);
        parser.addOption(noSplashOption);
        parser.process(app);
    }

    bool isHide = false;
    bool isNoSplash = false;

    QLatin1String hideOption("--hide");
    QLatin1String noSplashOption("--nosplash");

    for (int i = 1; i < argc; ++i) {
      if (QString(argv[i]) == hideOption)
        isHide = true;
      else if (QString(argv[i]) == noSplashOption)
        isNoSplash = true;
    }

#ifdef Q_OS_MAC
    // macOS: Homebrew-installed icon themes aren't in Qt's default search paths.
    // Add both ARM (/opt/homebrew) and Intel (/usr/local) Homebrew locations.
    {
        QStringList paths = QIcon::themeSearchPaths();
        paths << "/opt/homebrew/share/icons" << "/usr/local/share/icons";
        QIcon::setThemeSearchPaths(paths);
    }
#endif

    // Ensure Adwaita icons are available as a fallback theme
    if (QIcon::themeName().isEmpty())
        QIcon::setThemeName("Adwaita");
    QIcon::setFallbackThemeName("Adwaita");

    QFontDatabase::addApplicationFont(":/static/font/Ubuntu-R.ttf");
    QFontDatabase::addApplicationFont(":/static/font/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/static/font/Inter-Bold.ttf");
    QFontDatabase::addApplicationFont(":/static/font/JetBrainsMono-Regular.ttf");

    QPixmap pixSplash(":/static/splashscreen.png");

    QSplashScreen *splash = new QSplashScreen(pixSplash);

    if (!isNoSplash) splash->show();

    app.processEvents();

    App w;

    if (argc < 2 || !isHide) {
        w.show();
    }

    splash->finish(&w);

    delete splash;

    return app.exec();
}
