#include "mini_monitor_window.h"

#include <Managers/data_refresh_service.h>
#include <Managers/info_manager.h>
#include <Managers/app_manager.h>
#include <Managers/setting_manager.h>
#include <Info/memory_info.h>
#include <Info/disk_info.h>
#include <Utils/health_score_inputs.h>
#include <Utils/mini_monitor_format_util.h>
#include "signal_mapper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QCloseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QStyle>
#include <QSettings>

namespace {
// 8px base spacing unit (docs/design/DESIGN_SYSTEM.md).
constexpr int kSpacingUnit = 8;

// @monoFontFamily is not a values.ini key — AppManager substitutes it
// directly into the global QSS text (app_manager.cpp), so it can't be
// looked up via QSettings here. Mirror the same literal for this
// per-widget inline stylesheet.
const QString kMonoFontFamily = QStringLiteral("\"JetBrains Mono\", \"SF Mono\", Menlo, Consolas, monospace");
}

MiniMonitorWindow::MiniMonitorWindow(QWidget *parent)
    // Qt::Window (not Qt::Tool): an independent top-level window that stays
    // visible/on-top even if `parent` (the main window) is minimized or
    // hidden to tray — the whole point of a glanceable surface. `parent` is
    // still passed for ownership/cleanup, not for window-manager behavior.
    : QWidget(parent, Qt::Window | Qt::WindowStaysOnTopHint)
{
    setObjectName("miniMonitorWindow");
    setWindowTitle(tr("Nexis Mini Monitor"));
    setMinimumSize(200, 160);
    setMaximumSize(360, 320);

    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &MiniMonitorWindow::refreshThemeColors);

    const QByteArray savedGeometry = SettingManager::ins()->getMiniMonitorGeometry();
    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    } else {
        resize(220, 200);
        QScreen *screen = qApp->primaryScreen();
        if (screen) {
            setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignTop | Qt::AlignRight,
                size(), screen->availableGeometry()));
        }
    }
}

MiniMonitorWindow::~MiniMonitorWindow()
{
    setSubscribed(false);
}

void MiniMonitorWindow::buildLayout()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(2 * kSpacingUnit, 2 * kSpacingUnit, 2 * kSpacingUnit, 2 * kSpacingUnit);
    root->setSpacing(kSpacingUnit);

    auto *header = new QHBoxLayout();
    header->setSpacing(kSpacingUnit);

    auto *accentBar = new QFrame(this);
    accentBar->setObjectName("miniMonitorAccentBar");
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(20);
    header->addWidget(accentBar);

    auto *title = new QLabel(tr("Nexis"), this);
    title->setObjectName("miniMonitorTitle");
    header->addWidget(title);
    header->addStretch();
    root->addLayout(header);

    mLblScore = new QLabel("--", this);
    mLblScore->setObjectName("miniMonitorScoreValue");
    mLblScore->setAlignment(Qt::AlignHCenter);
    root->addWidget(mLblScore);

    mLblScoreLabel = new QLabel("", this);
    mLblScoreLabel->setObjectName("miniMonitorScoreLabel");
    mLblScoreLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(mLblScoreLabel);

    root->addSpacing(kSpacingUnit);

    auto addMetricRow = [&](QLabel *&dot, QLabel *&value) {
        auto *row = new QHBoxLayout();
        row->setSpacing(kSpacingUnit);
        dot = new QLabel(this);
        dot->setFixedSize(kSpacingUnit, kSpacingUnit);
        row->addWidget(dot);
        value = new QLabel(this);
        value->setObjectName("miniMonitorMetricValue");
        row->addWidget(value);
        row->addStretch();
        root->addLayout(row);
    };

    addMetricRow(mDotCpu, mLblCpu);
    addMetricRow(mDotMem, mLblMem);
    addMetricRow(mDotDisk, mLblDisk);

    root->addStretch();
}

void MiniMonitorWindow::setSubscribed(bool subscribed)
{
    if (mSubscribed == subscribed)
        return;
    mSubscribed = subscribed;

    if (mSubscribed) {
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Memory);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::DiskUsage);
        connect(DataRefreshService::ins(), &DataRefreshService::cpuUpdated,
                this, &MiniMonitorWindow::onCpuUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::memoryUpdated,
                this, &MiniMonitorWindow::onMemoryUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::diskUsageUpdated,
                this, &MiniMonitorWindow::onDiskUsageUpdated);
    } else {
        disconnect(DataRefreshService::ins(), nullptr, this, nullptr);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Memory);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::DiskUsage);
    }
}

