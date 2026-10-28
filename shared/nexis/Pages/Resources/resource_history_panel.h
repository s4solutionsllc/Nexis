#ifndef RESOURCE_HISTORY_PANEL_H
#define RESOURCE_HISTORY_PANEL_H

#include <QWidget>
#include <QtCharts>

#include <functional>
#include <optional>

#include <Tools/resource_history_store.h>

class QFrame;
class QLabel;
class QComboBox;
class QPushButton;

// SSO-10930 / GH#220: historical (1h/24h/7d/30d) view over the persistent
// CPU/load/memory log recorded by ResourceHistoryService. Deliberately a
// separate panel rather than retrofitting HistoryChart (D2 of the spec) —
// HistoryChart is hardwired to a reversed 0-60s QValueAxis and isn't
// reusable for a real QDateTimeAxis timeline. "Live" needs no entry here;
// the existing CPU/Memory HistoryChart widgets above already show it
// unchanged.
class ResourceHistoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ResourceHistoryPanel(QWidget *parent = nullptr);

    // DS §2/§3 (NEX-Resources): same elevated-card + accent-bar recipe as
    // HistoryChart::setElevated() / OomKillsWidget::setElevated().
    void setElevated(const QString &accentToken);

    // Re-reads the enabled setting and (if on) re-queries and redraws for
    // the currently selected range. Cheap enough to call on page activation,
    // on the enabled/disabled toggle, and whenever the store changes.
    void refresh();

signals:
    // Emitted when the user clicks through from the disabled-state prompt.
    void openSettingsRequested();

private slots:
    void onRangeChanged(int index);
    void onExportCsvClicked();
    void refreshThemeColors();

private:
    void buildUI();
    void rebuildCharts();
    static qint64 bucketSecondsForSpan(qint64 spanSecs);

    // Builds one or more QLineSeries for `accessor(bucket)` split wherever
    // the gap between consecutive buckets exceeds 3x bucketSecs (B4) so a
    // suspend/close doesn't draw as a straight interpolated line across the
    // missing time. Returns the created series (already added to the
    // caller's chart, not yet attached to an axis).
    static QList<QLineSeries *> buildSplitSeries(
        const QList<ResourceHistoryStore::Bucket> &buckets,
        qint64 bucketSecs,
        const std::function<std::optional<double>(const ResourceHistoryStore::Bucket &)> &accessor,
        const QString &name);

    QFrame *mCard = nullptr;
    QFrame *mAccentBar = nullptr;
    QLabel *mTitle = nullptr;
    QComboBox *mRangeCombo = nullptr;
    QPushButton *mExportButton = nullptr;

    QWidget *mDisabledState = nullptr;
    QPushButton *mOpenSettingsButton = nullptr;
    QLabel *mEmptyState = nullptr;
    QWidget *mChartsContainer = nullptr;

    QChartView *mCpuChartView = nullptr;
    QChart *mCpuChart = nullptr;
    QDateTimeAxis *mCpuAxisX = nullptr;
    QValueAxis *mCpuAxisY = nullptr;
    QValueAxis *mLoadAxisY = nullptr;

    QChartView *mMemChartView = nullptr;
    QChart *mMemChart = nullptr;
    QDateTimeAxis *mMemAxisX = nullptr;
    QValueAxis *mMemAxisY = nullptr;
};

#endif // RESOURCE_HISTORY_PANEL_H
