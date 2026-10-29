#include "history_chart.h"
#include "ui_history_chart.h"
#include "dpi.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "utilities.h"
#include <QStyle>

HistoryChart::~HistoryChart()
{
    delete ui;
}

HistoryChart::HistoryChart(const QString &title, const int &seriesCount,
                           QCategoryAxis* categoriAxisY, QWidget *parent,
                           AppManager *appManager, SignalMapper *signalMapper) :
    QWidget(parent),
    ui(new Ui::HistoryChart),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mTitle(title),
    mSeriesCount(seriesCount),
    mChartView(new QChartView(this)),
    mChart(mChartView->chart())
{
    ui->setupUi(this);

    init();

    if (categoriAxisY) {
        mAxisY = categoriAxisY;
        mAxisY->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
        QList<QAbstractAxis*> verticalAxes = mChart->axes(Qt::Vertical);
        for (QAbstractAxis *axis : verticalAxes) {
            mChart->removeAxis(axis);
        }
        mChart->addAxis(mAxisY, Qt::AlignLeft);
        for (int i = 0; i < seriesCount; ++i) {
            mSeriesList.at(i)->attachAxis(mAxisY);
        }
    }
}

void HistoryChart::init()
{
    ui->lblHistoryTitle->setText(mTitle);
    // Width is not stylable via QSS (mirrors #metricTileAccent /
    // #sectionHeaderAccent convention, style.qss DS §3 doc comment).
    ui->sectionHeaderAccent->setFixedWidth(3);

    // add series to chart
    for (int i = 0; i < mSeriesCount; i++) {
        mSeriesList.append(new QSplineSeries);
        mChart->addSeries(mSeriesList.at(i));
    }

    mChartView->setRenderHint(QPainter::Antialiasing);

    QSettings *sv = mAppManager->getStyleValues();
    if (sv) {
        for (int i = 0; i < mSeriesList.count(); ++i) {
            QString token = QString("@chartSeries%1").arg(i + 1, 2, 10, QChar('0'));
            QColor c(sv->value(token).toString());
            dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(c);
        }
    }

    // Chart Settings
    mChart->createDefaultAxes();

    mChart->axes(Qt::Horizontal).first()->setRange(0, 60);
    mChart->axes(Qt::Horizontal).first()->setReverse(true);
    // SSO-26718 (B11): the axis now plots real elapsed seconds (see
    // RollingSeries) rather than a sample count, so "Seconds" is honest even
    // when DataRefreshService's tick interval isn't 1s (battery/unfocused).
    mChart->axes(Qt::Horizontal).first()->setTitleText(tr("Seconds"));

    mChart->setContentsMargins(-Dpi::scale(11), -Dpi::scale(11), -Dpi::scale(11), -Dpi::scale(11));
    mChart->setMargins(QMargins(Dpi::scale(20), 0, Dpi::scale(10), Dpi::scale(10)));

    // SSO-26719: the chart view is body page 0 of a stack so a card can swap
    // in an alternate view (CPU's per-core toggle) in the same footprint
    // instead of a second grid cell. Row 1 is reserved for extrasRow() (a
    // status line folded in above the body); the body itself moves to row 2.
    mBodyStack = new QStackedWidget(this);
    mBodyStack->addWidget(mChartView);
    ui->layoutHistoryChart->addWidget(mBodyStack, 2, 0, 1, 4);

    // Header row originally ended at the fullscreen checkbox in column 3;
    // move it out to column 5 so an optional per-card toggle (column 3) and
    // hero value (column 4) can sit between the title spacer and it.
    ui->layoutHistoryChart->removeWidget(ui->checkHistoryTitle);
    ui->layoutHistoryChart->addWidget(ui->checkHistoryTitle, 0, 5);

    // SSO-26718 (B14): apply the current theme immediately, not just on the
    // next sigChangedAppTheme — a chart created lazily (GPU, disk health,
    // PSI) well after the page's own construction-time replay would
    // otherwise sit with unthemed axis/grid/background colors until the user
    // happens to switch themes.
    applyThemeColors();
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme, this, [=] {
        applyThemeColors();
    });
}