void MiniMonitorWindow::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                      const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)

    if (percents.isEmpty())
        return;
    mLastCpuPercent = percents.at(0);

    const int coreCount = InfoManager::ins()->getCpuCoreCount();
    const double load1m = loadAvgs.isEmpty() ? 0.0 : loadAvgs.first();
    mHealthCalculator.setCpuScore(HealthScoreInputs::cpuScore(coreCount, load1m));
    updateScoreDisplay();
    updateMetricRows();
}

void MiniMonitorWindow::onMemoryUpdated(const MemorySnapshot &snap)
{
    mLastMemPercent = snap.total > 0
        ? qBound(0, static_cast<int>(100.0 * snap.used / snap.total), 100)
        : 0;

    mHealthCalculator.setMemoryScore(HealthScoreInputs::memoryScore(snap));
    updateScoreDisplay();
    updateMetricRows();
}

void MiniMonitorWindow::onDiskUsageUpdated(const QList<Disk> &disks)
{
    mLastDiskPercent = MiniMonitorFormatUtil::aggregateDiskUsedPercent(disks);

    mHealthCalculator.setDiskScore(HealthScoreInputs::diskScore(disks));
    updateScoreDisplay();
    updateMetricRows();
}

void MiniMonitorWindow::updateMetricRows()
{
    mLblCpu->setText(MiniMonitorFormatUtil::formatMetricRow(tr("CPU"), mLastCpuPercent));
    mLblMem->setText(MiniMonitorFormatUtil::formatMetricRow(tr("MEM"), mLastMemPercent));
    mLblDisk->setText(MiniMonitorFormatUtil::formatMetricRow(tr("DSK"), mLastDiskPercent));
}

void MiniMonitorWindow::updateScoreDisplay()
{
    const int score = mHealthCalculator.compositeScore();
    mLblScore->setText(QString::number(score));
    mLblScoreLabel->setText(mHealthCalculator.scoreLabel());

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;
    const QString colorHex = sv->value(MiniMonitorFormatUtil::scoreColorToken(score)).toString();
    mLblScore->setStyleSheet(
        QStringLiteral("font-family: %1; font-size: 28px; font-weight: 700; color: %2;")
            .arg(kMonoFontFamily, colorHex));
}

void MiniMonitorWindow::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString accent = sv->value("@accentColor").toString();
    if (auto *bar = findChild<QFrame *>("miniMonitorAccentBar"))
        bar->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 1px;").arg(accent));

    const QString metricStyle = QStringLiteral("font-family: %1;").arg(kMonoFontFamily);
    mLblCpu->setStyleSheet(metricStyle);
    mLblMem->setStyleSheet(metricStyle);
    mLblDisk->setStyleSheet(metricStyle);

    auto dotStyle = [](const QString &color) {
        return QStringLiteral("background-color: %1; border-radius: %2px;")
            .arg(color).arg(kSpacingUnit / 2);
    };
    mDotCpu->setStyleSheet(dotStyle(sv->value("@cpuColor").toString()));
    mDotMem->setStyleSheet(dotStyle(sv->value("@memoryColor").toString()));
    mDotDisk->setStyleSheet(dotStyle(sv->value("@diskColor").toString()));

    updateScoreDisplay();
}

void MiniMonitorWindow::persistGeometry()
{
    SettingManager::ins()->setMiniMonitorGeometry(saveGeometry());
}

void MiniMonitorWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setSubscribed(true);
    SettingManager::ins()->setMiniMonitorVisible(true);
    emit visibilityToggled(true);
}

void MiniMonitorWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    setSubscribed(false);
    persistGeometry();
    SettingManager::ins()->setMiniMonitorVisible(false);
    emit visibilityToggled(false);
}

void MiniMonitorWindow::closeEvent(QCloseEvent *event)
{
    // Closing via the native title-bar control hides rather than destroys —
    // "closed/reopened without restarting the app" (SSO-23855 AC).
    event->ignore();
    hide();
}
