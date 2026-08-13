
#include <QApplication>
#include <QStyleFactory>
#include <QSplashScreen>
#include <QDebug>
#include <QFontDatabase>
#include <QIcon>
#include <QLockFile>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

#include "app.h"
#include <Managers/setting_manager.h>
#include <Managers/cleaner_service.h>
#include <Managers/schedule_manager.h>
#include <Managers/health_report_manager.h>
#include <Services/wipe_free_space_service.h>
#include <Utils/format_util.h>
#include <Utils/headless_util.h>
#ifdef Q_OS_MAC
#include <Info/sparkle_update_installer.h>
#endif

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context)

    // In release builds, suppress Qt warnings (noisy but harmless).
    // In debug builds, let them through — they often indicate real problems.
#ifdef QT_NO_DEBUG
    if (type == QtWarningMsg)
        return;
#endif

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

    const QString text = QString("[%1] [%2] %3")
                            .arg(QDateTime::currentDateTime().toString("dd-MM-yyyy hh:mm:ss"))
                            .arg(level)
                            .arg(message);

    // FR-109: persistent log file handle. Previously every message opened,
    // size-checked, wrote, and closed the file — a full stat+open+close cycle
    // per qDebug/qWarning. Now we open once lazily and keep the handle for
    // the process lifetime, rotating periodically. Mutex-guarded because
    // qDebug can be called from any thread (notably QtConcurrent workers).
    static QMutex mutex;
    static QFile *logFile = nullptr;
    static int writesSinceSizeCheck = 0;

    QMutexLocker lock(&mutex);

    if (!logFile) {
        const QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(logPath);
        logFile = new QFile(logPath + "/nexis.log");
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            delete logFile;
            logFile = nullptr;
            return;
        }
    }

    // Rotate when the file exceeds 1 MB. stat() isn't free, so only check
    // occasionally — at roughly 1 MB of log the file will have > 10 000
    // lines, so checking every 500 writes is cheap enough.
    if (++writesSinceSizeCheck >= 500) {
        writesSinceSizeCheck = 0;
        if (logFile->size() > (1L << 20)) {
            logFile->close();
            if (!logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                delete logFile;
                logFile = nullptr;
                return;
            }
        }
    }

    QTextStream stream(logFile);
    stream << text << Qt::endl;
    logFile->flush();
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

#ifdef Q_OS_LINUX
static QString detectSystemIconTheme()
{
    const QString home = QDir::homePath();

    // GTK4 / GTK3 settings.ini
    const QStringList gtkIni = {
        home + "/.config/gtk-4.0/settings.ini",
        home + "/.config/gtk-3.0/settings.ini",
    };
    for (const QString &path : gtkIni) {
        if (QFile::exists(path)) {
            QSettings s(path, QSettings::IniFormat);
            QString name = s.value("Settings/gtk-icon-theme-name").toString().trimmed();
            if (!name.isEmpty() && name != "hicolor")
                return name;
        }
    }

    // GTK2 ~/.gtkrc-2.0
    QFile gtkrc(home + "/.gtkrc-2.0");
    if (gtkrc.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QRegularExpression re(R"(gtk-icon-theme-name\s*=\s*["]?([^"\n]+)["]?)");
        while (!gtkrc.atEnd()) {
            QString line = QString::fromUtf8(gtkrc.readLine()).trimmed();
            QRegularExpressionMatch m = re.match(line);
            if (m.hasMatch()) {
                QString name = m.captured(1).trimmed();
                if (!name.isEmpty() && name != "hicolor")
                    return name;
            }
        }
    }

    // KDE / Plasma kdeglobals
    const QString kdeglobals = home + "/.config/kdeglobals";
    if (QFile::exists(kdeglobals)) {
        QSettings s(kdeglobals, QSettings::IniFormat);
        QString name = s.value("Icons/Theme").toString().trimmed();
        if (!name.isEmpty() && name != "hicolor")
            return name;
    }

    // gsettings fallback (GNOME/DEs that don't use GTK config files)
    QProcess proc;
    proc.start("gsettings", {"get", "org.gnome.desktop.interface", "icon-theme"});
    if (proc.waitForFinished(1000)) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        out.remove('\'');
        if (!out.isEmpty() && out != "hicolor")
            return out;
    }

    return {};
}
#endif