void HistoryChart::applyThemeColors()
{
    QSettings *sv = mAppManager->getStyleValues();
    if (!sv) return;
    QString chartLabelColor = sv->value("@chartLabelColor").toString();
    QString chartGridColor = sv->value("@chartGridColor").toString();
    QString historyChartBackground = sv->value(mBackgroundToken).toString();

    mChart->axes(Qt::Horizontal).first()->setLabelsColor(chartLabelColor);
    mChart->axes(Qt::Horizontal).first()->setGridLineColor(chartGridColor);

    mChart->axes(Qt::Vertical).first()->setLabelsColor(chartLabelColor);
    mChart->axes(Qt::Vertical).first()->setGridLineColor(chartGridColor);

    mChart->setBackgroundBrush(QColor(historyChartBackground));
    mChart->legend()->setLabelColor(chartLabelColor);

    for (int i = 0; i < mSeriesList.count(); ++i) {
        QString token = QString("@chartSeries%1").arg(i + 1, 2, 10, QChar('0'));
        QColor c(sv->value(token).toString());
        dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(c);
    }

    if (mSecondaryAxisY) {
        mSecondaryAxisY->setLabelsColor(chartLabelColor);
        mSecondaryAxisY->setGridLineVisible(false);
    }

    if (mReferenceLine) {
        QPen pen = mReferenceLine->pen();
        pen.setColor(QColor(chartGridColor));
        mReferenceLine->setPen(pen);
    }
}

void HistoryChart::setYMax(double value)
{
    mChart->axes(Qt::Vertical).first()->setRange(0, value);
}

void HistoryChart::setYRange(double min, double max)
{
    mChart->axes(Qt::Vertical).first()->setRange(min, max);
}

QCategoryAxis *HistoryChart::getAxisY()
{
    return mAxisY;
}

void HistoryChart::setCategoryAxisYLabels()
{
    if (mAxisY) {
        for (const QString &label : mAxisY->categoriesLabels()){
            mAxisY->remove(label);
        }

        for (int i = 1; i < 5; ++i) {
            mAxisY->append(FormatUtil::formatBytes((mAxisY->max()/4)*i), (mAxisY->max()/4)*i);
        }
    }
}

QVector<QSplineSeries*> HistoryChart::getSeriesList() const
{
    return mSeriesList;
}

void HistoryChart::setSeriesList(const QVector<QSplineSeries *> &seriesList)
{
    Q_UNUSED(seriesList);
    // In Qt6, series() returns by value so modifying the returned list is a no-op.
    // The series are already being modified in-place via their pointers (insert, replace, removePoints),
    // so we only need to trigger a repaint.
    // SSO-26718: repaint() forces an immediate synchronous redraw on the GUI
    // thread; update() just schedules one for the next event-loop iteration,
    // which is all a per-tick chart refresh needs and lets Qt coalesce
    // repeated calls within one tick instead of blocking on each.
    mChartView->update();
}

void HistoryChart::on_checkHistoryTitle_clicked(bool checked)
{
    // SSO-26719: the six subsystem cards now live inside a grid container
    // (objectName "resourcesCardGrid"), not as direct children of the page's
    // outer charts layout — fullscreen one card by hiding its siblings
    // within that grid instead of every top-level section of the page (which
    // would also hide the Live/History switch and the Disk Usage launcher).
    QWidget *cardGrid = topLevelWidget()->findChild<QWidget*>("resourcesCardGrid");
    if (!cardGrid)
        return;

    QLayout *grid = cardGrid->layout();
    if (!grid)
        return;

    // An elevated chart (setElevated()) still IS the direct grid-layout
    // item — no wrapper widget is introduced — so `this` already matches
    // whatever itemAt(i)->widget() returns; walk up defensively in case a
    // future wrapper is added around a chart.
    QWidget *topLevelItem = this;
    while (topLevelItem->parentWidget() && topLevelItem->parentWidget() != cardGrid)
        topLevelItem = topLevelItem->parentWidget();

    for (int i = 0; i < grid->count(); ++i) {
        if (QWidget *w = grid->itemAt(i)->widget())
            w->setVisible(!checked || w == topLevelItem);
    }
}

void HistoryChart::setHeaderValue(const QString &primary, const QString &secondary)
{
    if (!mHeaderValueContainer) {
        mHeaderValueContainer = new QWidget(this);
        auto *vl = new QVBoxLayout(mHeaderValueContainer);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        mLblHeaderValue = new QLabel(mHeaderValueContainer);
        mLblHeaderValue->setObjectName("historyChartHeaderValue");
        mLblHeaderValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        mLblHeaderSecondary = new QLabel(mHeaderValueContainer);
        mLblHeaderSecondary->setObjectName("historyChartHeaderSecondary");
        mLblHeaderSecondary->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        vl->addWidget(mLblHeaderValue);
        vl->addWidget(mLblHeaderSecondary);

        ui->layoutHistoryChart->addWidget(mHeaderValueContainer, 0, 4,
                                           Qt::AlignRight | Qt::AlignVCenter);
    }

    mLblHeaderValue->setText(primary);
    mLblHeaderValue->setVisible(!primary.isEmpty());
    mLblHeaderSecondary->setText(secondary);
    mLblHeaderSecondary->setVisible(!secondary.isEmpty());
    mHeaderValueContainer->setVisible(!primary.isEmpty() || !secondary.isEmpty());
}

