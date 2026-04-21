#include "process_alert_dialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ProcessAlertDialog::ProcessAlertDialog(const QString &processName, QWidget *parent)
    : QDialog(parent),
      mProcessName(processName)
{
    setWindowTitle(tr("Alert Threshold"));
    setMinimumWidth(420);
    buildUI();
    populateFromPrefs();
}

void ProcessAlertDialog::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    mLblName = new QLabel(tr("Alerts for processes named \"%1\"").arg(mProcessName), this);
    mLblName->setWordWrap(true);
    layout->addWidget(mLblName);

    auto *intro = new QLabel(
        tr("Fires a tray notification when any of these thresholds is exceeded. "
           "Values across PIDs with the same name are summed. Use 0 to disable "
           "a threshold."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout();
    mSpnCpu = new QSpinBox(this);
    mSpnCpu->setRange(0, 1000);       // allow >100 for multi-core aggregation
    mSpnCpu->setSuffix("%");
    form->addRow(tr("CPU threshold:"), mSpnCpu);

    auto *memRow = new QHBoxLayout();
    mSpnMem = new QSpinBox(this);
    mSpnMem->setRange(0, 1024 * 1024);
    memRow->addWidget(mSpnMem);
    mCmbUnit = new QComboBox(this);
    mCmbUnit->addItem("MB", 1024LL * 1024);
    mCmbUnit->addItem("GB", 1024LL * 1024 * 1024);
    mCmbUnit->setCurrentIndex(1);     // GB default — more useful
    memRow->addWidget(mCmbUnit);
    form->addRow(tr("Memory threshold:"), memRow);
    layout->addLayout(form);

    auto *buttons = new QHBoxLayout();
    mBtnDelete = new QPushButton(tr("Delete"), this);
    mBtnDelete->setCursor(Qt::PointingHandCursor);
    connect(mBtnDelete, &QPushButton::clicked, this, &ProcessAlertDialog::onDelete);
    buttons->addWidget(mBtnDelete);
    buttons->addStretch();

    mBtnCancel = new QPushButton(tr("Cancel"), this);
    mBtnCancel->setCursor(Qt::PointingHandCursor);
    connect(mBtnCancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(mBtnCancel);

    mBtnSave = new QPushButton(tr("Save"), this);
    mBtnSave->setCursor(Qt::PointingHandCursor);
    mBtnSave->setAccessibleName("primary");
    mBtnSave->setDefault(true);
    connect(mBtnSave, &QPushButton::clicked, this, &ProcessAlertDialog::onSave);
    buttons->addWidget(mBtnSave);

    layout->addLayout(buttons);
}

void ProcessAlertDialog::populateFromPrefs()
{
    auto *prefs = ProcessPrefsManager::ins();
    if (prefs->hasThreshold(mProcessName)) {
        mExisting = prefs->threshold(mProcessName);
        mSpnCpu->setValue(mExisting.cpuPercent);

        // Choose the nicest unit for display. Prefer GB if bytes are a clean
        // multiple ≥ 1 GB, else MB.
        constexpr qint64 GB = 1024LL * 1024 * 1024;
        constexpr qint64 MB = 1024LL * 1024;
        if (mExisting.memoryBytes >= GB && mExisting.memoryBytes % GB == 0) {
            mCmbUnit->setCurrentIndex(1);  // GB
            mSpnMem->setValue(static_cast<int>(mExisting.memoryBytes / GB));
        } else if (mExisting.memoryBytes > 0) {
            mCmbUnit->setCurrentIndex(0);  // MB
            mSpnMem->setValue(static_cast<int>(mExisting.memoryBytes / MB));
        }
    } else {
        mBtnDelete->setEnabled(false);
    }
}

void ProcessAlertDialog::onSave()
{
    ProcessPrefsManager::Threshold t;
    t.name = mProcessName;
    t.cpuPercent = mSpnCpu->value();
    const qint64 unit = mCmbUnit->currentData().toLongLong();
    t.memoryBytes = static_cast<qint64>(mSpnMem->value()) * unit;
    ProcessPrefsManager::ins()->setThreshold(t);
    accept();
}

void ProcessAlertDialog::onDelete()
{
    ProcessPrefsManager::ins()->removeThreshold(mProcessName);
    accept();
}
