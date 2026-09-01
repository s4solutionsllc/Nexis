#include "feedback.h"
#include "ui_feedback.h"

#include <QDesktopServices>
#include <QThreadPool>
#include <QUrl>

const QString Feedback::ISSUES_BASE_URL =
    QStringLiteral("https://github.com/s4solutionsllc/Nexis/issues/new?template=");

Feedback::~Feedback()
{
    delete ui;
}

Feedback::Feedback(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Feedback)
{
    ui->setupUi(this);
}

void Feedback::openIssueTemplate(const QString &templateFile)
{
    const QUrl url(ISSUES_BASE_URL + templateFile);
#ifdef Q_OS_LINUX
    // QDesktopServices::openUrl() can block the calling thread on some Linux
    // desktops (e.g. a registered but unresponsive xdg-desktop-portal backend,
    // GH#424) — dispatch it off the UI thread so the dialog always closes
    // immediately regardless of what the URL handler does.
    QThreadPool::globalInstance()->start([url]() { QDesktopServices::openUrl(url); });
#else
    QDesktopServices::openUrl(url);
#endif
    close();
}

void Feedback::on_btnReportBug_clicked()
{
    openIssueTemplate("bug_report.yml");
}

void Feedback::on_btnRequestFeature_clicked()
{
    openIssueTemplate("feature_request.yml");
}

void Feedback::on_btnFeedback_clicked()
{
    openIssueTemplate("feedback.yml");
}

void Feedback::on_btnClose_clicked()
{
    close();
}
