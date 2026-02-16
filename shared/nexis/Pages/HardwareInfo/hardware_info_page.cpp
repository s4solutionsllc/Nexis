#include "hardware_info_page.h"
#include "ui_hardware_info_page.h"

#include <Info/system_info.h>
#include <Info/cpu_info.h>
#include <Utils/format_util.h>

#include <QHeaderView>
#include <QSysInfo>
#include <QProcess>
#include <QDir>

HardwareInfoPage::~HardwareInfoPage()
{
    delete ui;
}

HardwareInfoPage::HardwareInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HardwareInfoPage),
    im(InfoManager::ins())
{
    ui->setupUi(this);

    init();
}

void HardwareInfoPage::init()
{
    populateSystem();
    populateProcessor();
    populateGraphics();
    populateMemory();
    populateThermal();
}

void HardwareInfoPage::addRow(QTableWidget *table, const QString &label, const QString &value)
{
    int row = table->rowCount();
    table->insertRow(row);

    QTableWidgetItem *labelItem = new QTableWidgetItem(label);
    QFont bold = labelItem->font();
    bold.setBold(true);
    labelItem->setFont(bold);
    table->setItem(row, 0, labelItem);

    QTableWidgetItem *valueItem = new QTableWidgetItem(value);
    table->setItem(row, 1, valueItem);
}

