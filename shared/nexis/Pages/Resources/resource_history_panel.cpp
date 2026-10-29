#include "resource_history_panel.h"

#include "Managers/app_manager.h"
#include "Managers/resource_history_service.h"
#include "Managers/setting_manager.h"
#include "Utils/file_util.h"
#include "signal_mapper.h"
#include "utilities.h"

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

ResourceHistoryPanel::ResourceHistoryPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &ResourceHistoryPanel::refreshThemeColors);
    refreshThemeColors(); // also performs the initial refresh()
}

void ResourceHistoryPanel::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mCard = new QFrame(this);
    mCard->setObjectName("resourceHistoryCard");
    root->addWidget(mCard);

    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 14, 16, 14);
    card->setSpacing(8);

    // DS §3 accent bar — hidden until setElevated() reveals + colors it
    // (same pattern as HistoryChart / OomKillsWidget).
    mAccentBar = new QFrame(mCard);
    mAccentBar->setObjectName("sectionHeaderAccent");
    mAccentBar->setFixedWidth(3);
    mAccentBar->setFrameShape(QFrame::NoFrame);
    mAccentBar->setVisible(false);

    mTitle = new QLabel(tr("Resource History"), mCard);
    mTitle->setObjectName("resourceHistoryTitle");
    mTitle->setProperty("textRole", "panelTitle");

    // B13: populated by updateRangeOptions() (called from refresh()) rather
    // than hardcoded here, so the offered ranges track the configured
    // retention from the start.
    mRangeCombo = new QComboBox(mCard);
    mRangeCombo->setCursor(Qt::PointingHandCursor);

    mExportButton = new QPushButton(tr("Export CSV…"), mCard);
    mExportButton->setCursor(Qt::PointingHandCursor);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);
    titleRow->addWidget(mAccentBar);
    titleRow->addWidget(mTitle);
    titleRow->addStretch();
    titleRow->addWidget(mRangeCombo);
    titleRow->addWidget(mExportButton);
    card->addLayout(titleRow);

    // Disabled state (D4): opt-in prompt + shortcut to Settings.
    mDisabledState = new QWidget(mCard);
    auto *disabledLayout = new QHBoxLayout(mDisabledState);
    disabledLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *disabledLabel = new QLabel(
        tr("Resource history is off. Enable it in Settings to see CPU, load, and memory trends over time."),
        mDisabledState);
    disabledLabel->setWordWrap(true);
    mOpenSettingsButton = new QPushButton(tr("Open Settings"), mDisabledState);
    mOpenSettingsButton->setCursor(Qt::PointingHandCursor);
    disabledLayout->addWidget(disabledLabel, 1);
    disabledLayout->addWidget(mOpenSettingsButton);
    card->addWidget(mDisabledState);

    // Empty state (D4): history is on, but nothing in the selected range yet.
    mEmptyState = new QLabel(tr("No history recorded for this period yet."), mCard);
    mEmptyState->setWordWrap(true);
    mEmptyState->setVisible(false);
    card->addWidget(mEmptyState);

    // Chart content.
    mChartsContainer = new QWidget(mCard);
    auto *chartsLayout = new QVBoxLayout(mChartsContainer);
    chartsLayout->setContentsMargins(0, 0, 0, 0);
    chartsLayout->setSpacing(8);

    mCpuChart = new QChart();
    mCpuAxisX = new QDateTimeAxis();
    mCpuAxisX->setFormat(QStringLiteral("MMM d, h:mm AP"));
    mCpuAxisY = new QValueAxis();
    mCpuAxisY->setTitleText(QStringLiteral("%"));
    mLoadAxisY = new QValueAxis();
    mLoadAxisY->setTitleText(tr("Load"));
    mCpuChart->addAxis(mCpuAxisX, Qt::AlignBottom);
    mCpuChart->addAxis(mCpuAxisY, Qt::AlignLeft);
    mCpuChart->addAxis(mLoadAxisY, Qt::AlignRight);
    mCpuChart->legend()->setVisible(true);
    mCpuChart->legend()->setAlignment(Qt::AlignBottom);
    mCpuChart->setTitle(tr("CPU & Load"));
    mCpuChartView = new QChartView(mCpuChart, mChartsContainer);
    mCpuChartView->setRenderHint(QPainter::Antialiasing);
    mCpuChartView->setMinimumHeight(220);
    chartsLayout->addWidget(mCpuChartView);

    mMemChart = new QChart();
    mMemAxisX = new QDateTimeAxis();
    mMemAxisX->setFormat(QStringLiteral("MMM d, h:mm AP"));
    mMemAxisY = new QValueAxis();
    mMemAxisY->setTitleText(QStringLiteral("GiB"));
    mMemChart->addAxis(mMemAxisX, Qt::AlignBottom);
    mMemChart->addAxis(mMemAxisY, Qt::AlignLeft);
    mMemChart->legend()->setVisible(true);
    mMemChart->legend()->setAlignment(Qt::AlignBottom);
    mMemChart->setTitle(tr("Memory & Swap"));
    mMemChartView = new QChartView(mMemChart, mChartsContainer);
    mMemChartView->setRenderHint(QPainter::Antialiasing);
    mMemChartView->setMinimumHeight(220);
    chartsLayout->addWidget(mMemChartView);

    card->addWidget(mChartsContainer);
    mChartsContainer->setVisible(false);

    connect(mRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResourceHistoryPanel::onRangeChanged);
    connect(mExportButton, &QPushButton::clicked, this, &ResourceHistoryPanel::onExportCsvClicked);
    connect(mOpenSettingsButton, &QPushButton::clicked, this, &ResourceHistoryPanel::openSettingsRequested);
}

