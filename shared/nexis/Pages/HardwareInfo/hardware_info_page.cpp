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
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDateTime>
#include <QClipboard>
#include <QApplication>
#include <QToolTip>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "dpi.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"

#ifdef Q_OS_LINUX
class SmartPermissionDialog : public QDialog
{
public:
    SmartPermissionDialog(int driveCount, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Unlock SMART Data"));
        setMinimumWidth(420);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setSpacing(12);
        layout->setContentsMargins(20, 16, 20, 16);

        QLabel *desc = new QLabel(
            tr("Read full health data for %n drive(s) using elevated permissions.", "", driveCount));
        desc->setWordWrap(true);
        layout->addWidget(desc);

        mChkPermanent = new QCheckBox(
            tr("Also grant permanent access — no password needed on future launches\n"
               "(runs: setcap cap_sys_rawio,cap_dac_override+ep)"));
        layout->addWidget(mChkPermanent);

        layout->addSpacing(4);

        QDialogButtonBox *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        btns->button(QDialogButtonBox::Ok)->setText(tr("Unlock"));
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(btns);
    }

    bool makePermanent() const { return mChkPermanent->isChecked(); }

private:
    QCheckBox *mChkPermanent = nullptr;
};
#endif

HardwareInfoPage::~HardwareInfoPage()
{
    delete ui;
}

HardwareInfoPage::HardwareInfoPage(QWidget *parent, InfoManager *infoManager) :
    QWidget(parent),
    ui(new Ui::HardwareInfoPage),
    im(infoManager ? infoManager : InfoManager::ins())
{
    ui->setupUi(this);

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &HardwareInfoPage::refreshThemeColors);
    // FR-98: defer populate*() work to first showEvent so sysctl, SMART,
    // battery, and fan I/O only run when the user actually visits this page.
}

void HardwareInfoPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!mPopulated) {
        mPopulated = true;
        init();
    }
}

void HardwareInfoPage::init()
{
    populateSystem();
    populateProcessor();
    populateGraphics();
    populateMemory();
    populateBattery();
    populateFans();
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

    for (int i = 0; i < table->rowCount(); ++i)
        table->setRowHeight(i, rowHeight);

    int height = headerHeight + table->rowCount() * rowHeight;
    table->setFixedHeight(height);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // QSS ::item padding (@dp6 per side) is not included in
    // resizeColumnsToContents() size hints — add it to prevent clipping
    const int paddingCompensation = Dpi::scale(6) * 2;
    table->setColumnWidth(0, table->columnWidth(0) + paddingCompensation);
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
        if (!gpu.driverName.isEmpty())
            addRow(t, tr("Driver"), gpu.driverName);
    }

    t->resizeColumnsToContents();
    fitTableHeight(t);

    auto *btnCopyGpu = new QPushButton(this);
    btnCopyGpu->setText(tr("Copy GPU Diagnostics"));
    btnCopyGpu->setCursor(Qt::PointingHandCursor);
    btnCopyGpu->setToolTip(tr("Copy detailed GPU diagnostic info to clipboard"));
    ui->grpGraphicsLayout->addWidget(btnCopyGpu);
    connect(btnCopyGpu, &QPushButton::clicked, this, &HardwareInfoPage::onCopyGpuDiagnostics);
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

