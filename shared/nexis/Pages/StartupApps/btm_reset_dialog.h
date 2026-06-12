// SSO-3738 / FW-10: confirmation dialog for `sudo sfltool resetbtm`.
// resetbtm wipes the Background Task Management database and forces every
// login item / launchd record to re-prompt the user. Risky enough that we
// require the user to type RESET to enable the action button.

#ifndef BTM_RESET_DIALOG_H
#define BTM_RESET_DIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;

class BtmResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BtmResetDialog(QWidget *parent = nullptr);

    static constexpr const char *kConfirmToken = "RESET";

private slots:
    void onConfirmTextChanged(const QString &text);

private:
    QLineEdit *mConfirmEdit = nullptr;
    QPushButton *mResetBtn = nullptr;
};

#endif // BTM_RESET_DIALOG_H
