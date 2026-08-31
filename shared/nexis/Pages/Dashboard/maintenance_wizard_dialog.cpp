#include "maintenance_wizard_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QScrollArea>
#include <QGroupBox>
#include <QStyle>
#include <QObject>
#include <QtConcurrent>

#include "Managers/app_manager.h"
#include "Managers/info_manager.h"
#include "Managers/tool_manager.h"
#include "signal_mapper.h"
#include "health_score_calculator.h"
#include "Utils/format_util.h"
#include "Common/trust_safety_preview_dialog.h"
#include "Common/trust_safety_types.h"

// SSO-15481: bridges CleanerService::ScanResult into the Trust & Safety
// preview / confirm / cancel / dry-run framework.
//
// scan() replays the already-collected mScanResult as TrustSafetyActionItems
// rather than re-hitting the filesystem — the wizard already owns a fresh
// scan from runChecks(), so a second scan would be redundant and racy.
//
// performItem() delegates individual file removal to CleanerService::cleanFiles()
// so the file-level side-effects (exclusions, access-denied tracking, etc.)
// are handled by the shared service, not re-implemented here.
class MaintenanceWizardCleanProvider : public TrustSafetyActionProvider
{
public:
    explicit MaintenanceWizardCleanProvider(const CleanerService::ScanResult &scanResult,
                                             const QList<CleanerService::CleanCategory> &safeCategories)
        : mScanResult(scanResult), mSafeCategories(safeCategories)
    {}

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override
    {
        int idCounter = 0;
        for (CleanerService::CleanCategory cat : mSafeCategories) {
            const QFileInfoList &files = mScanResult.categoryFiles.value(cat);
            if (files.isEmpty())
                continue;

            const QString catId    = QString::number(static_cast<int>(cat));
            const QString catLabel = CleanerService::categoryName(cat);

            for (const QFileInfo &fi : files) {
                if (cancelled && cancelled->loadRelaxed())
                    return;

                TrustSafetyActionItem item;
                item.id              = QStringLiteral("%1-%2").arg(catId).arg(idCounter++);
                item.label           = fi.fileName();
                item.description     = QObject::tr("Safe to remove: %1 in %2").arg(fi.fileName(), catLabel);
                item.command         = fi.absoluteFilePath();
                item.categoryId      = catId;
                item.categoryLabel   = catLabel;
                item.riskTier        = TrustSafetyActionItem::RiskTier::Standard;
                item.estimatedSizeBytes = fi.isDir() ? 0 : fi.size();
                itemFound(item);
            }
        }
    }

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override
    {
        TrustSafetyActionResult result;
        result.itemId = item.id;

        if (dryRun) {
            QFileInfo fi(item.command);
            result.succeeded    = fi.exists();
            result.bytesFreed   = fi.isDir() ? 0 : fi.size();
            return result;
        }

        quint64 freed = CleanerService::ins()->cleanFiles({item.command});
        result.succeeded  = true;
        result.bytesFreed = static_cast<qint64>(freed);
        return result;
    }

private:
    CleanerService::ScanResult mScanResult;
    QList<CleanerService::CleanCategory> mSafeCategories;
};

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
}

MaintenanceWizardDialog::~MaintenanceWizardDialog()
{
    // Backstop: block destruction until every detached worker that may
    // still hold a pointer to *this* (via the QPointer guard) has
    // finished. The QPointer guard in each lambda also breaks the chain
    // earlier, but this guarantees the strong invariant even if a
    // worker was about to re-enter when the dialog was destroyed.
    mScanFuture.waitForFinished();
    mOrphansFuture.waitForFinished();
    mUpdatesFuture.waitForFinished();
    mHealthFuture.waitForFinished();
    mCleanFuture.waitForFinished();
}

