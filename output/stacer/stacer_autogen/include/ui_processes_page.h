/********************************************************************************
** Form generated from reading UI file 'processes_page.ui'
**
** Created by: Qt User Interface Compiler version 6.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PROCESSES_PAGE_H
#define UI_PROCESSES_PAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ProcessesPage
{
public:
    QGridLayout *gridLayout;
    QHBoxLayout *topLayout;
    QLabel *lblProcessTitle;
    QCheckBox *checkAllProcesses;
    QSpacerItem *horizontalSpacer;
    QLineEdit *txtProcessSearch;
    QTableView *tableProcess;
    QWidget *bottomWidget;
    QHBoxLayout *bottomLayout;
    QLabel *lblRefresh;
    QSlider *sliderRefresh;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *btnEndProcess;

    void setupUi(QWidget *ProcessesPage)
    {
        if (ProcessesPage->objectName().isEmpty())
            ProcessesPage->setObjectName("ProcessesPage");
        ProcessesPage->resize(835, 612);
        ProcessesPage->setStyleSheet(QString::fromUtf8(""));
        gridLayout = new QGridLayout(ProcessesPage);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setHorizontalSpacing(0);
        gridLayout->setVerticalSpacing(5);
        gridLayout->setContentsMargins(20, 5, 20, 20);
        topLayout = new QHBoxLayout();
        topLayout->setSpacing(10);
        topLayout->setObjectName("topLayout");
        topLayout->setContentsMargins(5, 0, 10, -1);
        lblProcessTitle = new QLabel(ProcessesPage);
        lblProcessTitle->setObjectName("lblProcessTitle");

        topLayout->addWidget(lblProcessTitle);

        checkAllProcesses = new QCheckBox(ProcessesPage);
        checkAllProcesses->setObjectName("checkAllProcesses");
        checkAllProcesses->setCursor(QCursor(Qt::PointingHandCursor));
        checkAllProcesses->setFocusPolicy(Qt::NoFocus);
#if QT_CONFIG(accessibility)
        checkAllProcesses->setAccessibleName(QString::fromUtf8("circle"));
#endif // QT_CONFIG(accessibility)

        topLayout->addWidget(checkAllProcesses);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        topLayout->addItem(horizontalSpacer);

        txtProcessSearch = new QLineEdit(ProcessesPage);
        txtProcessSearch->setObjectName("txtProcessSearch");
        QFont font;
        font.setPointSize(10);
        txtProcessSearch->setFont(font);

        topLayout->addWidget(txtProcessSearch, 0, Qt::AlignRight);


        gridLayout->addLayout(topLayout, 1, 0, 1, 1);

        tableProcess = new QTableView(ProcessesPage);
        tableProcess->setObjectName("tableProcess");
        tableProcess->setFocusPolicy(Qt::NoFocus);
        tableProcess->setFrameShape(QFrame::NoFrame);
        tableProcess->setFrameShadow(QFrame::Sunken);
        tableProcess->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        tableProcess->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableProcess->setSelectionMode(QAbstractItemView::SingleSelection);
        tableProcess->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableProcess->setTextElideMode(Qt::ElideMiddle);
        tableProcess->setGridStyle(Qt::SolidLine);
        tableProcess->setSortingEnabled(true);
        tableProcess->setWordWrap(true);
        tableProcess->setCornerButtonEnabled(true);
        tableProcess->horizontalHeader()->setCascadingSectionResizes(false);
        tableProcess->horizontalHeader()->setStretchLastSection(true);
        tableProcess->verticalHeader()->setVisible(false);

        gridLayout->addWidget(tableProcess, 3, 0, 1, 1);

        bottomWidget = new QWidget(ProcessesPage);
        bottomWidget->setObjectName("bottomWidget");
        bottomLayout = new QHBoxLayout(bottomWidget);
        bottomLayout->setSpacing(10);
        bottomLayout->setObjectName("bottomLayout");
        bottomLayout->setContentsMargins(0, 5, 0, 0);
        lblRefresh = new QLabel(bottomWidget);
        lblRefresh->setObjectName("lblRefresh");
        lblRefresh->setText(QString::fromUtf8("Refresh (1)"));

        bottomLayout->addWidget(lblRefresh, 0, Qt::AlignLeft|Qt::AlignVCenter);

        sliderRefresh = new QSlider(bottomWidget);
        sliderRefresh->setObjectName("sliderRefresh");
        sliderRefresh->setCursor(QCursor(Qt::PointingHandCursor));
        sliderRefresh->setFocusPolicy(Qt::NoFocus);
        sliderRefresh->setStyleSheet(QString::fromUtf8(""));
        sliderRefresh->setOrientation(Qt::Horizontal);

        bottomLayout->addWidget(sliderRefresh, 0, Qt::AlignLeft|Qt::AlignVCenter);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        bottomLayout->addItem(horizontalSpacer_2);

        btnEndProcess = new QPushButton(bottomWidget);
        btnEndProcess->setObjectName("btnEndProcess");
        btnEndProcess->setCursor(QCursor(Qt::PointingHandCursor));
        btnEndProcess->setFocusPolicy(Qt::NoFocus);
#if QT_CONFIG(accessibility)
        btnEndProcess->setAccessibleName(QString::fromUtf8("primary"));
#endif // QT_CONFIG(accessibility)

        bottomLayout->addWidget(btnEndProcess);


        gridLayout->addWidget(bottomWidget, 4, 0, 1, 1);


        retranslateUi(ProcessesPage);

        QMetaObject::connectSlotsByName(ProcessesPage);
    } // setupUi

    void retranslateUi(QWidget *ProcessesPage)
    {
        ProcessesPage->setWindowTitle(QCoreApplication::translate("ProcessesPage", "Processes", nullptr));
        lblProcessTitle->setText(QCoreApplication::translate("ProcessesPage", "Processes", nullptr));
        checkAllProcesses->setText(QCoreApplication::translate("ProcessesPage", "All Processes", nullptr));
        txtProcessSearch->setPlaceholderText(QCoreApplication::translate("ProcessesPage", "Search...", nullptr));
        btnEndProcess->setText(QCoreApplication::translate("ProcessesPage", "End Process", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ProcessesPage: public Ui_ProcessesPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PROCESSES_PAGE_H
