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
    QScreen *screen = qApp->primaryScreen();
    if (screen) {
        setGeometry(
            QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                size(), screen->availableGeometry())
        );
    }

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
    ui->spnStartupDelay->setValue(0);
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
                QString cmd = progMatch.captured(1);
                // Detect shell-wrapped delay: /bin/bash -c "sleep N && <cmd>"
                QRegularExpression sleepReg("^/bin/bash$");
                if (sleepReg.match(cmd).hasMatch()) {
                    QRegularExpression argReg("sleep\\s+(\\d+)\\s*&&\\s*(.+)");
                    QRegularExpressionMatch argMatch = argReg.match(content);
                    if (argMatch.hasMatch()) {
                        ui->spnStartupDelay->setValue(argMatch.captured(1).toInt());
                        ui->txtStartupAppCommand->setText(argMatch.captured(2).trimmed());
                    } else {
                        ui->txtStartupAppCommand->setText(cmd);
                    }
                } else {
                    ui->txtStartupAppCommand->setText(cmd);
                }
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

QString StartupAppEdit::buildPlistContent()
{
    int delay = ui->spnStartupDelay->value();
    QString label = ui->txtStartupAppName->text();
    QString cmd = ui->txtStartupAppCommand->text();

    QString plist;
    plist += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\">\n"
             "<dict>\n"
             "    <key>Label</key>\n"
             "    <string>" + label + "</string>\n"
             "    <key>ProgramArguments</key>\n"
             "    <array>\n";

    if (delay > 0) {
        plist += "        <string>/bin/bash</string>\n"
                 "        <string>-c</string>\n"
                 "        <string>sleep " + QString::number(delay) + " &amp;&amp; " + cmd + "</string>\n";
    } else {
        plist += "        <string>" + cmd + "</string>\n";
    }

    plist += "    </array>\n"
             "    <key>RunAtLoad</key>\n"
             "    <true/>\n"
             "</dict>\n"
             "</plist>\n";

    return plist;
}

void StartupAppEdit::on_btnSave_clicked()
{
    if(isValid()) {
        if(! selectedFilePath.isEmpty()) {
            QString appContent = buildPlistContent();
            FileUtil::writeFile(selectedFilePath, appContent, QIODevice::ReadWrite | QIODevice::Truncate);
        }
        else {
            QString appContent = buildPlistContent();

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
