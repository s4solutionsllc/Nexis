/********************************************************************************
** Form generated from reading UI file 'feedback.ui'
**
** Created by: Qt User Interface Compiler version 6.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FEEDBACK_H
#define UI_FEEDBACK_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

QT_BEGIN_NAMESPACE

class Ui_Feedback
{
public:
    QGridLayout *gridLayout;
    QSpacerItem *verticalSpacer;
    QPlainTextEdit *txtMessage;
    QLineEdit *txtName;
    QLineEdit *txtEmail;
    QLabel *lblTitle;
    QPushButton *btnSend;
    QLabel *lblErrorMsg;
    QPushButton *btnClose;

    void setupUi(QDialog *Feedback)
    {
        if (Feedback->objectName().isEmpty())
            Feedback->setObjectName("Feedback");
        Feedback->resize(476, 350);
        Feedback->setModal(true);
        gridLayout = new QGridLayout(Feedback);
        gridLayout->setSpacing(15);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(30, 20, 30, 20);
        verticalSpacer = new QSpacerItem(359, 2, QSizePolicy::Minimum, QSizePolicy::Maximum);

        gridLayout->addItem(verticalSpacer, 5, 0, 1, 3);

        txtMessage = new QPlainTextEdit(Feedback);
        txtMessage->setObjectName("txtMessage");

        gridLayout->addWidget(txtMessage, 3, 0, 1, 3);

        txtName = new QLineEdit(Feedback);
        txtName->setObjectName("txtName");

        gridLayout->addWidget(txtName, 1, 0, 1, 3);

        txtEmail = new QLineEdit(Feedback);
        txtEmail->setObjectName("txtEmail");

        gridLayout->addWidget(txtEmail, 2, 0, 1, 3);

        lblTitle = new QLabel(Feedback);
        lblTitle->setObjectName("lblTitle");
#if QT_CONFIG(accessibility)
        lblTitle->setAccessibleName(QString::fromUtf8("dialog-title"));
#endif // QT_CONFIG(accessibility)
        lblTitle->setAlignment(Qt::AlignCenter);

        gridLayout->addWidget(lblTitle, 0, 0, 1, 3);

        btnSend = new QPushButton(Feedback);
        btnSend->setObjectName("btnSend");
        btnSend->setCursor(QCursor(Qt::PointingHandCursor));
        btnSend->setFocusPolicy(Qt::NoFocus);
#if QT_CONFIG(accessibility)
        btnSend->setAccessibleName(QString::fromUtf8("primary"));
#endif // QT_CONFIG(accessibility)

        gridLayout->addWidget(btnSend, 4, 2, 1, 1);

        lblErrorMsg = new QLabel(Feedback);
        lblErrorMsg->setObjectName("lblErrorMsg");

        gridLayout->addWidget(lblErrorMsg, 4, 0, 1, 1);

        btnClose = new QPushButton(Feedback);
        btnClose->setObjectName("btnClose");
        btnClose->setCursor(QCursor(Qt::PointingHandCursor));

        gridLayout->addWidget(btnClose, 4, 1, 1, 1);

        QWidget::setTabOrder(txtName, txtEmail);
        QWidget::setTabOrder(txtEmail, txtMessage);

        retranslateUi(Feedback);

        btnSend->setDefault(true);


        QMetaObject::connectSlotsByName(Feedback);
    } // setupUi

    void retranslateUi(QDialog *Feedback)
    {
        Feedback->setWindowTitle(QCoreApplication::translate("Feedback", "Feedback", nullptr));
        txtMessage->setPlaceholderText(QCoreApplication::translate("Feedback", "Message", nullptr));
        txtName->setPlaceholderText(QCoreApplication::translate("Feedback", "Name", nullptr));
        txtEmail->setPlaceholderText(QCoreApplication::translate("Feedback", "Email Address", nullptr));
        lblTitle->setText(QCoreApplication::translate("Feedback", "Send a Feedback", nullptr));
        btnSend->setText(QCoreApplication::translate("Feedback", "Send", nullptr));
        lblErrorMsg->setText(QString());
#if QT_CONFIG(accessibility)
        btnClose->setAccessibleName(QCoreApplication::translate("Feedback", "danger", nullptr));
#endif // QT_CONFIG(accessibility)
        btnClose->setText(QCoreApplication::translate("Feedback", "Close", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Feedback: public Ui_Feedback {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FEEDBACK_H
