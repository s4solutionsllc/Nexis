/********************************************************************************
** Form generated from reading UI file 'helpers_page.ui'
**
** Created by: Qt User Interface Compiler version 6.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HELPERS_PAGE_H
#define UI_HELPERS_PAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_HelpersPage
{
public:
    QGridLayout *gridLayout;
    QStackedWidget *stackedWidget;
    QWidget *nav;
    QHBoxLayout *navLayout;
    QPushButton *btnHostManage;
    QSpacerItem *horizontalSpacer;
    QButtonGroup *buttonGroup;

    void setupUi(QWidget *HelpersPage)
    {
        if (HelpersPage->objectName().isEmpty())
            HelpersPage->setObjectName("HelpersPage");
        HelpersPage->resize(839, 590);
        gridLayout = new QGridLayout(HelpersPage);
        gridLayout->setSpacing(10);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(15, 10, 15, 10);
        stackedWidget = new QStackedWidget(HelpersPage);
        stackedWidget->setObjectName("stackedWidget");

        gridLayout->addWidget(stackedWidget, 2, 0, 1, 3);

        nav = new QWidget(HelpersPage);
        nav->setObjectName("nav");
        navLayout = new QHBoxLayout(nav);
        navLayout->setSpacing(12);
        navLayout->setObjectName("navLayout");
        navLayout->setContentsMargins(0, 0, 0, 0);
        btnHostManage = new QPushButton(nav);
        buttonGroup = new QButtonGroup(HelpersPage);
        buttonGroup->setObjectName("buttonGroup");
        buttonGroup->addButton(btnHostManage);
        btnHostManage->setObjectName("btnHostManage");
        btnHostManage->setCursor(QCursor(Qt::PointingHandCursor));
        btnHostManage->setFocusPolicy(Qt::NoFocus);
        btnHostManage->setCheckable(true);
        btnHostManage->setChecked(true);

        navLayout->addWidget(btnHostManage, 0, Qt::AlignLeft);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        navLayout->addItem(horizontalSpacer);


        gridLayout->addWidget(nav, 0, 0, 1, 3);


        retranslateUi(HelpersPage);

        QMetaObject::connectSlotsByName(HelpersPage);
    } // setupUi

    void retranslateUi(QWidget *HelpersPage)
    {
        HelpersPage->setWindowTitle(QCoreApplication::translate("HelpersPage", "Helpers", nullptr));
        btnHostManage->setText(QCoreApplication::translate("HelpersPage", "Host Manage", nullptr));
    } // retranslateUi

};

namespace Ui {
    class HelpersPage: public Ui_HelpersPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HELPERS_PAGE_H
