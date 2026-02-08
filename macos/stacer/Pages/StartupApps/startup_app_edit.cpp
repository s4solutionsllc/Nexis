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

    mAutostartPath = QDir::homePath() + "/Library/LaunchAgents";

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
            // For macOS, rewrite the plist with updated values
            QString appContent = mNewAppTemplate
                    .arg(ui->txtStartupAppName->text())
                    .arg(ui->txtStartupAppComment->text())
                    .arg(ui->txtStartupAppCommand->text());

            FileUtil::writeFile(selectedFilePath, appContent, QIODevice::ReadWrite | QIODevice::Truncate);
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

            QString path = QString("%1/%2.plist").arg(mAutostartPath).arg(appFileName);

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
