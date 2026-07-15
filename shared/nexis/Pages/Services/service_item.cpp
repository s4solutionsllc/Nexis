#include "service_item.h"
#include "ui_service_item.h"

#include <QFontMetrics>
#include <QResizeEvent>

ServiceItem::~ServiceItem()
{
    delete ui;
}

ServiceItem::ServiceItem(const QString &name,
                         const QString description,
                         const bool status,
                         const bool active,
                         QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ServiceItem),
    tm(ToolManager::ins()),
    mDescription(description)
{
    ui->setupUi(this);

    ui->lblServiceName->setText(name);
    ui->lblServiceDescription->setMinimumWidth(0);
    updateDescriptionElision();
    ui->checkServiceRunning->setChecked(active);
    ui->checkServiceRunning->setText(active ? tr("Running") : tr("Stopped"));
    ui->checkServiceStartup->setChecked(status);

    ui->lblServiceName->setToolTip(name);
    ui->lblServiceDescription->setToolTip(description);
}

void ServiceItem::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDescriptionElision();
}

void ServiceItem::updateDescriptionElision()
{
    const QFontMetrics fm(ui->lblServiceDescription->fontMetrics());
    const int available = ui->lblServiceDescription->width();

    if (available > 0) {
        ui->lblServiceDescription->setText(fm.elidedText("- " + mDescription, Qt::ElideRight, available));
    } else {
        ui->lblServiceDescription->setText("- " + mDescription);
    }
}

void ServiceItem::on_checkServiceStartup_clicked(bool status)
{
    QString name = ui->lblServiceName->text();

    tm->changeServiceStatus(name, status);

    ui->checkServiceStartup->setChecked(tm->serviceIsEnabled(name));
}

void ServiceItem::on_checkServiceRunning_clicked(bool status)
{
    QString name = ui->lblServiceName->text();

    tm->changeServiceActive(name, status);

    bool nowActive = tm->serviceIsActive(name);
    ui->checkServiceRunning->setChecked(nowActive);
    ui->checkServiceRunning->setText(nowActive ? tr("Running") : tr("Stopped"));
}
