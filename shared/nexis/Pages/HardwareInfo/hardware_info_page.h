#ifndef HARDWARE_INFO_PAGE_H
#define HARDWARE_INFO_PAGE_H

#include <QWidget>
#include <QTableWidget>

#include "Managers/info_manager.h"

namespace Ui {
    class HardwareInfoPage;
}

class HardwareInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit HardwareInfoPage(QWidget *parent = 0);
    ~HardwareInfoPage();

private slots:
    void init();

private:
    void populateSystem();
    void populateProcessor();
    void populateGraphics();
    void populateMemory();

    void addRow(QTableWidget *table, const QString &label, const QString &value);
    void fitTableHeight(QTableWidget *table);

private:
    Ui::HardwareInfoPage *ui;
    InfoManager *im;
};

#endif // HARDWARE_INFO_PAGE_H
