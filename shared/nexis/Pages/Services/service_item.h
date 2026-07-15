#ifndef SERVICE_ITEM_H
#define SERVICE_ITEM_H

#include <QWidget>
#include <QDebug>
#include "Managers/tool_manager.h"

namespace Ui {
    class ServiceItem;
}

class QResizeEvent;

class ServiceItem : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceItem(const QString &name, const QString description, const bool status, const bool active, QWidget *parent = 0);
    ~ServiceItem();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void on_checkServiceRunning_clicked(bool status);
    void on_checkServiceStartup_clicked(bool status);

private:
    void updateDescriptionElision();

private:
    Ui::ServiceItem *ui;

private:
    ToolManager *tm;
    QString mDescription;
};

#endif // SERVICE_ITEM_H
