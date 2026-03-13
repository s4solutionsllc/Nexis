#include "maintenance_wizard_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QSettings>
#include <QtConcurrent>

#include "Managers/app_manager.h"
#include "Managers/info_manager.h"
#include "Managers/tool_manager.h"
#include "signal_mapper.h"
#include "health_score_calculator.h"
#include "Utils/format_util.h"

MaintenanceWizardDialog::MaintenanceWizardDialog(QWidget *parent,
                                                   AppManager *appManager,
                                                   InfoManager *infoManager,
                                                   ToolManager *toolManager,
                                                   SignalMapper *signalMapper)
    : QDialog(parent)
    , mAppManager(appManager ? appManager : AppManager::ins())
    , mInfoManager(infoManager ? infoManager : InfoManager::ins())
    , mToolManager(toolManager ? toolManager : ToolManager::ins())
    , mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
    , mChecksComplete(0)
    , mHealthScore(0)
    , mCleaningDone(false)
{
    qRegisterMetaType<CleanerService::ScanResult>("CleanerService::ScanResult");
    qRegisterMetaType<QList<OrphanPackage>>("QList<OrphanPackage>");
    qRegisterMetaType<UpdateCheckResult>("UpdateCheckResult");

    setObjectName("maintenanceWizardDialog");
    setWindowTitle(tr("System Checkup"));
    setMinimumSize(520, 420);

    buildUI();
    refreshThemeColors();

    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &MaintenanceWizardDialog::refreshThemeColors);
}

static QLabel *makeStepIcon()
{
    auto *lbl = new QLabel;
    lbl->setFixedSize(24, 24);
    lbl->setAlignment(Qt::AlignCenter);
    return lbl;
}

static QWidget *makeStepRow(QLabel *icon, const QString &title, QLabel *&detailOut)
{
    auto *row = new QWidget;
    auto *hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(0, 4, 0, 4);

    hbox->addWidget(icon);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(0);

    auto *lblTitle = new QLabel(title);
    auto f = lblTitle->font();
    f.setBold(true);
    lblTitle->setFont(f);
    textCol->addWidget(lblTitle);

    detailOut = new QLabel;
    detailOut->setWordWrap(true);
    textCol->addWidget(detailOut);

    hbox->addLayout(textCol, 1);
    return row;
}

