#ifndef HISTORYCHART_H
#define HISTORYCHART_H

#include <QWidget>
#include <QDebug>
#include <QtCharts>
#include <QTimer>
#include <QLabel>
#include <QToolButton>
#include <QStackedWidget>

#include "Utils/format_util.h"

class AppManager;
class SignalMapper;

namespace Ui {
    class HistoryChart;
}

class HistoryChart : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryChart(const QString &title, const int &seriesCount,
                          QCategoryAxis* categoriAxisY = nullptr, QWidget *parent = 0,
                          AppManager *appManager = nullptr, SignalMapper *signalMapper = nullptr);
    ~HistoryChart();

    QVector<QSplineSeries *> getSeriesList() const;
    QCategoryAxis *getAxisY();
    void setYMax(double value);
    void setYRange(double min, double max);
    void setSeriesList(const QVector<QSplineSeries *> &seriesList);
    void setCategoryAxisYLabels();

    // DS §2/§3 (NEX-Resources): opt a chart into the elevated-card header
    // treatment — reveals + colors the accent bar (accentToken is one of the
    // #sectionHeaderAccent[accentToken=...] values in style.qss, e.g. "cpu",
    // "gpu", "disk"), pads the header/body, switches the plot surface to the
    // @chartBackgroundColor token, and applies the DS §2 drop shadow. Charts
    // that don't call this keep today's flat, unshadowed-by-default chrome.
    void setElevated(const QString &accentToken);

    // SSO-26719 (six-card Resources layout): a Dashboard-tile-style hero
    // number for the card header — the "current value" each subsystem card
    // now leads with instead of a verbose legend. `secondary` is an optional
    // smaller line under the hero (RAPL watts, a second rate, a drive temp);
    // passing an empty string hides whichever of the two has no content.
    void setHeaderValue(const QString &primary, const QString &secondary = QString());

    // SSO-26721: a value dropped from the chart (e.g. Memory's Wired/Active)
    // moves here instead — visible on hover over the header's hero value
    // rather than its own line. Call after setHeaderValue(); empty clears it.
    void setHeaderTooltip(const QString &tooltip);

    // SSO-26721: series whose scale is unrelated to the chart's primary Y
    // axis (Load & Pressure's 0-100% PSI lines alongside load averages that
    // range with core count) get a second, independent value axis on the
    // right. Returned series reuse the same @chartSeriesNN coloring
    // convention as the primary-axis series (appended to getSeriesList()),
    // but the caller must push their values into a separate RollingSeries —
    // sharing one RollingSeries across both axes would let the 0-100 PSI
    // points dominate windowYMax() and squash the load-average lines.
    QVector<QSplineSeries *> addSecondaryAxisSeries(int count, double rangeMin, double rangeMax,
                                                      const QString &axisTitle);

    // SSO-26721: a static dashed guide line (Load & Pressure's core-count
    // reference) — not rolling data, so it bypasses RollingSeries entirely.
    // Lazily created on first call; later calls just move it to the new
    // value. Uses @chartGridColor (a passive guide-line token already in
    // every theme) rather than a new per-purpose token.
    void setReferenceLine(double value, const QString &legendName);

    // A small checkable QToolButton (BUG-52: QToolButton, not QPushButton,
    // for an icon/text toggle on macOS) inserted into the header row, to the
    // left of the hero value. Caller owns wiring toggled(bool); HistoryChart
    // only reserves the header real estate.
    QToolButton *addHeaderToggle(const QString &text);

    // A lazily-created row between the header and the chart body, for a
    // compact cross-cutting status line (OOM kills, PSI) a card folds in
    // without giving it its own card. Caller adds widgets to
    // extrasRow()->layout() and shows/hides them itself; the row's own
    // visibility follows whether it has any visible children would require
    // tracking per-child, so callers toggle extrasRow()->setVisible() too
    // when they have nothing to show.
    QWidget *extrasRow();

    // The chart view is body page 0. A card that needs an alternate view in
    // the same footprint (CPU's per-core toggle) adds it here and switches
    // with setBodyPage() instead of juggling a second grid cell.
    int addBodyPage(QWidget *widget);
    void setBodyPage(int index);

private slots:
    void on_checkHistoryTitle_clicked(bool checked);

private:
    void init();
    // SSO-26718 (B14): shared by init() (so a chart picks up the current
    // theme immediately at creation — including one created lazily well
    // after the page's own construction-time sigChangedAppTheme replay, e.g.
    // the GPU chart) and by the sigChangedAppTheme handler (so a live theme
    // switch still repaints it).
    void applyThemeColors();

private:
    Ui::HistoryChart *ui;

    AppManager *mAppManager;
    SignalMapper *mSignalMapper;

    QString mTitle;
    int mSeriesCount;
    QChartView *mChartView;
    QChart *mChart;
    QStackedWidget *mBodyStack = nullptr;
    QVector<QSplineSeries *> mSeriesList;

    // SSO-26717 (B8): must default to nullptr — most charts are constructed
    // with categoriAxisY == nullptr and never set this in that path, but
    // getAxisY() / setCategoryAxisYLabels() both branch on it being null.
    QCategoryAxis *mAxisY = nullptr;

    QString mBackgroundToken = QStringLiteral("@historyChartBackgroundColor");

    // SSO-26719: lazily created on first use so charts that never call
    // setHeaderValue()/extrasRow() don't pay for the extra widgets.
    QWidget *mHeaderValueContainer = nullptr;
    QLabel  *mLblHeaderValue = nullptr;
    QLabel  *mLblHeaderSecondary = nullptr;
    QWidget *mExtrasRow = nullptr;

    // SSO-26721
    QValueAxis *mSecondaryAxisY = nullptr;
    QLineSeries *mReferenceLine = nullptr;
};

#endif // HISTORYCHART_H
