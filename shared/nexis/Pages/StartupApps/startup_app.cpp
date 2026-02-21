#include "startup_app.h"
#include "ui_startup_app.h"
#include "utilities.h"
#include <QIcon>
#include <QFileInfo>
#ifdef Q_OS_MACOS
#include <QFileIconProvider>
#endif

StartupApp::~StartupApp()
{
    delete ui;
}

StartupApp::StartupApp(const QString &startupAppName, bool enabled, const QString &filePath, const QString &iconName, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StartupApp),
    mStartupAppName(startupAppName),
    mIconName(iconName),
    mEnabled(enabled),
    mFilePath(filePath)
{
    ui->setupUi(this);

    ui->lblStartupAppName->setText(startupAppName);
    ui->checkStartup->setChecked(enabled);

    // Resolve and display app icon
    QIcon appIcon;
    if (!iconName.isEmpty()) {
#ifdef Q_OS_MACOS
        // On macOS, iconName is the path to a .app bundle
        if (iconName.endsWith(".app") && QFileInfo::exists(iconName)) {
            QFileIconProvider iconProvider;
            appIcon = iconProvider.icon(QFileInfo(iconName));
        }
#else
        // On Linux, iconName is a freedesktop icon theme name or absolute path.
        // Try loading directly from path first, then fall back to theme lookup.
        if (QFileInfo::exists(iconName)) {
            appIcon = QIcon(iconName);
        }
        if (appIcon.isNull()) {
            appIcon = QIcon::fromTheme(iconName, QIcon(":/static/themes/common/img/package.png"));
        }
#endif
    }

    if (!appIcon.isNull()) {
        QSize iconSize = ui->lblStartupAppIcon->minimumSize();
        ui->lblStartupAppIcon->setPixmap(appIcon.pixmap(iconSize));
    } else {
        ui->lblStartupAppIcon->hide();
    }

    Utilities::addDropShadow(this, 50);
}

void StartupApp::on_checkStartup_clicked(bool status)
{
    QStringList lines = FileUtil::readListFromFile(mFilePath);

#ifdef Q_OS_MACOS
    // macOS LaunchAgents: toggle Disabled key in plist XML
    QRegularExpression disabledKeyReg("^\\s*<key>Disabled</key>");
    int pos = lines.indexOf(disabledKeyReg);

    if (status) {
        // Enable: remove Disabled key and its value
        if (pos != -1) {
            lines.removeAt(pos); // remove <key>Disabled</key>
            if (pos < lines.size()) {
                lines.removeAt(pos); // remove <true/> or <false/>
            }
        }
    } else {
        // Disable: add or update Disabled key
        if (pos != -1) {
            // Update the value line
            if (pos + 1 < lines.size()) {
                lines.replace(pos + 1, "\t<true/>");
            }
        } else {
            // Insert before </dict>
            QRegularExpression dictEndReg("^\\s*</dict>");
            int dictEnd = lines.indexOf(dictEndReg);
            if (dictEnd != -1) {
                lines.insert(dictEnd, "\t<true/>");
                lines.insert(dictEnd, "\t<key>Disabled</key>");
            }
        }
    }
#else
    // Linux .desktop: toggle Hidden and X-GNOME-Autostart-enabled keys
    // Hidden=[true|false]
    int pos = lines.indexOf(HIDDEN_REG);

    QString _status = status ? "true" : "false";

    if (pos != -1) {
        _status = status ? "false" : "true";
        lines.replace(pos, QString("Hidden=%1").arg(_status));
    } else {
        // X-GNOME-Autostart-enabled=[true|false]
        pos = lines.indexOf(GNOME_ENABLED_REG);
        if (pos != -1) {
            lines.replace(pos, QString("X-GNOME-Autostart-enabled=%1").arg(_status));
        }
    }

    if (pos == -1) {
        _status = status ? "false" : "true";
        lines.append(QString("Hidden=%1").arg(_status));
    }
#endif

    FileUtil::writeFile(mFilePath, lines.join('\n').append('\n'));
}

void StartupApp::on_btnDeleteStartupApp_clicked()
{
    if (QFile::remove(mFilePath)) {
        emit deleteAppS();
    }
}

void StartupApp::on_btnEditStartupApp_clicked()
{
    emit editStartupAppS(mFilePath);
}

QString StartupApp::getAppName() const
{
    return mStartupAppName;
}

void StartupApp::setAppName(const QString &value)
{
    mStartupAppName = value;
}

bool StartupApp::getEnabled() const
{
    return mEnabled;
}

void StartupApp::setEnabled(bool value)
{
    mEnabled = value;
}

QString StartupApp::getFilePath() const
{
    return mFilePath;
}

void StartupApp::setFilePath(const QString &value)
{
    mFilePath = value;
}