void MaintenanceWizardDialog::buildUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 16, 20, 16);

    auto *lblTitle = new QLabel(tr("System Checkup"));
    lblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(lblTitle);

    auto *scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->viewport()->setAutoFillBackground(false);

    mResultsWidget = new QWidget;
    mResultsWidget->setAutoFillBackground(false);
    mResultsLayout = new QVBoxLayout(mResultsWidget);
    mResultsLayout->setContentsMargins(0, 0, 0, 0);
    mResultsLayout->setSpacing(8);

    mIconJunk = makeStepIcon();
    mIconOrphans = makeStepIcon();
    mIconUpdates = makeStepIcon();
    mIconHealth = makeStepIcon();

    mResultsLayout->addWidget(makeStepRow(mIconJunk, tr("Cleanable Junk"), mLblJunkDetail));
    mResultsLayout->addWidget(makeStepRow(mIconOrphans, tr("Orphan Packages"), mLblOrphansDetail));
    mResultsLayout->addWidget(makeStepRow(mIconUpdates, tr("Pending Updates"), mLblUpdatesDetail));
    mResultsLayout->addWidget(makeStepRow(mIconHealth, tr("Health Score"), mLblHealthDetail));

    mResultsLayout->addStretch();

    scrollArea->setWidget(mResultsWidget);
    mainLayout->addWidget(scrollArea, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    mBtnClean = new QPushButton(tr("Clean Safe Items"));
    mBtnClean->setProperty("accessibleName", "primary");
    mBtnClean->setEnabled(false);
    connect(mBtnClean, &QPushButton::clicked, this, &MaintenanceWizardDialog::onCleanSafeItems);
    btnRow->addWidget(mBtnClean);

    btnRow->addStretch();

    mBtnClose = new QPushButton(tr("Close"));
    connect(mBtnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(mBtnClose);

    mainLayout->addLayout(btnRow);
}

void MaintenanceWizardDialog::runChecks()
{
    mChecksComplete.storeRelaxed(0);
    mCleaningDone = false;
    mBtnClean->setEnabled(false);

    setStepStatus(mIconJunk, mLblJunkDetail, "running", tr("Scanning..."));
    setStepStatus(mIconOrphans, mLblOrphansDetail, "running", tr("Checking..."));
    setStepStatus(mIconUpdates, mLblUpdatesDetail, "running", tr("Checking..."));
    setStepStatus(mIconHealth, mLblHealthDetail, "running", tr("Calculating..."));

    // Check 1: Cleanable junk scan
    QtConcurrent::run([this]() {
        auto result = CleanerService::ins()->scan(CleanerService::allCategories());
        QMetaObject::invokeMethod(this, "onScanFinished", Qt::QueuedConnection,
                                  Q_ARG(CleanerService::ScanResult, result));
    });

    // Check 2: Orphan packages
    QtConcurrent::run([this]() {
        auto orphans = mToolManager->getOrphanPackages();
        QMetaObject::invokeMethod(this, "onOrphansFinished", Qt::QueuedConnection,
                                  Q_ARG(QList<OrphanPackage>, orphans));
    });

    // Check 3: Pending updates
    QtConcurrent::run([this]() {
        auto result = mInfoManager->checkForSystemUpdates();
        QMetaObject::invokeMethod(this, "onUpdatesFinished", Qt::QueuedConnection,
                                  Q_ARG(UpdateCheckResult, result));
    });

    // Check 4: Health score (computed from current data)
    QtConcurrent::run([this]() {
        HealthScoreCalculator calc;

        int coreCount = mInfoManager->getCpuCoreCount();
        QList<double> loadAvgs = mInfoManager->getCpuLoadAvgs();
        if (coreCount > 0 && !loadAvgs.isEmpty()) {
            double ratio = loadAvgs.first() / coreCount;
            calc.setCpuScore(qBound(0, qRound(100.0 * (1.0 - ratio)), 100));
        }

        auto disks = mInfoManager->getDisks();
        if (!disks.isEmpty()) {
            qint64 totalSize = 0;
            double weightedScore = 0;
            for (const Disk &d : disks) {
                if (d.size == 0) continue;
                int usedPct = (int)(100.0 * d.used / d.size);
                int dScore = qBound(0, 100 - usedPct, 100);
                weightedScore += (double)dScore * d.size;
                totalSize += d.size;
            }
            if (totalSize > 0)
                calc.setDiskScore(qBound(0, (int)qRound(weightedScore / totalSize), 100));
        }

        calc.setComponentAvailable("temp", mInfoManager->hasThermalSensors());
        calc.setComponentAvailable("battery", mInfoManager->hasBattery());
        calc.setComponentAvailable("smart", mInfoManager->hasDiskHealth());

        if (mInfoManager->hasThermalSensors()) {
            double tempC = mInfoManager->getThermalTemperature(0);
            int tScore = 100;
            if (tempC >= 100.0) tScore = 0;
            else if (tempC > 60.0) tScore = qRound(100.0 * (100.0 - tempC) / 40.0);
            calc.setTempScore(tScore);
        }

        int score = calc.compositeScore();
        QString label = calc.scoreLabel();
        QMetaObject::invokeMethod(this, "onHealthScoreFinished", Qt::QueuedConnection,
                                  Q_ARG(int, score), Q_ARG(QString, label));
    });
}

void MaintenanceWizardDialog::onScanFinished(CleanerService::ScanResult result)
{
    mScanResult = result;

    if (result.totalSize > 0) {
        QString detail = FormatUtil::formatBytes(result.totalSize);
        QStringList cats;
        for (auto it = result.categoryFiles.begin(); it != result.categoryFiles.end(); ++it) {
            if (!it.value().isEmpty()) {
                quint64 catSize = 0;
                for (const QFileInfo &fi : it.value())
                    catSize += fi.isDir() ? 0 : fi.size();
                if (catSize > 0)
                    cats << QString("%1: %2").arg(CleanerService::categoryName(it.key()),
                                                  FormatUtil::formatBytes(catSize));
            }
        }
        QString fullDetail = tr("%1 found").arg(detail);
        if (!cats.isEmpty())
            fullDetail += "\n" + cats.join(", ");
        setStepStatus(mIconJunk, mLblJunkDetail, "warning", fullDetail);
    } else {
        setStepStatus(mIconJunk, mLblJunkDetail, "ok", tr("System is clean"));
    }

    mChecksComplete.fetchAndAddRelaxed(1);
    if (mChecksComplete.loadRelaxed() >= 4)
        onAllChecksComplete();
}

void MaintenanceWizardDialog::onOrphansFinished(QList<OrphanPackage> orphans)
{
    mOrphanResult = orphans;

    if (!orphans.isEmpty()) {
        quint64 totalSize = 0;
        for (const OrphanPackage &p : orphans)
            totalSize += p.size;
        QString detail = tr("%n orphan package(s)", "", orphans.size());
        if (totalSize > 0)
            detail += QString(" (%1)").arg(FormatUtil::formatBytes(totalSize));
        setStepStatus(mIconOrphans, mLblOrphansDetail, "warning", detail);
    } else {
        setStepStatus(mIconOrphans, mLblOrphansDetail, "ok", tr("No orphan packages"));
    }

    mChecksComplete.fetchAndAddRelaxed(1);
    if (mChecksComplete.loadRelaxed() >= 4)
        onAllChecksComplete();
}

void MaintenanceWizardDialog::onUpdatesFinished(UpdateCheckResult result)
{
    mUpdateResult = result;

    if (result.totalCount > 0) {
        QMap<QString, int> bySrc;
        for (const UpdateEntry &e : result.entries)
            bySrc[e.source]++;
        QStringList parts;
        for (auto it = bySrc.begin(); it != bySrc.end(); ++it)
            parts << QString("%1: %2").arg(it.key()).arg(it.value());
        setStepStatus(mIconUpdates, mLblUpdatesDetail, "info",
                      tr("%n update(s) available", "", result.totalCount) +
                      (parts.isEmpty() ? "" : " (" + parts.join(", ") + ")"));
    } else if (result.success) {
        setStepStatus(mIconUpdates, mLblUpdatesDetail, "ok", tr("System is up to date"));
    } else {
        setStepStatus(mIconUpdates, mLblUpdatesDetail, "ok",
                      tr("Could not check: %1").arg(result.errorMessage));
    }

    mChecksComplete.fetchAndAddRelaxed(1);
    if (mChecksComplete.loadRelaxed() >= 4)
        onAllChecksComplete();
}

void MaintenanceWizardDialog::onHealthScoreFinished(int score, QString label)
{
    mHealthScore = score;
    mHealthLabel = label;

    QString status = (score >= 75) ? "ok" : (score >= 40 ? "warning" : "error");
    setStepStatus(mIconHealth, mLblHealthDetail, status,
                  QString("%1/100 — %2").arg(score).arg(label));

    mChecksComplete.fetchAndAddRelaxed(1);
    if (mChecksComplete.loadRelaxed() >= 4)
        onAllChecksComplete();
}

void MaintenanceWizardDialog::onAllChecksComplete()
{
    bool hasCleanable = mScanResult.totalSize > 0;
    mBtnClean->setEnabled(hasCleanable && !mCleaningDone);

    if (!hasCleanable)
        mBtnClean->setText(tr("Nothing to Clean"));
}

void MaintenanceWizardDialog::onCleanSafeItems()
{
    mBtnClean->setEnabled(false);
    mBtnClean->setText(tr("Cleaning..."));

    QList<CleanerService::CleanCategory> safeCategories = {
        CleanerService::PACKAGE_CACHE,
        CleanerService::CRASH_REPORTS,
        CleanerService::APPLICATION_LOGS,
        CleanerService::APPLICATION_CACHES,
        CleanerService::DEV_TOOL_CACHES,
        CleanerService::BROKEN_SYMLINKS,
#ifndef Q_OS_MACOS
        CleanerService::SNAP_FLATPAK_REVISIONS,
#endif
    };

    QtConcurrent::run([this, safeCategories]() {
        auto result = CleanerService::ins()->clean(safeCategories);
        QMetaObject::invokeMethod(this, "onCleanFinished", Qt::QueuedConnection,
                                  Q_ARG(quint64, result.totalBytesFreed));
    });
}

void MaintenanceWizardDialog::onCleanFinished(quint64 bytesFreed)
{
    mCleaningDone = true;
    mBtnClean->setText(tr("Cleaned %1").arg(FormatUtil::formatBytes(bytesFreed)));
    mBtnClean->setEnabled(false);

    // Re-scan to update the junk display
    setStepStatus(mIconJunk, mLblJunkDetail, "ok",
                  tr("Freed %1").arg(FormatUtil::formatBytes(bytesFreed)));
}

void MaintenanceWizardDialog::setStepStatus(QLabel *icon, QLabel *detail,
                                             const QString &status, const QString &detailText)
{
    detail->setText(detailText);

    QSettings *sv = mAppManager->getStyleValues();
    QString successColor = sv->value("@successColor", "#2ec27e").toString();
    QString warningColor = sv->value("@warningColor", "#e5a50a").toString();
    QString destructiveColor = sv->value("@destructiveColor", "#e01b24").toString();
    QString textColor = sv->value("@color05", "#ccc").toString();

    if (status == "running") {
        icon->setText("\u2022\u2022\u2022");
        icon->setStyleSheet(QString("color: %1; font-weight: bold;").arg(textColor));
    } else if (status == "ok") {
        icon->setText("\u2713");
        icon->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(successColor));
    } else if (status == "warning") {
        icon->setText("\u26A0");
        icon->setStyleSheet(QString("color: %1; font-size: 14px;").arg(warningColor));
    } else if (status == "info") {
        icon->setText("\u2139");
        icon->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(textColor));
    } else if (status == "error") {
        icon->setText("\u2717");
        icon->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(destructiveColor));
    }
}

void MaintenanceWizardDialog::refreshThemeColors()
{
    QSettings *sv = mAppManager->getStyleValues();
    Q_UNUSED(sv);

    // Re-apply status colors to existing step icons if checks are already complete
    if (mChecksComplete.loadRelaxed() >= 4) {
        // Trigger a re-render of step statuses would require storing status strings.
        // For simplicity, the dialog's global QSS handles background/text colors.
    }
}

void MaintenanceWizardDialog::navigateToPage(const QString &pageTitle)
{
    emit mSignalMapper->sigNavigateToPage(pageTitle);
    accept();
}
