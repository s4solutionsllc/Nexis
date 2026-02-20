#include "hardware_info_page.h"
#include "ui_hardware_info_page.h"

#include <Info/system_info.h>
#include <Info/cpu_info.h>

#ifdef Q_OS_MACOS
#include <Info/system_info_macos.h>
#include <Info/cpu_info_macos.h>
#else
#include <Info/system_info_linux.h>
#include <Info/cpu_info_linux.h>
#endif
#include <Utils/format_util.h>

#include <QHeaderView>
#include <QSysInfo>
#include <QProcess>
#include <QDir>
#include "dpi.h"

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
    populateBattery();
    populateStorage();
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
    const int rowHeight = Dpi::scale(30);
    const int headerHeight = table->horizontalHeader()->isVisible() ? Dpi::scale(36) : 0;
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

#ifdef Q_OS_MACOS
    SystemInfoMacOS sysInfo;
#else
    SystemInfoLinux sysInfo;
#endif
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

#ifdef Q_OS_MACOS
    SystemInfoMacOS sysInfo;
    CpuInfoMacOS cpuInfo;
#else
    SystemInfoLinux sysInfo;
    CpuInfoLinux cpuInfo;
#endif

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

void HardwareInfoPage::populateBattery()
{
    QTableWidget *t = ui->tblBattery;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    if (!im->hasBattery()) {
        ui->grpBattery->hide();
        return;
    }

    im->updateBatteryInfo();
    BatteryData bat = im->getBatteryData();

    addRow(t, tr("Status"), bat.status);

    if (bat.healthPercent >= 0)
        addRow(t, tr("Health"), QString("%1% (%2)").arg(bat.healthPercent).arg(bat.condition));

    if (bat.chargePercent >= 0)
        addRow(t, tr("Charge"), QString("%1%").arg(bat.chargePercent));

    if (bat.cycleCount >= 0) {
        QString cycleStr;
        if (bat.designCycleCount > 0)
            cycleStr = QString("%1 / %2").arg(bat.cycleCount).arg(bat.designCycleCount);
        else
            cycleStr = QString::number(bat.cycleCount);
        addRow(t, tr("Cycle Count"), cycleStr);
    }

    if (bat.currentCapacityMah >= 0)
        addRow(t, tr("Current Capacity"), QString("%1 mAh").arg(bat.currentCapacityMah, 0, 'f', 0));

    if (bat.maxCapacityMah >= 0)
        addRow(t, tr("Maximum Capacity"), QString("%1 mAh").arg(bat.maxCapacityMah, 0, 'f', 0));

    if (bat.designCapacityMah >= 0)
        addRow(t, tr("Design Capacity"), QString("%1 mAh").arg(bat.designCapacityMah, 0, 'f', 0));

    if (bat.temperatureCelsius >= 0) {
        double tempF = bat.temperatureCelsius * 9.0 / 5.0 + 32.0;
        addRow(t, tr("Temperature"), QString("%1 \u00B0C / %2 \u00B0F")
               .arg(bat.temperatureCelsius, 0, 'f', 1)
               .arg(tempF, 0, 'f', 1));
    }

    if (bat.voltageMv > 0)
        addRow(t, tr("Voltage"), QString("%1 V").arg(bat.voltageMv / 1000.0, 0, 'f', 3));

    if (bat.powerWatts > 0)
        addRow(t, tr("Power"), QString("%1 W (%2)")
               .arg(bat.powerWatts, 0, 'f', 1)
               .arg(bat.isCharging ? tr("charging") : tr("discharging")));

    if (bat.timeRemainingMinutes >= 0) {
        int h = bat.timeRemainingMinutes / 60;
        int m = bat.timeRemainingMinutes % 60;
        QString timeStr;
        if (bat.isCharging)
            timeStr = QString("%1h %2m %3").arg(h).arg(m).arg(tr("to full"));
        else
            timeStr = QString("%1h %2m %3").arg(h).arg(m).arg(tr("remaining"));
        addRow(t, tr("Time Remaining"), timeStr);
    }

    if (!bat.manufacturer.isEmpty())
        addRow(t, tr("Manufacturer"), bat.manufacturer);

    if (!bat.model.isEmpty())
        addRow(t, tr("Model"), bat.model);

    if (!bat.technology.isEmpty())
        addRow(t, tr("Technology"), bat.technology);

    if (bat.manufactureDate.isValid())
        addRow(t, tr("Manufacture Date"), bat.manufactureDate.toString("yyyy-MM-dd"));

    if (bat.chargeStartThreshold >= 0 && bat.chargeStopThreshold >= 0) {
        addRow(t, tr("Charge Limit"),
               QString("%1% \u2013 %2% (%3)")
               .arg(bat.chargeStartThreshold)
               .arg(bat.chargeStopThreshold)
               .arg(tr("managed by TLP")));
    }

    t->resizeColumnsToContents();
    fitTableHeight(t);
}

