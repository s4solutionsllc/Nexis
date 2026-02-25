#include "helpers_page.h"
#include "ui_helpers_page.h"

#include <Utils/command_util.h>
#include <QMessageBox>

HelpersPage::~HelpersPage()
{
    delete ui;
}

HelpersPage::HelpersPage(QWidget *parent) :
    QWidget(parent),
    widgetHostManage(new HostManage),
    ui(new Ui::HelpersPage)
{
    ui->setupUi(this);

    init();
}

void HelpersPage::init()
{
    ui->stackedWidget->addWidget(widgetHostManage);
    //ui->stackedWidget->addWidget();

    Utilities::addDropShadow({
        ui->btnHostManage,
        ui->btnFlushDNS
    }, 40);
}

void HelpersPage::on_btnHostManage_clicked()
{
    widgetHostManage->loadIfNeeded();
    ui->stackedWidget->setCurrentIndex(0);
}

void HelpersPage::on_btnFlushDNS_clicked()
{
    if (QMessageBox::question(this, tr("Flush DNS Cache"),
            tr("This will clear the local DNS cache. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    QString errorMsg;
    bool success = false;

#ifdef Q_OS_MACOS
    try {
        CommandUtil::exec("dscacheutil", {"-flushcache"});
        CommandUtil::sudoExec("killall", {"-HUP", "mDNSResponder"});
        success = true;
    } catch (const QString &ex) {
        errorMsg = ex;
    }
#else
    // Linux: try resolvers in order of likelihood
    if (CommandUtil::isExecutable("resolvectl")) {
        ExecResult r = CommandUtil::execWithStatus("resolvectl", {"flush-caches"});
        success = (r.exitCode == 0);
        if (!success) errorMsg = r.error;
    } else if (CommandUtil::isExecutable("systemd-resolve")) {
        ExecResult r = CommandUtil::execWithStatus("systemd-resolve", {"--flush-caches"});
        success = (r.exitCode == 0);
        if (!success) errorMsg = r.error;
    } else if (CommandUtil::isExecutable("nscd")) {
        ExecResult r = CommandUtil::execWithStatus("nscd", {"-i", "hosts"});
        success = (r.exitCode == 0);
        if (!success) errorMsg = r.error;
    } else {
        errorMsg = tr("No DNS cache service detected (systemd-resolved, nscd).");
    }
#endif

    if (success) {
        QMessageBox::information(this, tr("DNS Cache Flushed"),
            tr("The local DNS cache has been cleared successfully."));
    } else {
        QMessageBox::warning(this, tr("DNS Flush Failed"),
            tr("Could not flush DNS cache: %1").arg(errorMsg));
    }
}
