/********************************************************************************
** Form generated from reading UI file 'history_chart.ui'
**
** Created by: Qt User Interface Compiler version 6.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HISTORY_CHART_H
#define UI_HISTORY_CHART_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_HistoryChart
{
public:
    QGridLayout *layoutHistoryChart;
    QCheckBox *checkHistoryTitle;
    QLabel *lblHistoryTitle;
    QSpacerItem *horizontalSpacer;

    void setupUi(QWidget *HistoryChart)
    {
        if (HistoryChart->objectName().isEmpty())
            HistoryChart->setObjectName("HistoryChart");
        HistoryChart->resize(759, 275);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(HistoryChart->sizePolicy().hasHeightForWidth());
        HistoryChart->setSizePolicy(sizePolicy);
        HistoryChart->setMinimumSize(QSize(0, 200));
        HistoryChart->setWindowTitle(QString::fromUtf8(""));
        layoutHistoryChart = new QGridLayout(HistoryChart);
        layoutHistoryChart->setObjectName("layoutHistoryChart");
        layoutHistoryChart->setHorizontalSpacing(10);
        layoutHistoryChart->setVerticalSpacing(0);
        layoutHistoryChart->setContentsMargins(0, 0, 0, 0);
        checkHistoryTitle = new QCheckBox(HistoryChart);
        checkHistoryTitle->setObjectName("checkHistoryTitle");
        checkHistoryTitle->setCursor(QCursor(Qt::PointingHandCursor));
        checkHistoryTitle->setFocusPolicy(Qt::NoFocus);
        checkHistoryTitle->setLayoutDirection(Qt::LeftToRight);
        checkHistoryTitle->setStyleSheet(QString::fromUtf8(""));
        checkHistoryTitle->setText(QString::fromUtf8(""));

        layoutHistoryChart->addWidget(checkHistoryTitle, 0, 1, 1, 1);

        lblHistoryTitle = new QLabel(HistoryChart);
        lblHistoryTitle->setObjectName("lblHistoryTitle");
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(lblHistoryTitle->sizePolicy().hasHeightForWidth());
        lblHistoryTitle->setSizePolicy(sizePolicy1);
#if QT_CONFIG(accessibility)
        lblHistoryTitle->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        lblHistoryTitle->setText(QString::fromUtf8("Chart Title"));

        layoutHistoryChart->addWidget(lblHistoryTitle, 0, 0, 1, 1, Qt::AlignLeft|Qt::AlignTop);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        layoutHistoryChart->addItem(horizontalSpacer, 0, 2, 1, 1);


        retranslateUi(HistoryChart);

        QMetaObject::connectSlotsByName(HistoryChart);
    } // setupUi

    void retranslateUi(QWidget *HistoryChart)
    {
        (void)HistoryChart;
    } // retranslateUi

};

namespace Ui {
    class HistoryChart: public Ui_HistoryChart {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HISTORY_CHART_H
