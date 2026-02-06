/********************************************************************************
** Form generated from reading UI file 'host_manage.ui'
**
** Created by: Qt User Interface Compiler version 6.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HOST_MANAGE_H
#define UI_HOST_MANAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_HostManage
{
public:
    QGridLayout *gridLayout;
    QLabel *lblErrorMsg;
    QLabel *lblChangesMsg;
    QPushButton *btnSaveChanges;
    QHBoxLayout *horizontalLayout_2;
    QLabel *lblHostTitle;
    QSpacerItem *horizontalSpacer;
    QPushButton *btnNewHost;
    QWidget *widgetAddEditHost;
    QHBoxLayout *horizontalLayout;
    QLineEdit *txtIP;
    QLineEdit *txtFullyQualified;
    QLineEdit *txtAliases;
    QPushButton *btnSave;
    QPushButton *btnCancel;
    QTableView *tableViewHosts;

    void setupUi(QWidget *HostManage)
    {
        if (HostManage->objectName().isEmpty())
            HostManage->setObjectName("HostManage");
        HostManage->resize(804, 534);
        gridLayout = new QGridLayout(HostManage);
        gridLayout->setSpacing(10);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(5, 0, 5, 10);
        lblErrorMsg = new QLabel(HostManage);
        lblErrorMsg->setObjectName("lblErrorMsg");
        QFont font;
        font.setPointSize(10);
        lblErrorMsg->setFont(font);

        gridLayout->addWidget(lblErrorMsg, 2, 0, 1, 1);

        lblChangesMsg = new QLabel(HostManage);
        lblChangesMsg->setObjectName("lblChangesMsg");

        gridLayout->addWidget(lblChangesMsg, 5, 0, 1, 1);

        btnSaveChanges = new QPushButton(HostManage);
        btnSaveChanges->setObjectName("btnSaveChanges");
        btnSaveChanges->setCursor(QCursor(Qt::PointingHandCursor));
        btnSaveChanges->setFocusPolicy(Qt::NoFocus);

        gridLayout->addWidget(btnSaveChanges, 5, 1, 1, 1, Qt::AlignRight);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(10);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        horizontalLayout_2->setContentsMargins(0, 0, -1, -1);
        lblHostTitle = new QLabel(HostManage);
        lblHostTitle->setObjectName("lblHostTitle");

        horizontalLayout_2->addWidget(lblHostTitle, 0, Qt::AlignLeft);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        btnNewHost = new QPushButton(HostManage);
        btnNewHost->setObjectName("btnNewHost");
        btnNewHost->setCursor(QCursor(Qt::PointingHandCursor));
        btnNewHost->setFocusPolicy(Qt::NoFocus);
        btnNewHost->setCheckable(true);
        btnNewHost->setFlat(true);

        horizontalLayout_2->addWidget(btnNewHost, 0, Qt::AlignRight);


        gridLayout->addLayout(horizontalLayout_2, 0, 0, 1, 2);

        widgetAddEditHost = new QWidget(HostManage);
        widgetAddEditHost->setObjectName("widgetAddEditHost");
        horizontalLayout = new QHBoxLayout(widgetAddEditHost);
        horizontalLayout->setSpacing(8);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        txtIP = new QLineEdit(widgetAddEditHost);
        txtIP->setObjectName("txtIP");
        txtIP->setFocusPolicy(Qt::StrongFocus);

        horizontalLayout->addWidget(txtIP);

        txtFullyQualified = new QLineEdit(widgetAddEditHost);
        txtFullyQualified->setObjectName("txtFullyQualified");

        horizontalLayout->addWidget(txtFullyQualified);

        txtAliases = new QLineEdit(widgetAddEditHost);
        txtAliases->setObjectName("txtAliases");

        horizontalLayout->addWidget(txtAliases);

        btnSave = new QPushButton(widgetAddEditHost);
        btnSave->setObjectName("btnSave");
        btnSave->setCursor(QCursor(Qt::PointingHandCursor));
        btnSave->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(btnSave);

        btnCancel = new QPushButton(widgetAddEditHost);
        btnCancel->setObjectName("btnCancel");
        btnCancel->setCursor(QCursor(Qt::PointingHandCursor));
        btnCancel->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(btnCancel);


        gridLayout->addWidget(widgetAddEditHost, 1, 0, 1, 2);

        tableViewHosts = new QTableView(HostManage);
        tableViewHosts->setObjectName("tableViewHosts");
        tableViewHosts->setFocusPolicy(Qt::NoFocus);
        tableViewHosts->setFrameShape(QFrame::NoFrame);
        tableViewHosts->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableViewHosts->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableViewHosts->setTextElideMode(Qt::ElideMiddle);
        tableViewHosts->setSortingEnabled(true);
        tableViewHosts->horizontalHeader()->setProperty("showSortIndicator", QVariant(true));
        tableViewHosts->horizontalHeader()->setStretchLastSection(true);
        tableViewHosts->verticalHeader()->setVisible(false);

        gridLayout->addWidget(tableViewHosts, 3, 0, 1, 2);


        retranslateUi(HostManage);

        QMetaObject::connectSlotsByName(HostManage);
    } // setupUi

    void retranslateUi(QWidget *HostManage)
    {
        HostManage->setWindowTitle(QCoreApplication::translate("HostManage", "Form", nullptr));
        lblErrorMsg->setText(QString());
        lblChangesMsg->setText(QString());
#if QT_CONFIG(accessibility)
        btnSaveChanges->setAccessibleName(QCoreApplication::translate("HostManage", "primary", nullptr));
#endif // QT_CONFIG(accessibility)
        btnSaveChanges->setText(QCoreApplication::translate("HostManage", "Save Changes", nullptr));
        lblHostTitle->setText(QString());
#if QT_CONFIG(accessibility)
        btnNewHost->setAccessibleName(QString());
#endif // QT_CONFIG(accessibility)
        btnNewHost->setText(QCoreApplication::translate("HostManage", "New Host", nullptr));
        txtIP->setPlaceholderText(QCoreApplication::translate("HostManage", "IP Address *", nullptr));
        txtFullyQualified->setPlaceholderText(QCoreApplication::translate("HostManage", "Fully Qualified Name *", nullptr));
        txtAliases->setPlaceholderText(QCoreApplication::translate("HostManage", "Aliases", nullptr));
#if QT_CONFIG(accessibility)
        btnSave->setAccessibleName(QCoreApplication::translate("HostManage", "primary", nullptr));
#endif // QT_CONFIG(accessibility)
        btnSave->setText(QCoreApplication::translate("HostManage", "Save", nullptr));
#if QT_CONFIG(accessibility)
        btnCancel->setAccessibleName(QCoreApplication::translate("HostManage", "danger", nullptr));
#endif // QT_CONFIG(accessibility)
        btnCancel->setText(QCoreApplication::translate("HostManage", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class HostManage: public Ui_HostManage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HOST_MANAGE_H