void HardwareInfoPage::populateStorage()
{
    QTableWidget *t = ui->tblStorage;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    if (!im->hasDiskHealth()) {
        ui->grpStorage->hide();
        return;
    }

    QList<DriveHealth> drives = im->getDriveHealth();
    if (drives.isEmpty()) {
        ui->grpStorage->hide();
        return;
    }

    for (int i = 0; i < drives.size(); ++i) {
        const DriveHealth &d = drives.at(i);

        QString driveName = d.model.isEmpty() ? d.deviceName : d.model;
        if (drives.size() > 1)
            addRow(t, tr("Drive %1").arg(i + 1), driveName);
        else
            addRow(t, tr("Drive"), driveName);

        if (d.sizeBytes > 0)
            addRow(t, tr("  Size"), FormatUtil::formatBytes(d.sizeBytes));

        QString typeStr;
        switch (d.driveType) {
        case DriveHealth::NVMe:     typeStr = "NVMe SSD"; break;
        case DriveHealth::SATA_SSD: typeStr = "SATA SSD"; break;
        case DriveHealth::SATA_HDD: typeStr = "HDD"; break;
        default:                    typeStr = d.protocol; break;
        }
        if (!typeStr.isEmpty())
            addRow(t, tr("  Type"), typeStr);

        if (!d.healthVerdict.isEmpty()) {
            QString healthStr;
            if (d.healthPercent >= 0)
                healthStr = QString("%1% (%2)").arg(d.healthPercent).arg(d.healthVerdict);
            else
                healthStr = d.healthVerdict;

            int row = t->rowCount();
            t->insertRow(row);

            QTableWidgetItem *labelItem = new QTableWidgetItem(tr("  Health"));
            QFont bold = labelItem->font();
            bold.setBold(true);
            labelItem->setFont(bold);
            t->setItem(row, 0, labelItem);

            QTableWidgetItem *valueItem = new QTableWidgetItem(healthStr);
            if (d.healthVerdict == "Good")
                valueItem->setForeground(QColor("#2ec27e"));
            else if (d.healthVerdict == "Caution")
                valueItem->setForeground(QColor("#e5a50a"));
            else if (d.healthVerdict == "Critical")
                valueItem->setForeground(QColor("#e01b24"));
            t->setItem(row, 1, valueItem);
        }

        if (d.temperatureCelsius >= 0) {
            double tempF = d.temperatureCelsius * 9.0 / 5.0 + 32.0;
            addRow(t, tr("  Temperature"), QString("%1 \u00B0C / %2 \u00B0F")
                   .arg(d.temperatureCelsius, 0, 'f', 0)
                   .arg(tempF, 0, 'f', 0));
        }

        if (d.driveType == DriveHealth::NVMe) {
            if (d.percentageUsed >= 0)
                addRow(t, tr("  Endurance Used"), QString("%1%").arg(d.percentageUsed));
            if (d.availableSpare >= 0)
                addRow(t, tr("  Available Spare"), QString("%1%").arg(d.availableSpare));
            if (d.dataUnitsWritten >= 0) {
                double tb = d.dataUnitsWritten * 512000.0 / (1024.0 * 1024.0 * 1024.0 * 1024.0);
                addRow(t, tr("  Data Written"), QString("%1 TB").arg(tb, 0, 'f', 2));
            }
            if (d.dataUnitsRead >= 0) {
                double tb = d.dataUnitsRead * 512000.0 / (1024.0 * 1024.0 * 1024.0 * 1024.0);
                addRow(t, tr("  Data Read"), QString("%1 TB").arg(tb, 0, 'f', 2));
            }
            if (d.unsafeShutdowns >= 0)
                addRow(t, tr("  Unsafe Shutdowns"), QString::number(d.unsafeShutdowns));
            if (d.mediaErrors >= 0)
                addRow(t, tr("  Media Errors"), QString::number(d.mediaErrors));
        }

        if (d.driveType == DriveHealth::SATA_SSD) {
            if (d.wearLevelingCount >= 0)
                addRow(t, tr("  Wear Leveling"), QString("%1%").arg(d.wearLevelingCount));
            if (d.reallocatedSectors >= 0)
                addRow(t, tr("  Reallocated Sectors"), QString::number(d.reallocatedSectors));
        }

        if (d.driveType == DriveHealth::SATA_HDD) {
            if (d.reallocatedSectors >= 0)
                addRow(t, tr("  Reallocated Sectors"), QString::number(d.reallocatedSectors));
            if (d.pendingSectors >= 0)
                addRow(t, tr("  Pending Sectors"), QString::number(d.pendingSectors));
            if (d.uncorrectableSectors >= 0)
                addRow(t, tr("  Uncorrectable Sectors"), QString::number(d.uncorrectableSectors));
        }

        if (d.powerOnHours >= 0) {
            int days = d.powerOnHours / 24;
            addRow(t, tr("  Power-On Time"), QString("%1 hours (%2 days)").arg(d.powerOnHours).arg(days));
        }
        if (d.powerCycles >= 0)
            addRow(t, tr("  Power Cycles"), QString::number(d.powerCycles));

        if (!d.smartPassed)
            addRow(t, tr("  SMART Status"), tr("FAILING"));

        if (d.needsElevation)
            addRow(t, tr("  Note"), tr("Limited data \u2014 smartctl requires elevated permissions"));

        if (i < drives.size() - 1) {
            int row = t->rowCount();
            t->insertRow(row);
            t->setItem(row, 0, new QTableWidgetItem(""));
            t->setItem(row, 1, new QTableWidgetItem(""));
        }
    }

    t->resizeColumnsToContents();
    fitTableHeight(t);
}

