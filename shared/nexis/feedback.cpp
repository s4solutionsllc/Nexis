#include "feedback.h"
#include "ui_feedback.h"

#include <QDesktopServices>
#include <QUrl>

const QString Feedback::ISSUES_BASE_URL =
    QStringLiteral("https://github.com/lsimpsonsfdc/Nexis/issues/new?template=");

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

void Feedback::on_btnReportBug_clicked()
{
    QDesktopServices::openUrl(QUrl(ISSUES_BASE_URL + "bug_report.yml"));
    close();
}

void Feedback::on_btnRequestFeature_clicked()
{
    QDesktopServices::openUrl(QUrl(ISSUES_BASE_URL + "feature_request.yml"));
    close();
}

void Feedback::on_btnFeedback_clicked()
{
    QDesktopServices::openUrl(QUrl(ISSUES_BASE_URL + "feedback.yml"));
    close();
}

void Feedback::on_btnClose_clicked()
{
    close();
}