void HardwareInfoPage::fitTableHeight(QTableWidget *table)
{
    const int rowHeight = 30;
    const int headerHeight = table->horizontalHeader()->isVisible() ? 36 : 0;
    int height = headerHeight + table->rowCount() * rowHeight;
    table->setFixedHeight(height);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HardwareInfoPage::populateSystem()
{
    QTableWidget *t = ui->tblSystem;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    SystemInfo sysInfo;
    addRow(t, tr("Hostname"), sysInfo.getHostname());
    addRow(t, tr("Platform"), sysInfo.getPlatform());
    addRow(t, tr("Distribution"), sysInfo.getDistribution());
    addRow(t, tr("Kernel"), sysInfo.getKernel());
    addRow(t, tr("Architecture"), QSysInfo::currentCpuArchitecture());

#ifdef Q_OS_LINUX
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (desktop.isEmpty())
        desktop = qEnvironmentVariable("DESKTOP_SESSION");
    if (!desktop.isEmpty())
        addRow(t, tr("Desktop Environment"), desktop);
#elif defined(Q_OS_MAC)
    addRow(t, tr("Desktop Environment"), QStringLiteral("Aqua"));
#endif

    t->resizeColumnsToContents();
    fitTableHeight(t);
}

void HardwareInfoPage::populateProcessor()
{
    QTableWidget *t = ui->tblProcessor;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    SystemInfo sysInfo;
    CpuInfo cpuInfo;

    addRow(t, tr("Model"), sysInfo.getCpuModel());
    addRow(t, tr("Physical Cores"), QString::number(cpuInfo.getCpuPhysicalCoreCount()));
    addRow(t, tr("Logical Cores"), sysInfo.getCpuCore());
    addRow(t, tr("Base Clock"), sysInfo.getCpuSpeed());

#ifdef Q_OS_MAC
    auto readSysctl = [](const char *key) -> QString {
        QProcess proc;
        proc.start("sysctl", {"-n", key});
        proc.waitForFinished(2000);
        QString val = proc.readAllStandardOutput().trimmed();
        if (val.isEmpty())
            return QString();
        bool ok;
        quint64 bytes = val.toULongLong(&ok);
        if (!ok || bytes == 0)
            return QString();
        if (bytes >= 1048576)
            return QString("%1 MB").arg(bytes / 1048576);
        else if (bytes >= 1024)
            return QString("%1 KB").arg(bytes / 1024);
        return QString("%1 B").arg(bytes);
    };

    QString l1d = readSysctl("hw.l1dcachesize");
    QString l1i = readSysctl("hw.l1icachesize");
    if (!l1d.isEmpty() && !l1i.isEmpty())
        addRow(t, tr("L1 Cache"), QString("%1 (D) / %2 (I)").arg(l1d, l1i));
    else if (!l1d.isEmpty())
        addRow(t, tr("L1 Cache"), l1d);

    QString l2 = readSysctl("hw.l2cachesize");
    if (!l2.isEmpty())
        addRow(t, tr("L2 Cache"), l2);

    QString l3 = readSysctl("hw.l3cachesize");
    if (!l3.isEmpty())
        addRow(t, tr("L3 Cache"), l3);
#elif defined(Q_OS_LINUX)
    QDir cacheDir("/sys/devices/system/cpu/cpu0/cache");
    if (cacheDir.exists()) {
        QStringList indices = cacheDir.entryList({"index*"}, QDir::Dirs, QDir::Name);
        for (const QString &idx : indices) {
            QString levelPath = cacheDir.filePath(idx + "/level");
            QString typePath = cacheDir.filePath(idx + "/type");
            QString sizePath = cacheDir.filePath(idx + "/size");

            QString level = FileUtil::readStringFromFile(levelPath).trimmed();
            QString type = FileUtil::readStringFromFile(typePath).trimmed();
            QString size = FileUtil::readStringFromFile(sizePath).trimmed();

            if (!level.isEmpty() && !size.isEmpty()) {
                QString label = QString("L%1 Cache").arg(level);
                if (type == "Data")
                    label += " (D)";
                else if (type == "Instruction")
                    label += " (I)";
                else if (type == "Unified")
                    label += " (Unified)";
                addRow(t, label, size);
            }
        }
    }
#endif

    t->resizeColumnsToContents();
    fitTableHeight(t);
}

void HardwareInfoPage::populateGraphics()
{
    QTableWidget *t = ui->tblGraphics;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    if (!im->hasGpu()) {
        ui->grpGraphics->hide();
        return;
    }

    QList<GpuDevice> gpus = im->getGpuDevices();
    for (int i = 0; i < gpus.size(); ++i) {
        const GpuDevice &gpu = gpus.at(i);
        if (gpus.size() > 1)
            addRow(t, tr("GPU %1").arg(i + 1), gpu.name);
        else
            addRow(t, tr("Name"), gpu.name);
        addRow(t, tr("Vendor"), gpu.vendor);
    }

    t->resizeColumnsToContents();
    fitTableHeight(t);
}

void HardwareInfoPage::populateMemory()
{
    QTableWidget *t = ui->tblMemory;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    im->updateMemoryInfo();

    addRow(t, tr("Total RAM"), FormatUtil::formatBytes(im->getMemTotal()));
    addRow(t, tr("Swap Total"), FormatUtil::formatBytes(im->getSwapTotal()));

    t->resizeColumnsToContents();
    fitTableHeight(t);
}

void HardwareInfoPage::populateThermal()
{
    QTableWidget *t = ui->tblThermal;
    t->verticalHeader()->setVisible(false);

    if (!im->hasThermalSensors()) {
        ui->grpThermal->hide();
        return;
    }

    t->setColumnCount(4);
    t->setHorizontalHeaderLabels({
        tr("Sensor"), tr("Temperature"), tr("Max"), tr("Critical")
    });

    QList<ThermalSensor> sensors = im->getThermalSensors();
    for (int i = 0; i < sensors.size(); ++i) {
        const ThermalSensor &s = sensors.at(i);
        double temp = im->getThermalTemperature(i);

        int row = t->rowCount();
        t->insertRow(row);
        t->setItem(row, 0, new QTableWidgetItem(s.label));
        t->setItem(row, 1, new QTableWidgetItem(
            temp >= 0 ? QString("%1 \u00B0C").arg(temp, 0, 'f', 1) : tr("N/A")));
        t->setItem(row, 2, new QTableWidgetItem(
            s.maxTemp >= 0 ? QString("%1 \u00B0C").arg(s.maxTemp, 0, 'f', 1) : tr("N/A")));
        t->setItem(row, 3, new QTableWidgetItem(
            s.critTemp >= 0 ? QString("%1 \u00B0C").arg(s.critTemp, 0, 'f', 1) : tr("N/A")));
    }

    t->horizontalHeader()->setStretchLastSection(true);
    t->resizeColumnsToContents();
    fitTableHeight(t);
}