void HistoryChart::setHeaderTooltip(const QString &tooltip)
{
    if (mHeaderValueContainer)
        mHeaderValueContainer->setToolTip(tooltip);
}

QVector<QSplineSeries *> HistoryChart::addSecondaryAxisSeries(int count, double rangeMin,
                                                                double rangeMax,
                                                                const QString &axisTitle)
{
    if (!mSecondaryAxisY) {
        mSecondaryAxisY = new QValueAxis;
        // Integer labels ("0".."100") rather than QValueAxis's default
        // float formatting — in a half-width grid card there isn't room for
        // two multi-decimal Y axes side by side without Qt eliding one.
        mSecondaryAxisY->setLabelFormat(QStringLiteral("%d"));
        mChart->addAxis(mSecondaryAxisY, Qt::AlignRight);
    }
    mSecondaryAxisY->setRange(rangeMin, rangeMax);
    mSecondaryAxisY->setTitleText(axisTitle);

    QAbstractAxis *axisX = mChart->axes(Qt::Horizontal).first();

    QVector<QSplineSeries *> added;
    added.reserve(count);
    for (int i = 0; i < count; ++i) {
        auto *s = new QSplineSeries;
        mChart->addSeries(s);
        s->attachAxis(axisX);
        s->attachAxis(mSecondaryAxisY);
        mSeriesList.append(s);
        added.append(s);
    }

    applyThemeColors();
    return added;
}

void HistoryChart::setReferenceLine(double value, const QString &legendName)
{
    if (!mReferenceLine) {
        mReferenceLine = new QLineSeries;
        mReferenceLine->setName(legendName);
        QPen pen(mReferenceLine->pen());
        pen.setStyle(Qt::DashLine);
        mReferenceLine->setPen(pen);

        mChart->addSeries(mReferenceLine);
        mReferenceLine->attachAxis(mChart->axes(Qt::Horizontal).first());
        mReferenceLine->attachAxis(mChart->axes(Qt::Vertical).first());

        applyThemeColors();
    }

    mReferenceLine->replace(QList<QPointF>{ QPointF(0, value), QPointF(60, value) });
}

QToolButton *HistoryChart::addHeaderToggle(const QString &text)
{
    auto *btn = new QToolButton(this);
    btn->setText(text);
    btn->setCheckable(true);
    btn->setAutoRaise(true); // BUG-52: QToolButton, not QPushButton, for macOS
    btn->setCursor(Qt::PointingHandCursor);
    ui->layoutHistoryChart->addWidget(btn, 0, 3, Qt::AlignRight | Qt::AlignVCenter);
    return btn;
}

QWidget *HistoryChart::extrasRow()
{
    if (!mExtrasRow) {
        mExtrasRow = new QWidget(this);
        auto *l = new QVBoxLayout(mExtrasRow);
        l->setContentsMargins(0, 2, 0, 2);
        l->setSpacing(4);
        mExtrasRow->setVisible(false);
        ui->layoutHistoryChart->addWidget(mExtrasRow, 1, 0, 1, 4);
    }
    return mExtrasRow;
}

int HistoryChart::addBodyPage(QWidget *widget)
{
    return mBodyStack->addWidget(widget);
}

void HistoryChart::setBodyPage(int index)
{
    mBodyStack->setCurrentIndex(index);
}

void HistoryChart::setElevated(const QString &accentToken)
{
    // DS §2 elevated container + DS §3 accent-bar header (style.qss
    // [cardRole="elevated"] / #sectionHeaderAccent[accentToken=...] — NEX
    // F1/F2 groundwork). Scoped to this instance only: ui->layoutHistoryChart
    // and ui->sectionHeaderAccent belong to this HistoryChart's own ui, so
    // charts that never call this (Memory, Network, ...) are unaffected.
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("cardRole", "elevated");

    ui->sectionHeaderAccent->setProperty("compact", true);
    ui->sectionHeaderAccent->setProperty("accentToken", accentToken);
    ui->sectionHeaderAccent->setVisible(true);
    style()->unpolish(ui->sectionHeaderAccent);
    style()->polish(ui->sectionHeaderAccent);

    ui->layoutHistoryChart->setContentsMargins(16, 12, 16, 14);

    mBackgroundToken = QStringLiteral("@chartBackgroundColor");
    QSettings *sv = mAppManager->getStyleValues();
    if (sv)
        mChart->setBackgroundBrush(QColor(sv->value(mBackgroundToken).toString()));

    Utilities::addDropShadow(this, 90, 26);
}
