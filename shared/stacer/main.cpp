
#include <QApplication>
#include <QSplashScreen>
#include <QDebug>
#include <QFontDatabase>
#include <QIcon>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>

#include "app.h"

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
        level = "UNDEFIEND"; break;
    }

    if (type != QtWarningMsg) {

        QString text = QString("[%1] [%2] %3")
                                .arg(QDateTime::currentDateTime().toString("dd-MM-yyyy hh:mm:ss"))
                                .arg(level)
                                .arg(message);

        static QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

        QFile file(logPath + "/stacer.log");

        QIODevice::OpenMode openMode = file.size() > (1L << 20) ? QIODevice::Truncate : QIODevice::Append;

        if (file.open(QIODevice::WriteOnly | openMode)) {
            QTextStream stream(&file);
            stream << text << Qt::endl;

            file.close();
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    qApp->setApplicationName("stacer");
    qApp->setApplicationDisplayName("Stacer");
    qApp->setApplicationVersion("2.0.1");
    qApp->setWindowIcon(QIcon(":/static/logo.svg"));

    // Single-instance enforcement (BUG-03 / FR-02)
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/stacer.lock";
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0);

    if (!lockFile.tryLock(100)) {
        QMessageBox::warning(nullptr, "Stacer",
            QObject::tr("Another instance of Stacer is already running."));
        return 1;
    }

    {
       QCommandLineOption hideOption("hide", "Hide Stacer while launching.");
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

    QPixmap pixSplash(":/static/splashscreen-2.0.1.png");

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