int main(int argc, char *argv[])
{
    // Force-include the Qt Resource data from the nexis-gui static library.
    // Without this, the linker dead-strips qrc_static.cpp.o and all :/ paths fail.
    Q_INIT_RESOURCE(static);

    // SSO-3368 / audit H6: cron and systemd-user `--clean` runs have no display.
    // Constructing QApplication without a QPA platform aborts the process, so
    // scheduled cleans were silently never running. Steer the headless modes
    // to the offscreen platform before QApplication touches the display. We
    // only override when the user has not pinned QT_QPA_PLATFORM themselves.
    if (HeadlessUtil::shouldForceOffscreen(
            HeadlessUtil::isHeadlessArgv(argc, argv),
            qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setQuitOnLastWindowClosed(false);

    qApp->setApplicationName("nexis");
    qApp->setApplicationDisplayName("Nexis");
    qApp->setApplicationVersion(APP_VERSION);
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":/static/logo.svg"));
#endif
    qApp->setDesktopFileName("nexis");

    // ── Headless mode detection (before QLockFile) ──────────────────────
    // Parse --clean <schedule-id>, --check-threshold, and --report
    // <schedule-id> before the single-instance lock so that OS-scheduled
    // headless invocations don't conflict with a running GUI instance.

    QString cleanScheduleId;
    bool checkThreshold = false;
    QString reportScheduleId;

    for (int i = 1; i < argc; ++i) {
        QString arg(argv[i]);
        if (arg == "--clean" && i + 1 < argc) {
            cleanScheduleId = QString(argv[++i]);
        } else if (arg == "--check-threshold") {
            checkThreshold = true;
        } else if (arg == "--report" && i + 1 < argc) {
            reportScheduleId = QString(argv[++i]);
        }
    }

    bool headless = !cleanScheduleId.isEmpty() || checkThreshold || !reportScheduleId.isEmpty();

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

    // ── Headless --report ────────────────────────────────────────────────
    // Fired by the launchd/systemd/cron entries syncReportSchedulesToOS()
    // installs (SSO-5778, follow-up to FW-14 / SSO-3742).
    if (!reportScheduleId.isEmpty()) {
        qInstallMessageHandler(messageHandler);

        HealthReportManager::ins()->runScheduledReport(reportScheduleId);

        return 0;
    }

    // ── GUI mode ────────────────────────────────────────────────────────

    // FR-109: capture qDebug/qWarning/qCritical to ~/.config/nexis/nexis.log
    // for the interactive path too. Previously this was only installed for
    // the two headless entry points above, so normal app runs produced no
    // logs at all.
    qInstallMessageHandler(messageHandler);

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

    // SSO-17776 design doc §4 / §10.3: verified update artifacts are kept
    // alive for the process lifetime (no waitable "installer finished"
    // handle exists to clean up on), so a normal quit cleans them up via
    // static destruction. This sweep covers the crash/force-quit case.
    SparkleUpdateInstaller::sweepStaleArtifacts();
#elif defined(Q_OS_LINUX)
    // AppImage bundles its own Qt, which loses access to system icon theme paths.
    // Add standard XDG icon directories so QIcon::fromTheme() finds the user's
    // desktop theme (e.g., Papirus, Breeze) for the "System Theme" tray icon.
    if (!qEnvironmentVariableIsEmpty("APPIMAGE")) {
        QStringList paths = QIcon::themeSearchPaths();
        const QString home = QDir::homePath();
        const QStringList extra = {
            home + "/.local/share/icons",
            home + "/.icons",
            "/usr/local/share/icons",
            "/usr/share/icons",
            "/usr/share/pixmaps",
        };
        for (const auto &p : extra) {
            if (!paths.contains(p) && QDir(p).exists())
                paths << p;
        }
        QIcon::setThemeSearchPaths(paths);

        // The bundled Qt has no GTK platform plugin, so QIcon::themeName() returns
        // empty. Detect the system theme from GTK/KDE config files so fromTheme()
        // searches the correct theme directory (e.g., Papirus-Dark).
        const QString currentTheme = QIcon::themeName();
        if (currentTheme.isEmpty() || currentTheme == "hicolor") {
            const QString detected = detectSystemIconTheme();
            if (!detected.isEmpty())
                QIcon::setThemeName(detected);
        }
    }
#endif

    // Ensure Adwaita icons are available as a fallback theme
    if (QIcon::themeName().isEmpty())
        QIcon::setThemeName("Adwaita");
    QIcon::setFallbackThemeName("Adwaita");

    QFontDatabase::addApplicationFont(":/static/font/Ubuntu-R.ttf");
    QFontDatabase::addApplicationFont(":/static/font/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/static/font/Inter-Bold.ttf");
    QFontDatabase::addApplicationFont(":/static/font/Inter-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/static/font/JetBrainsMono-Regular.ttf");

    // Resolve splashscreen variant from the user's color scheme preference.
    // SettingManager/AppManager are not yet constructed, so read QSettings directly.
    QString splashPath;
    {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QSettings settings(configPath + "/settings.ini", QSettings::IniFormat);
        QString scheme = settings.value("ColorScheme", "auto").toString();

        bool useLight = false;
        if (scheme == "light") {
            useLight = true;
        } else if (scheme == "auto") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            useLight = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light);
#endif
        }
        splashPath = useLight ? ":/static/splashscreen_light.png"
                              : ":/static/splashscreen_dark.png";
    }

    QPixmap pixSplash(splashPath);
    QSplashScreen *splash = new QSplashScreen(pixSplash);

    if (!isNoSplash) splash->show();

    app.processEvents();

    // SSO-15382: if the previous run was killed/crashed mid-wipe, remove the
    // leftover fill file now so the volume is never left artificially full.
    WipeFreeSpaceService::recoverFromCrash();

    App w;

    // SSO-354: setting-based equivalent of --hide for any launch path. Never
    // honor this without a usable tray — otherwise the app launches with no
    // window and no way to bring it back.
    const bool startMinimizedToTray = SettingManager::ins()->getStartMinimizedToTray()
        && QSystemTrayIcon::isSystemTrayAvailable();

    if (!isHide && !startMinimizedToTray) {
        w.show();
    }

    splash->finish(&w);

    delete splash;

    return app.exec();
}
