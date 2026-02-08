#include "startup_app.h"
#include "ui_startup_app.h"
#include "utilities.h"

StartupApp::~StartupApp()
{
    delete ui;
}

StartupApp::StartupApp(const QString &startupAppName, bool enabled, const QString &filePath, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StartupApp),
    mStartupAppName(startupAppName),
    mEnabled(enabled),
    mFilePath(filePath)
{
    ui->setupUi(this);

    ui->lblStartupAppName->setText(startupAppName);
    ui->checkStartup->setChecked(enabled);

    Utilities::addDropShadow(this, 50);
}

void StartupApp::on_checkStartup_clicked(bool status)
{
    // macOS LaunchAgents: toggle Disabled key in plist
    QStringList lines = FileUtil::readListFromFile(mFilePath);
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
