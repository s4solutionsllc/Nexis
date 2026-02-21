#ifndef NEXIS_PAGE_H
#define NEXIS_PAGE_H

#include <QWidget>

class NexisPage : public QWidget
{
    Q_OBJECT

public:
    explicit NexisPage(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual void onPageActivated() {}
    virtual void onPageDeactivated() {}
};

#endif // NEXIS_PAGE_H