void ResourceHistoryPanel::setElevated(const QString &accentToken)
{
    mCard->setAttribute(Qt::WA_StyledBackground, true);
    mCard->setProperty("cardRole", "elevated");
    mCard->style()->unpolish(mCard);
    mCard->style()->polish(mCard);

    mAccentBar->setProperty("accentToken", accentToken);
    mAccentBar->setVisible(true);
    mAccentBar->style()->unpolish(mAccentBar);
    mAccentBar->style()->polish(mAccentBar);

    Utilities::addDropShadow(mCard, 90, 26);
}

void ResourceHistoryPanel::refresh()
{
    // B13 (SSO-26720): the panel can be hidden either because Live mode is
    // active or because the Resources page itself isn't current. Either
    // way, isVisible() is false and there's no point querying the store or
    // rebuilding charts nobody can see — catch up once in showEvent().
    if (!isVisible()) {
        mNeedsRefresh = true;
        return;
    }
    mNeedsRefresh = false;

    updateRangeOptions();

    if (!SettingManager::ins()->getResourceHistoryEnabled()) {
        mDisabledState->setVisible(true);
        mEmptyState->setVisible(false);
        mChartsContainer->setVisible(false);
        return;
    }

    mDisabledState->setVisible(false);
    rebuildCharts();
}

void ResourceHistoryPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (mNeedsRefresh)
        refresh();
}

void ResourceHistoryPanel::onRangeChanged(int index)
{
    Q_UNUSED(index)
    refresh();
}

QList<qint64> ResourceHistoryPanel::availableRangeSecsForRetention(int retentionDays)
{
    // B13: offer only ranges the configured retention can actually fill.
    // Chosen behavior (stated in the PR): hide a range outright once it
    // exceeds retention, rather than leaving it in the list disabled — a
    // disabled "30 d" next to an enabled "7 d" invites a click that can
    // only ever show an empty chart.
    static const QList<qint64> kAllRangeSecs = { 3600, 86400, 604800, 2592000 };

    QList<qint64> out;
    for (qint64 spanSecs : kAllRangeSecs) {
        const qint64 spanDays = (spanSecs + 86399) / 86400; // ceil to whole days
        if (spanDays <= retentionDays)
            out.append(spanSecs);
    }
    if (out.isEmpty())
        out.append(kAllRangeSecs.first()); // always offer something
    return out;
}

QString ResourceHistoryPanel::labelForRangeSecs(qint64 spanSecs)
{
    switch (spanSecs) {
    case 3600:    return tr("Last Hour");
    case 86400:   return tr("Last 24 Hours");
    case 604800:  return tr("Last 7 Days");
    case 2592000: return tr("Last 30 Days");
    default:      return QString::number(spanSecs);
    }
}

void ResourceHistoryPanel::updateRangeOptions()
{
    const int retentionDays = SettingManager::ins()->getResourceHistoryRetentionDays();
    const QList<qint64> available = availableRangeSecsForRetention(retentionDays);
    if (available == mAvailableRangeSecs)
        return;

    const qint64 previousSpan = mRangeCombo->count() > 0
        ? mRangeCombo->currentData().value<qint64>() : 0;

    mAvailableRangeSecs = available;
    const QSignalBlocker blocker(mRangeCombo);
    mRangeCombo->clear();
    for (qint64 spanSecs : available)
        mRangeCombo->addItem(labelForRangeSecs(spanSecs), QVariant::fromValue<qint64>(spanSecs));

    const int idx = mRangeCombo->findData(QVariant::fromValue<qint64>(previousSpan));
    mRangeCombo->setCurrentIndex(idx >= 0 ? idx : mRangeCombo->count() - 1);
}

qint64 ResourceHistoryPanel::bucketSecondsForSpan(qint64 spanSecs)
{
    // D3: keep every range at or under ~360 points per series.
    if (spanSecs <= 3600)
        return 10;
    if (spanSecs <= 86400)
        return 240;
    if (spanSecs <= 604800)
        return 1680;
    return 7200;
}

