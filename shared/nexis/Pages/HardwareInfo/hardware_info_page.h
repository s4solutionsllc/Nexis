#ifndef HARDWARE_INFO_PAGE_H
#define HARDWARE_INFO_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QToolButton>

#include "Managers/info_manager.h"

namespace Ui {
    class HardwareInfoPage;
}

class HardwareInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit HardwareInfoPage(QWidget *parent = nullptr,
                              InfoManager *infoManager = nullptr);
    ~HardwareInfoPage();

private slots:
    void init();
    void on_btnExportReport_clicked();
    void onCopyGpuDiagnostics();

private:
    void populateSystem();
    void populateProcessor();
    void populateGraphics();
    void populateMemory();
    void populateBattery();
    void populateFans();
    void populateStorage();

    void addRow(QTableWidget *table, const QString &label, const QString &value);
    void fitTableHeight(QTableWidget *table);
    void refreshThemeColors();

    struct HealthItem {
        QTableWidgetItem *item;
        QString verdict;
    };

private:
    Ui::HardwareInfoPage *ui;
    InfoManager *im;
    QList<HealthItem> mHealthItems;
};

#endif // HARDWARE_INFO_PAGE_H