static QLabel *makeStepIcon()
{
    auto *lbl = new QLabel;
    lbl->setObjectName("wizardStepIcon");
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

    // Worker-thread safety contract:
    //   * Captures: QPointer<MaintenanceWizardDialog> for cross-thread
    //     lifetime checks, plus the singleton manager pointers by value
    //     so workers never dereference `this` for member reads.
    //   * Result delivery: QMetaObject::invokeMethod with a context
    //     object + functor overload (Qt::QueuedConnection). If the
    //     dialog is destroyed before the slot runs on the GUI thread,
    //     Qt drops the queued invocation.
    //   * Backstop: the destructor waitForFinished()s each future, so
    //     the dialog cannot be deleted while a worker is mid-execution.
    QPointer<MaintenanceWizardDialog> self(this);
    InfoManager *infoMgr = mInfoManager;
    ToolManager *toolMgr = mToolManager;

    // Check 1: Cleanable junk scan
    mScanFuture = QtConcurrent::run([self]() {
        auto result = CleanerService::ins()->scan(CleanerService::allCategories());
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, result]() {
            if (!self) return;
            self->onScanFinished(result);
        }, Qt::QueuedConnection);
    });

    // Check 2: Orphan packages
    mOrphansFuture = QtConcurrent::run([self, toolMgr]() {
        auto orphans = toolMgr->getOrphanPackages();
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, orphans]() {
            if (!self) return;
            self->onOrphansFinished(orphans);
        }, Qt::QueuedConnection);
    });

    // Check 3: Pending updates
    mUpdatesFuture = QtConcurrent::run([self, infoMgr]() {
        auto result = infoMgr->checkForSystemUpdates();
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, result]() {
            if (!self) return;
            self->onUpdatesFinished(result);
        }, Qt::QueuedConnection);
    });

    // Check 4: Health score (computed from current data)
    // WI-23: snapshot provider state on the UI thread before launching the
    // worker. DiskInfo::disks is republished on the UI thread by the medium
    // tick (FR-101) and macOS SMC reads serialize on a single connection;
    // calling those getters from a worker raced with the UI-thread writes.
    // Capturing by value keeps the worker's calc pure and lock-free.
    const int coreCount        = infoMgr->getCpuCoreCount();
    const QList<double> loadAvgs = infoMgr->getCpuLoadAvgs();
    const QList<Disk> disksSnap  = infoMgr->getDisks();
    const bool hasTemp         = infoMgr->hasThermalSensors();
    const bool hasBattery      = infoMgr->hasBattery();
    const bool hasSmart        = infoMgr->hasDiskHealth();
    const double tempCSnap     = hasTemp ? infoMgr->getThermalTemperature(0) : 0.0;

    mHealthFuture = QtConcurrent::run([self, coreCount, loadAvgs, disksSnap,
                                       hasTemp, hasBattery, hasSmart, tempCSnap]() {
        HealthScoreCalculator calc;

        if (coreCount > 0 && !loadAvgs.isEmpty()) {
            double ratio = loadAvgs.first() / coreCount;
            calc.setCpuScore(qBound(0, qRound(100.0 * (1.0 - ratio)), 100));
        }

        if (!disksSnap.isEmpty()) {
            qint64 totalSize = 0;
            double weightedScore = 0;
            for (const Disk &d : disksSnap) {
                if (d.size == 0) continue;
                int usedPct = (int)(100.0 * d.used / d.size);
                int dScore = qBound(0, 100 - usedPct, 100);
                weightedScore += (double)dScore * d.size;
                totalSize += d.size;
            }
            if (totalSize > 0)
                calc.setDiskScore(qBound(0, (int)qRound(weightedScore / totalSize), 100));
        }

        calc.setComponentAvailable("temp", hasTemp);
        calc.setComponentAvailable("battery", hasBattery);
        calc.setComponentAvailable("smart", hasSmart);

        if (hasTemp) {
            int tScore = 100;
            if (tempCSnap >= 100.0) tScore = 0;
            else if (tempCSnap > 60.0) tScore = qRound(100.0 * (100.0 - tempCSnap) / 40.0);
            calc.setTempScore(tScore);
        }

        int score = calc.compositeScore();
        QString label = calc.scoreLabel();
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, score, label]() {
            if (!self) return;
            self->onHealthScoreFinished(score, label);
        }, Qt::QueuedConnection);
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
    // SSO-15481: route through TrustSafetyPreviewDialog for itemized preview,
    // per-item deselection, cancel, and dry-run before any deletion occurs.
    // The wizard's "safe" category set is narrower than the System Cleaner's
    // risky set (BROWSER_PRIVACY, TRASH, DOWNLOADS_AGED are intentionally
    // excluded), so all items are Standard risk — the risky-category gate
    // is wired in the shared dialog but unused here by design.
    // SSO-23853: category set now lives in CleanerService::safeCategories()
    // so the menu-bar "Clean Now" one-click action shares it verbatim.
    MaintenanceWizardCleanProvider provider(mScanResult, CleanerService::safeCategories());

    TrustSafetyPreviewDialog::Config cfg;
    cfg.windowTitle          = tr("Review Items to Clean");
    cfg.primaryActionLabel   = tr("Clean Selected");
    cfg.confirmationSentence = tr("This will permanently delete the selected items.");

    TrustSafetyPreviewDialog dlg(&provider, cfg, this, mAppManager);
    dlg.exec();

    const TrustSafetyRunSummary summary = dlg.lastRunSummary();
    if (summary.totalItemsRequested > 0 && !summary.dryRun)
        onCleanFinished(static_cast<quint64>(summary.totalBytesFreed));
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

    if (status == "running") {
        icon->setText("\u2022\u2022\u2022");
        icon->setProperty("status", "info");
    } else if (status == "ok") {
        icon->setText("\u2713");
        icon->setProperty("status", "success");
    } else if (status == "warning") {
        icon->setText("\u26A0");
        icon->setProperty("status", "warning");
    } else if (status == "info") {
        icon->setText("\u2139");
        icon->setProperty("status", "info");
    } else if (status == "error") {
        icon->setText("\u2717");
        icon->setProperty("status", "error");
    }
    icon->style()->unpolish(icon);
    icon->style()->polish(icon);
}

void MaintenanceWizardDialog::navigateToPage(const QString &pageTitle)
{
    emit mSignalMapper->sigNavigateToPage(pageTitle);
    accept();
}