QList<QLineSeries *> ResourceHistoryPanel::buildSplitSeries(
    const QList<ResourceHistoryStore::Bucket> &buckets,
    qint64 bucketSecs,
    const std::function<std::optional<double>(const ResourceHistoryStore::Bucket &)> &accessor,
    const QString &name)
{
    QList<QLineSeries *> result;
    QLineSeries *current = nullptr;
    qint64 lastTs = 0;
    bool first = true;

    for (const ResourceHistoryStore::Bucket &b : buckets) {
        const std::optional<double> v = accessor(b);
        if (!v.has_value())
            continue;

        // B4: a gap wider than 3x the bucket width (app closed, machine
        // suspended) starts a new segment instead of drawing a straight
        // line across the missing time.
        const bool gap = !first && (b.ts - lastTs) > 3 * bucketSecs;
        if (!current || gap) {
            current = new QLineSeries();
            current->setName(result.isEmpty() ? name : QString());
            result.append(current);
        }

        current->append(static_cast<qreal>(b.ts) * 1000.0, *v);
        lastTs = b.ts;
        first = false;
    }

    return result;
}

void ResourceHistoryPanel::rebuildCharts()
{
    const qint64 spanSecs = mRangeCombo->currentData().value<qint64>();
    const qint64 bucketSecs = bucketSecondsForSpan(spanSecs);
    const qint64 toTs = QDateTime::currentSecsSinceEpoch();
    const qint64 fromTs = toTs - spanSecs;

    const QList<ResourceHistoryStore::Bucket> buckets =
        ResourceHistoryService::ins()->queryBuckets(fromTs, toTs, bucketSecs);

    if (buckets.isEmpty()) {
        mEmptyState->setVisible(true);
        mChartsContainer->setVisible(false);
        return;
    }

    mEmptyState->setVisible(false);
    mChartsContainer->setVisible(true);

    mCpuChart->removeAllSeries();
    mMemChart->removeAllSeries();

    QSettings *sv = AppManager::ins()->getStyleValues();
    const QColor cpuAvgColor(sv ? sv->value("@chartSeries01").toString() : QStringLiteral("#4C9AFF"));
    const QColor cpuMaxColor(sv ? sv->value("@chartSeries02").toString() : QStringLiteral("#A0C8FF"));
    const QColor loadColor(sv ? sv->value("@chartSeries03").toString() : QStringLiteral("#F4A261"));
    const QColor memColor(sv ? sv->value("@chartSeries01").toString() : QStringLiteral("#4C9AFF"));
    const QColor swapColor(sv ? sv->value("@chartSeries02").toString() : QStringLiteral("#A0C8FF"));

    auto addSeriesGroup = [](QChart *chart, QAbstractAxis *axisX, QAbstractAxis *axisY,
                              const QList<QLineSeries *> &group, const QColor &color) {
        for (int i = 0; i < group.size(); ++i) {
            QLineSeries *s = group.at(i);
            s->setColor(color);
            chart->addSeries(s);
            s->attachAxis(axisX);
            s->attachAxis(axisY);
            // Only the first segment of a split (gap-interrupted) metric
            // carries a name; hide the legend marker for the rest so a
            // suspend/resume gap doesn't show as repeated blank entries.
            if (i > 0) {
                for (QLegendMarker *marker : chart->legend()->markers(s))
                    marker->setVisible(false);
            }
        }
    };

    const auto cpuAvgSeries = buildSplitSeries(buckets, bucketSecs,
        [](const ResourceHistoryStore::Bucket &b) { return b.cpuAvg; }, tr("CPU avg"));
    const auto cpuMaxSeries = buildSplitSeries(buckets, bucketSecs,
        [](const ResourceHistoryStore::Bucket &b) { return b.cpuMax; }, tr("CPU max"));
    const auto loadSeries = buildSplitSeries(buckets, bucketSecs,
        [](const ResourceHistoryStore::Bucket &b) { return b.load1Avg; }, tr("Load (1m)"));

    addSeriesGroup(mCpuChart, mCpuAxisX, mCpuAxisY, cpuAvgSeries, cpuAvgColor);
    addSeriesGroup(mCpuChart, mCpuAxisX, mCpuAxisY, cpuMaxSeries, cpuMaxColor);
    addSeriesGroup(mCpuChart, mCpuAxisX, mLoadAxisY, loadSeries, loadColor);

    const auto memSeries = buildSplitSeries(buckets, bucketSecs,
        [](const ResourceHistoryStore::Bucket &b) -> std::optional<double> {
            if (!b.memUsedAvg.has_value())
                return std::nullopt;
            return *b.memUsedAvg / (1024.0 * 1024.0 * 1024.0);
        }, tr("Memory used"));
    const auto swapSeries = buildSplitSeries(buckets, bucketSecs,
        [](const ResourceHistoryStore::Bucket &b) -> std::optional<double> {
            if (!b.swapUsedAvg.has_value())
                return std::nullopt;
            return *b.swapUsedAvg / (1024.0 * 1024.0 * 1024.0);
        }, tr("Swap used"));

    addSeriesGroup(mMemChart, mMemAxisX, mMemAxisY, memSeries, memColor);
    addSeriesGroup(mMemChart, mMemAxisX, mMemAxisY, swapSeries, swapColor);

    const QDateTime fromDt = QDateTime::fromSecsSinceEpoch(fromTs);
    const QDateTime toDt = QDateTime::fromSecsSinceEpoch(toTs);
    mCpuAxisX->setRange(fromDt, toDt);
    mMemAxisX->setRange(fromDt, toDt);
    mCpuAxisY->setRange(0, 100);

    double maxLoad = 1.0;
    double maxMemGiB = 1.0;
    for (const ResourceHistoryStore::Bucket &b : buckets) {
        if (b.load1Avg.has_value())
            maxLoad = qMax(maxLoad, *b.load1Avg);
        if (b.memUsedMax.has_value())
            maxMemGiB = qMax(maxMemGiB, *b.memUsedMax / (1024.0 * 1024.0 * 1024.0));
    }
    mLoadAxisY->setRange(0, maxLoad * 1.2);
    mMemAxisY->setRange(0, maxMemGiB * 1.2);
}

