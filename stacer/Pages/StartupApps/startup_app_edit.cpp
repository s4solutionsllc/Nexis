#include "startup_app_edit.h"
#include "ui_startup_app_edit.h"
#include "utilities.h"
#include <QDebug>
#include <QStyle>
#include <QRegularExpression>

StartupAppEdit::~StartupAppEdit()
{
    delete ui;
}

QString StartupAppEdit::selectedFilePath = "";

StartupAppEdit::StartupAppEdit(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StartupAppEdit),
#ifdef Q_OS_LINUX
    mNewAppTemplate("[Desktop Entry]\n"
                   "Name=%1\n"
                   "Comment=%2\n"
                   "Exec=%3\n"
                   "Type=Application\n"
                   "Terminal=false\n"
                   "Hidden=false\n")
#elif defined(Q_OS_MACOS)
    mNewAppTemplate("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                   "<plist version=\"1.0\">\n"
                   "<dict>\n"
                   "    <key>Label</key>\n"
                   "    <string>%1</string>\n"
                   "    <key>ProgramArguments</key>\n"
                   "    <array>\n"
                   "        <string>%3</string>\n"
                   "    </array>\n"
                   "    <key>RunAtLoad</key>\n"
                   "    <true/>\n"
                   "</dict>\n"
                   "</plist>\n")
#endif
{
    ui->setupUi(this);

    init();
}

void StartupAppEdit::init()
{
    setGeometry(
        QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
            size(), qApp->primaryScreen()->availableGeometry())
    );

#ifdef Q_OS_LINUX
    mAutostartPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
#elif defined(Q_OS_MACOS)
    mAutostartPath = QDir::homePath() + "/Library/LaunchAgents";
#endif

    ui->lblErrorMsg->hide();

    setStyleSheet(AppManager::ins()->getStylesheetFileContent());
}

void StartupAppEdit::show()
{
    // clear fields
    ui->txtStartupAppName->clear();
    ui->txtStartupAppComment->clear();
    ui->txtStartupAppCommand->clear();
    ui->lblErrorMsg->hide();

    if(! selectedFilePath.isEmpty())
    {
        QStringList lines = FileUtil::readListFromFile(selectedFilePath);

        if(! lines.isEmpty())
        {
#ifdef Q_OS_LINUX
            ui->txtStartupAppName->setText(Utilities::getDesktopValue(NAME_REG, lines));
            ui->txtStartupAppComment->setText(Utilities::getDesktopValue(COMMENT_REG, lines));
            ui->txtStartupAppCommand->setText(Utilities::getDesktopValue(EXEC_REG, lines));
#elif defined(Q_OS_MACOS)
            // Parse plist XML to extract Label, ProgramArguments
            QString content = lines.join("\n");
            QRegularExpression labelReg("<key>Label</key>\\s*<string>(.*?)</string>");
            QRegularExpression progReg("<key>ProgramArguments</key>\\s*<array>\\s*<string>(.*?)</string>");

            QRegularExpressionMatch labelMatch = labelReg.match(content);
            if (labelMatch.hasMatch()) {
                ui->txtStartupAppName->setText(labelMatch.captured(1));
            }

            QRegularExpressionMatch progMatch = progReg.match(content);
            if (progMatch.hasMatch()) {
                ui->txtStartupAppCommand->setText(progMatch.captured(1));
            }

            // Comment field: not standard in plists, leave empty or use Label
            ui->txtStartupAppComment->setText(ui->txtStartupAppName->text());
#endif
        }
    }

    QDialog::show();
}

void StartupAppEdit::changeDesktopValue(QStringList &lines, const QRegularExpression &reg, const QString &text)
{
    int pos = lines.indexOf(reg);

    if (pos != -1) {
        lines.replace(pos, text);
    } else {
        lines.append(text);
    }
}

void StartupAppEdit::on_btnSave_clicked()
{
    if(isValid()) {
        if(! selectedFilePath.isEmpty()) {
#ifdef Q_OS_LINUX
            QStringList lines = FileUtil::readListFromFile(selectedFilePath);

            changeDesktopValue(lines, NAME_REG, QString("Name=%1").arg(ui->txtStartupAppName->text()));
            changeDesktopValue(lines, COMMENT_REG, QString("Comment=%1").arg(ui->txtStartupAppComment->text()));
            changeDesktopValue(lines, EXEC_REG, QString("Exec=%1").arg(ui->txtStartupAppCommand->text()));

            FileUtil::writeFile(selectedFilePath, lines.join("\n"), QIODevice::ReadWrite | QIODevice::Truncate);
#elif defined(Q_OS_MACOS)
            // For macOS, rewrite the plist with updated values
            QString appContent = mNewAppTemplate
                    .arg(ui->txtStartupAppName->text())
                    .arg(ui->txtStartupAppComment->text())
                    .arg(ui->txtStartupAppCommand->text());

            FileUtil::writeFile(selectedFilePath, appContent, QIODevice::ReadWrite | QIODevice::Truncate);
#endif
        }
        else {
            // new file content
            QString appContent = mNewAppTemplate
                    .arg(ui->txtStartupAppName->text())
                    .arg(ui->txtStartupAppComment->text())
                    .arg(ui->txtStartupAppCommand->text());

            // file name
            QString appFileName = ui->txtStartupAppName->text()
                    .simplified()
                    .replace(' ', '-')
                    .toLower();

            qDebug() << appFileName;

#ifdef Q_OS_LINUX
            QString path = QString("%1/%2.desktop").arg(mAutostartPath).arg(appFileName);
#elif defined(Q_OS_MACOS)
            QString path = QString("%1/%2.plist").arg(mAutostartPath).arg(appFileName);
#endif

            FileUtil::writeFile(path, appContent);
        }

        emit startupAppAdded(); // signal
        close();
    }
    else {
        ui->lblErrorMsg->show();
    }

    selectedFilePath = "";
}

bool StartupAppEdit::isValid()
{
    return ! ui->txtStartupAppName->text().isEmpty() &&
           ! ui->txtStartupAppComment->text().isEmpty() &&
           ! ui->txtStartupAppCommand->text().isEmpty();
}
