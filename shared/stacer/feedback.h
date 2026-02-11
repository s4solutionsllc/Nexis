#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <QDialog>

namespace Ui {
class Feedback;
}

class Feedback : public QDialog
{
    Q_OBJECT

public:
    explicit Feedback(QWidget *parent = nullptr);
    ~Feedback();

private slots:
    void on_btnReportBug_clicked();
    void on_btnRequestFeature_clicked();
    void on_btnFeedback_clicked();
    void on_btnClose_clicked();

private:
    Ui::Feedback *ui;

    static const QString ISSUES_BASE_URL;
};

#endif // FEEDBACK_H
