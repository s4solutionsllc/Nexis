#include "service_item.h"
#include "ui_service_item.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPointer>
#include <QResizeEvent>
#include <QtConcurrent>

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
    // The description must own the spare width. With a Preferred policy its
    // size hint is the already-elided text, so it could never grow back and
    // stayed clipped at a dozen characters while the spacer took the room.
    ui->lblServiceDescription->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ui->serviceItemLayout->setColumnStretch(2, 1);
    ui->serviceItemLayout->setColumnStretch(3, 0);
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

// The change can wait on an admin prompt, so it runs off the UI thread; the
// switch is locked until the service's real state has been read back.
void ServiceItem::on_checkServiceStartup_clicked(bool status)
{
    const QString name = ui->lblServiceName->text();
    ui->checkServiceStartup->setEnabled(false);

    ToolManager *tools = tm;
    QPointer<ServiceItem> self(this);
    (void)QtConcurrent::run([tools, self, name, status]() {
        tools->changeServiceStatus(name, status);
        const bool enabled = tools->serviceIsEnabled(name);
        QMetaObject::invokeMethod(qApp, [self, enabled]() {
            if (!self)
                return;
            self->ui->checkServiceStartup->setChecked(enabled);
            self->ui->checkServiceStartup->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}

void ServiceItem::on_checkServiceRunning_clicked(bool status)
{
    const QString name = ui->lblServiceName->text();
    ui->checkServiceRunning->setEnabled(false);

    ToolManager *tools = tm;
    QPointer<ServiceItem> self(this);
    (void)QtConcurrent::run([tools, self, name, status]() {
        tools->changeServiceActive(name, status);
        const bool active = tools->serviceIsActive(name);
        QMetaObject::invokeMethod(qApp, [self, active]() {
            if (!self)
                return;
            self->ui->checkServiceRunning->setChecked(active);
            self->ui->checkServiceRunning->setText(active ? tr("Running") : tr("Stopped"));
            self->ui->checkServiceRunning->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}
