#ifndef APP_CLEANERS_DIALOG_H
#define APP_CLEANERS_DIALOG_H

#include <QDialog>
#include <QHash>
#include <QSet>

#include <Tools/cleanerml_model.h>

class QLineEdit;
class QPushButton;
class QTreeWidget;

// SSO-25782: picks which per-application cleaner options (CleanerML) to run.
// It deletes nothing itself; the caller runs the selection through the Trust
// & Safety preview dialog.
class AppCleanersDialog : public QDialog
{
    Q_OBJECT

public:
    // `runningIds`: cleaners whose application is open; they are listed but
    // cannot be selected.
    AppCleanersDialog(const QList<CleanerML::Cleaner> &cleaners, const QSet<QString> &runningIds,
                      QWidget *parent = nullptr);

    // cleaner id -> selected option ids; cleaners with nothing ticked are absent.
    QHash<QString, QSet<QString>> selection() const;

private slots:
    void applyFilter(const QString &text);
    void updateReviewEnabled();

private:
    QTreeWidget *mTree;
    QLineEdit *mEditFilter;
    QPushButton *mBtnReview;
};

#endif // APP_CLEANERS_DIALOG_H