void ResourceHistoryPanel::onExportCsvClicked()
{
    const qint64 spanSecs = mRangeCombo->currentData().value<qint64>();
    const qint64 toTs = QDateTime::currentSecsSinceEpoch();
    const qint64 fromTs = toTs - spanSecs;

    const QList<ResourceHistoryStore::Sample> rows =
        ResourceHistoryService::ins()->queryRaw(fromTs, toTs);

    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/resource_history.csv");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Resource History"), defaultPath, tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;

    auto cell = [](const std::optional<double> &v) {
        return v.has_value() ? QString::number(*v, 'f', 2) : QString();
    };
    auto cellU64 = [](const std::optional<quint64> &v) {
        return v.has_value() ? QString::number(*v) : QString();
    };

    QString csv = QStringLiteral(
        "timestamp_utc,source,cpu_pct,cpu_max_pct,load1,load5,load15,"
        "mem_used_bytes,mem_total_bytes,swap_used_bytes,swap_total_bytes\n");
    for (const ResourceHistoryStore::Sample &s : rows) {
        csv += QDateTime::fromSecsSinceEpoch(s.ts).toUTC().toString(Qt::ISODate);
        csv += QLatin1Char(',');
        csv += QString::number(static_cast<int>(s.source));
        csv += QLatin1Char(',');
        csv += cell(s.cpuPct);
        csv += QLatin1Char(',');
        csv += cell(s.cpuMaxPct);
        csv += QLatin1Char(',');
        csv += cell(s.load1);
        csv += QLatin1Char(',');
        csv += cell(s.load5);
        csv += QLatin1Char(',');
        csv += cell(s.load15);
        csv += QLatin1Char(',');
        csv += cellU64(s.memUsedBytes);
        csv += QLatin1Char(',');
        csv += cellU64(s.memTotalBytes);
        csv += QLatin1Char(',');
        csv += cellU64(s.swapUsedBytes);
        csv += QLatin1Char(',');
        csv += cellU64(s.swapTotalBytes);
        csv += QLatin1Char('\n');
    }

    if (!FileUtil::writeFile(path, csv)) {
        QMessageBox::warning(this, tr("Export Resource History"),
                              tr("Could not write %1.").arg(path));
    }
}

void ResourceHistoryPanel::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString chartLabelColor = sv->value("@chartLabelColor").toString();
    const QString chartGridColor = sv->value("@chartGridColor").toString();
    const QString background = sv->value("@chartBackgroundColor").toString();

    mCpuChart->setBackgroundBrush(QColor(background));
    mCpuChart->legend()->setLabelColor(chartLabelColor);
    mMemChart->setBackgroundBrush(QColor(background));
    mMemChart->legend()->setLabelColor(chartLabelColor);

    auto colorAxis = [&](QAbstractAxis *axis) {
        axis->setLabelsColor(chartLabelColor);
        axis->setGridLineColor(chartGridColor);
    };
    colorAxis(mCpuAxisX);
    colorAxis(mCpuAxisY);
    colorAxis(mLoadAxisY);
    colorAxis(mMemAxisX);
    colorAxis(mMemAxisY);

    // Series colors come from @chartSeriesNN tokens too — easiest to just
    // re-query and redraw with the new palette.
    refresh();
}