void HardwareInfoPage::populateFans()
{
    QTableWidget *t = ui->tblFans;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    if (!im->hasFanSensors()) {
        ui->grpFans->hide();
        return;
    }

    QList<FanSensor> fans = im->getFanSensors();
    for (int i = 0; i < fans.size(); ++i) {
        int rpm = im->getFanSpeed(i);
        addRow(t, fans.at(i).label, QString("%1 RPM").arg(rpm));
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

    mHealthItems.clear();

    if (!im->hasDiskHealth()) {
        ui->grpStorage->hide();
        return;
    }

    QList<DriveHealth> drives = im->getDriveHealth();
    if (drives.isEmpty()) {
        ui->grpStorage->hide();
        return;
    }

    mStorageDrives = drives;

    // Remove any previous unlock bar (handles repopulate calls)
    if (QWidget *old = ui->grpStorage->findChild<QWidget*>("storageUnlockBar"))
        old->deleteLater();

#ifdef Q_OS_LINUX
    {
        bool anyNeedsElevation = false;
        for (const DriveHealth &d : drives) {
            if (d.needsElevation) { anyNeedsElevation = true; break; }
        }
        if (anyNeedsElevation) {
            QWidget *bar = new QWidget(ui->grpStorage);
            bar->setObjectName("storageUnlockBar");
            QHBoxLayout *barLayout = new QHBoxLayout(bar);
            barLayout->setContentsMargins(0, 0, 4, 4);
            barLayout->setSpacing(8);

            QPushButton *btnUnlockAll = new QPushButton;
            btnUnlockAll->setText(tr("Unlock All Drives"));
            btnUnlockAll->setCursor(Qt::PointingHandCursor);
            btnUnlockAll->setToolTip(tr("Re-read SMART data for all drives with elevated permissions"));
            connect(btnUnlockAll, &QPushButton::clicked, this, &HardwareInfoPage::onUnlockAllDrives);
            barLayout->addWidget(btnUnlockAll);

            barLayout->addStretch();
            ui->grpStorageLayout->insertWidget(0, bar);
        }
    }
#endif

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
            QSettings *sv = AppManager::ins()->getStyleValues();
            if (sv) {
                if (d.healthVerdict == "Good")
                    valueItem->setForeground(QColor(sv->value("@successColor").toString()));
                else if (d.healthVerdict == "Caution")
                    valueItem->setForeground(QColor(sv->value("@warningColor").toString()));
                else if (d.healthVerdict == "Critical")
                    valueItem->setForeground(QColor(sv->value("@destructiveColor").toString()));
                // "Unknown" intentionally uses default text color
            }
            mHealthItems.append({valueItem, d.healthVerdict});
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

        if (d.needsElevation) {
            int row = t->rowCount();
            t->insertRow(row);

            QTableWidgetItem *noteLabel = new QTableWidgetItem(tr("  Note"));
            QFont bold = noteLabel->font();
            bold.setBold(true);
            noteLabel->setFont(bold);
            t->setItem(row, 0, noteLabel);

#ifdef Q_OS_LINUX
            QWidget *noteWidget = new QWidget;
            QHBoxLayout *noteLayout = new QHBoxLayout(noteWidget);
            noteLayout->setContentsMargins(0, 0, 0, 0);
            noteLayout->setSpacing(6);

            QLabel *noteText = new QLabel(tr("Limited data \u2014 smartctl requires elevated permissions"));
            noteText->setWordWrap(false);
            noteLayout->addWidget(noteText);

            QPushButton *btnUnlock = new QPushButton;
            btnUnlock->setText(tr("Unlock"));
            btnUnlock->setCursor(Qt::PointingHandCursor);
            btnUnlock->setToolTip(tr("Re-read SMART data with elevated permissions (pkexec)"));
            noteLayout->addWidget(btnUnlock);
            noteLayout->addStretch();

            const QString devicePath = d.devicePath;
            connect(btnUnlock, &QPushButton::clicked, this, [this, devicePath]() {
                onUnlockSmartDrive(devicePath);
            });

            t->setCellWidget(row, 1, noteWidget);
#else
            t->setItem(row, 1, new QTableWidgetItem(tr("Limited data \u2014 smartctl requires elevated permissions")));
#endif
        }

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

void HardwareInfoPage::onCopyGpuDiagnostics()
{
    QString report = im->getGpuDiagnosticReport();
    QApplication::clipboard()->setText(report);
    QToolTip::showText(QCursor::pos(), tr("Copied to clipboard"), this);
}

void HardwareInfoPage::onUnlockSmartDrive(const QString &devicePath)
{
    im->refreshDiskHealthElevated(devicePath);

    QList<DriveHealth> updated = im->getDriveHealth();
    bool stillNeedsElevation = true;
    for (const DriveHealth &d : updated) {
        if (d.devicePath == devicePath) {
            stillNeedsElevation = d.needsElevation;
            break;
        }
    }

    if (!stillNeedsElevation) {
        repopulateStorage();
    }
    // If still needs elevation (pkexec cancelled or failed), the existing
    // table state remains — button stays visible, user can try again.
}

void HardwareInfoPage::onUnlockAllDrives()
{
    QStringList devices;
    for (const DriveHealth &d : im->getDriveHealth()) {
        if (d.needsElevation)
            devices.append(d.devicePath);
    }
    if (devices.isEmpty())
        return;

#ifdef Q_OS_LINUX
    SmartPermissionDialog dlg(devices.size(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    im->refreshDiskHealthElevatedBatch(devices, dlg.makePermanent());
#endif

    repopulateStorage();
}

void HardwareInfoPage::repopulateStorage()
{
    ui->tblStorage->setRowCount(0);
    populateStorage();
    fitTableHeight(ui->tblStorage);
}

void HardwareInfoPage::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    for (const HealthItem &entry : mHealthItems) {
        if (entry.verdict == "Good")
            entry.item->setForeground(QColor(sv->value("@successColor").toString()));
        else if (entry.verdict == "Caution")
            entry.item->setForeground(QColor(sv->value("@warningColor").toString()));
        else if (entry.verdict == "Critical")
            entry.item->setForeground(QColor(sv->value("@destructiveColor").toString()));
    }
}

static QString tableToText(QTableWidget *table, const QString &sectionTitle)
{
    if (!table->isVisible() || table->rowCount() == 0)
        return QString();

    QString section;
    section += sectionTitle.toUpper() + "\n";
    section += QString("-").repeated(sectionTitle.length()) + "\n";

    int maxLabelWidth = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *label = table->item(row, 0);
        if (label && !label->text().isEmpty())
            maxLabelWidth = qMax(maxLabelWidth, label->text().length());
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *label = table->item(row, 0);
        QTableWidgetItem *value = table->item(row, 1);
        QString l = label ? label->text() : QString();
        QString v = value ? value->text() : QString();

        if (l.isEmpty() && v.isEmpty())
            continue;

        section += QString("%1  %2\n").arg(l, -maxLabelWidth).arg(v);
    }
    section += "\n";
    return section;
}

void HardwareInfoPage::on_btnExportReport_clicked()
{
    QString defaultName = QString("nexis-report-%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"));

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export System Report"), defaultName,
        tr("Text Files (*.txt);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    QString report;
    report += "Nexis System Report\n";
    report += QString("Generated: %1\n").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    report += QString("Version:   %1\n").arg(qApp->applicationVersion());
    report += "\n";

    report += tableToText(ui->tblSystem, tr("System"));
    report += tableToText(ui->tblProcessor, tr("Processor"));
    report += tableToText(ui->tblGraphics, tr("Graphics"));
    report += tableToText(ui->tblMemory, tr("Memory"));
    report += tableToText(ui->tblBattery, tr("Battery"));
    report += tableToText(ui->tblFans, tr("Fans"));
    report += tableToText(ui->tblStorage, tr("Storage"));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write to %1: %2").arg(filePath, file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream << report;
    file.close();

    QMessageBox::information(this, tr("Report Exported"),
        tr("System report saved to %1").arg(filePath));
}

QString HardwareInfoPage::tableToHtml(QTableWidget *table, const QString &title)
{
    if (!table->isVisible() || table->rowCount() == 0)
        return QString();

    QString html;
    html += QStringLiteral("<h2>%1</h2>\n<table>\n<tbody>\n").arg(title.toHtmlEscaped());

    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *label = table->item(row, 0);
        QTableWidgetItem *value = table->item(row, 1);
        const QString l = label ? label->text().toHtmlEscaped() : QString();
        const QString v = value ? value->text().toHtmlEscaped() : QString();
        if (l.isEmpty() && v.isEmpty())
            continue;
        html += QStringLiteral("<tr><td class=\"label\">%1</td><td>%2</td></tr>\n").arg(l, v);
    }

    html += QStringLiteral("</tbody>\n</table>\n");
    return html;
}

QString HardwareInfoPage::buildHtmlReport() const
{
    // Collect live data
    im->updateMemoryInfo();
    im->updateProcesses();

    // Top-10 processes by CPU
    QList<Process> procs = im->getProcesses();
    std::sort(procs.begin(), procs.end(), [](const Process &a, const Process &b) {
        return a.getPcpu() > b.getPcpu();
    });
    if (procs.size() > 10)
        procs = procs.mid(0, 10);

    // Pending updates
    QString updatesHtml;
    UpdateCheckResult upd = im->checkForSystemUpdates();
    if (!upd.success) {
        updatesHtml = QStringLiteral("<p>%1</p>").arg(tr("Update check unavailable.").toHtmlEscaped());
    } else if (upd.totalCount == 0) {
        updatesHtml = QStringLiteral("<p>%1</p>").arg(tr("No pending updates.").toHtmlEscaped());
    } else {
        updatesHtml += QStringLiteral("<p>%1</p>\n<table>\n<tbody>\n")
            .arg(tr("%n update(s) available", "", upd.totalCount).toHtmlEscaped());
        for (const UpdateEntry &e : upd.entries)
            updatesHtml += QStringLiteral("<tr><td class=\"label\">%1</td><td>%2</td></tr>\n")
                .arg(e.name.toHtmlEscaped(), e.version.toHtmlEscaped());
        updatesHtml += QStringLiteral("</tbody>\n</table>\n");
    }

    // System snapshot
    const QList<int> cpuPcts = im->getCpuPercents();
    int cpuAvg = 0;
    if (!cpuPcts.isEmpty()) {
        for (int p : cpuPcts) cpuAvg += p;
        cpuAvg /= cpuPcts.size();
    }
    const quint64 memUsed  = im->getMemUsed();
    const quint64 memTotal = im->getMemTotal();

    QString snapshotHtml;
    snapshotHtml += QStringLiteral("<div class=\"metrics\">\n");
    snapshotHtml += QStringLiteral("<div class=\"metric\"><div class=\"val\">%1%</div><div class=\"key\">CPU</div></div>\n")
        .arg(cpuAvg);
    snapshotHtml += QStringLiteral("<div class=\"metric\"><div class=\"val\">%1</div><div class=\"key\">Memory Used</div></div>\n")
        .arg(FormatUtil::formatBytes(memUsed).toHtmlEscaped());
    snapshotHtml += QStringLiteral("<div class=\"metric\"><div class=\"val\">%1</div><div class=\"key\">Memory Total</div></div>\n")
        .arg(FormatUtil::formatBytes(memTotal).toHtmlEscaped());
    if (im->hasGpu()) {
        for (const GpuDevice &g : im->getGpuDevices()) {
            snapshotHtml += QStringLiteral("<div class=\"metric\"><div class=\"val\">%1%</div><div class=\"key\">GPU (%2)</div></div>\n")
                .arg(g.utilization).arg(g.name.toHtmlEscaped());
        }
    }
    if (im->hasBattery()) {
        const BatteryData bat = im->getBatteryData();
        snapshotHtml += QStringLiteral("<div class=\"metric\"><div class=\"val\">%1%</div><div class=\"key\">Battery</div></div>\n")
            .arg(bat.chargePercent);
    }
    snapshotHtml += QStringLiteral("</div>\n");

    // Processes table
    QString procsHtml;
    procsHtml += QStringLiteral("<h2>%1</h2>\n<table>\n<thead><tr><th>%2</th><th>%3</th><th>%4</th><th>%5</th></tr></thead>\n<tbody>\n")
        .arg(tr("Top Processes (by CPU)").toHtmlEscaped(),
             tr("Name").toHtmlEscaped(),
             tr("User").toHtmlEscaped(),
             tr("CPU %").toHtmlEscaped(),
             tr("Memory").toHtmlEscaped());
    for (const Process &p : procs) {
        procsHtml += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3%</td><td>%4</td></tr>\n")
            .arg(p.getCmd().toHtmlEscaped(),
                 p.getUname().toHtmlEscaped(),
                 QString::number(p.getPcpu(), 'f', 1),
                 FormatUtil::formatBytes(p.getRss()).toHtmlEscaped());
    }
    procsHtml += QStringLiteral("</tbody>\n</table>\n");

    // Inline CSS
    static const QString css = QStringLiteral(
        "body{font-family:system-ui,sans-serif;max-width:960px;margin:40px auto;padding:0 20px;color:#222;background:#fff}"
        "h1{font-size:1.6em;border-bottom:2px solid #0066cc;padding-bottom:8px;color:#0066cc}"
        "h2{font-size:1.1em;margin-top:2em;color:#0066cc;border-bottom:1px solid #ddd;padding-bottom:4px}"
        "table{width:100%;border-collapse:collapse;margin-bottom:1em;font-size:.9em}"
        "th,td{text-align:left;padding:6px 10px;border-bottom:1px solid #eee}"
        "th{background:#f5f5f5;font-weight:600}"
        "tr:hover td{background:#fafafa}"
        "td.label{color:#555;width:40%;font-weight:500}"
        ".metrics{display:flex;flex-wrap:wrap;gap:12px;margin-bottom:1.5em}"
        ".metric{background:#f0f5ff;border:1px solid #ccd9f0;border-radius:6px;padding:12px 18px;min-width:130px}"
        ".metric .val{font-size:1.4em;font-weight:700;color:#0066cc}"
        ".metric .key{font-size:.8em;color:#666;margin-top:4px}"
        "p.meta{color:#666;font-size:.85em}"
    );

    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QString html;
    html += QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
                           "<meta charset=\"UTF-8\">\n"
                           "<title>Nexis System Report</title>\n"
                           "<style>%1</style>\n</head>\n<body>\n").arg(css);
    html += QStringLiteral("<h1>Nexis System Report</h1>\n");
    html += QStringLiteral("<p class=\"meta\">Generated: %1 &nbsp;|&nbsp; Version: %2</p>\n")
        .arg(ts.toHtmlEscaped(), qApp->applicationVersion().toHtmlEscaped());

    html += QStringLiteral("<h2>%1</h2>\n").arg(tr("System Overview").toHtmlEscaped());
    html += snapshotHtml;

    html += tableToHtml(ui->tblSystem,    tr("System"));
    html += tableToHtml(ui->tblProcessor, tr("Processor"));
    html += tableToHtml(ui->tblGraphics,  tr("Graphics"));
    html += tableToHtml(ui->tblMemory,    tr("Memory"));
    html += tableToHtml(ui->tblBattery,   tr("Battery"));
    html += tableToHtml(ui->tblFans,      tr("Fans"));
    html += tableToHtml(ui->tblStorage,   tr("Storage"));

    html += procsHtml;

    html += QStringLiteral("<h2>%1</h2>\n").arg(tr("Pending Updates").toHtmlEscaped());
    html += updatesHtml;

    html += QStringLiteral("</body>\n</html>\n");
    return html;
}

void HardwareInfoPage::on_btnExportHtmlReport_clicked()
{
    const QString defaultName = QString("nexis-report-%1.html")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"));

    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export as HTML"), defaultName,
        tr("HTML Files (*.html);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    ui->btnExportHtmlReport->setEnabled(false);
    ui->btnExportHtmlReport->setText(tr("Generating…"));
    QApplication::processEvents();

    const QString html = buildHtmlReport();

    ui->btnExportHtmlReport->setEnabled(true);
    ui->btnExportHtmlReport->setText(tr("Export as HTML..."));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write to %1: %2").arg(filePath, file.errorString()));
        return;
    }

    QTextStream(&file) << html;
    file.close();

    QMessageBox::information(this, tr("Report Exported"),
        tr("HTML report saved to %1").arg(filePath));
}

