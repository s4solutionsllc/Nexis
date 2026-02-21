#ifndef SERVICESPAGE_H
#define SERVICESPAGE_H

#include <QWidget>

class SystemServiceManager;

namespace Ui {
    class ServicesPage;
}

class ServicesPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServicesPage(QWidget *parent = nullptr,
                          SystemServiceManager *serviceManager = nullptr);
    ~ServicesPage();

private slots:
    void init();
    void loadServices();

    void on_cmbRunningStatus_currentIndexChanged(int index);
    void on_cmbStartupStatus_currentIndexChanged(int index);

public slots:
    void setServiceCount();

private:
    Ui::ServicesPage *ui;
    SystemServiceManager *mServiceManager;
};

#endif // SERVICESPAGE_H
