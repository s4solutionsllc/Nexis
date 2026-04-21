#include "metric_tile_base.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include "Managers/app_manager.h"

MetricTileBase::MetricTileBase(const QString &title, const QString &colorToken, QWidget *parent)
    : QWidget(parent),
      mTitle(title),
      mColorToken(colorToken)
{
    mDataBuffer.reserve(SPARKLINE_SIZE);
    mPointsCache.reserve(SPARKLINE_SIZE);
    for (int i = 0; i < SPARKLINE_SIZE; ++i) {
        mDataBuffer.append(0.0);
        mPointsCache.append(QPointF(i, 0.0));
    }
}

void MetricTileBase::setDiskInfo(int percent, const QString &usedText, const QString &totalText)
{
    setValue(percent, QString("%1%").arg(percent));
    setSubtitle(QString("%1 / %2").arg(usedText, totalText));
}

void MetricTileBase::setDriveHealth(const QString &, const QString &, int, bool)
{
}

void MetricTileBase::clearDriveHealth()
{
}

void MetricTileBase::clearDataPoints()
{
    mDataBuffer.clear();
    mPointsCache.clear();
    for (int i = 0; i < SPARKLINE_SIZE; ++i) {
        mDataBuffer.append(0.0);
        mPointsCache.append(QPointF(i, 0.0));
    }
    update();
}

void MetricTileBase::shiftDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);

    // Shift y-values in the cache; x coords 0..SPARKLINE_SIZE-1 stay fixed.
    const int n = mDataBuffer.size();
    if (mPointsCache.size() != n) {
        mPointsCache.clear();
        mPointsCache.reserve(n);
        for (int i = 0; i < n; ++i)
            mPointsCache.append(QPointF(i, mDataBuffer.at(i)));
        return;
    }
    for (int i = 0; i < n; ++i)
        mPointsCache[i].setY(mDataBuffer.at(i));
}

void MetricTileBase::setColorOverride(const QString &hexColor)
{
    mColorOverride = hexColor;
    refreshThemeColors();
}

void MetricTileBase::setColorRange(const QString &rangeId)
{
    mColorRange = rangeId;
    refreshThemeColors();
}

QList<QColor> MetricTileBase::rangeColors(const QString &rangeId)
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    auto c = [&](const QString &token, const QString &fallback) {
        return QColor(sv ? sv->value(token).toString() : fallback);
    };

    QColor disk    = c("@diskColor",    "#E05454");
    QColor cpu     = c("@cpuColor",     "#FF6B1A");
    QColor memory  = c("@memoryColor",  "#FFB347");
    QColor battery = c("@batteryColor", "#2EC27E");
    QColor temp    = c("@tempColor",    "#5B9BD5");
    QColor network = c("@networkColor", "#26A69A");
    QColor purple("#9B59B6");
    QColor lime("#8BC34A");

    if (rangeId == "red-green")
        return { disk, cpu, memory, battery };
    if (rangeId == "blue-red")
        return { temp, purple, cpu, disk };
    if (rangeId == "teal-orange")
        return { network, lime, memory, cpu };
    // Default: green-red
    return { battery, memory, cpu, disk };
}

QStringList MetricTileBase::availableRangeIds()
{
    return { "green-red", "red-green", "blue-red", "teal-orange" };
}

QString MetricTileBase::rangeDisplayName(const QString &rangeId)
{
    if (rangeId == "green-red")   return tr("Green \u2192 Red");
    if (rangeId == "red-green")   return tr("Red \u2192 Green");
    if (rangeId == "blue-red")    return tr("Blue \u2192 Red");
    if (rangeId == "teal-orange") return tr("Teal \u2192 Orange");
    return rangeId;
}

QColor MetricTileBase::resolvedColor() const
{
    if (!mColorOverride.isEmpty())
        return QColor(mColorOverride);

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (sv)
        return QColor(sv->value(mColorToken).toString());
    return QColor();
}

QString MetricTileBase::trendText(TrendDirection dir) const
{
    switch (dir) {
    case Rising:  return QStringLiteral("\u2191 rising");
    case Falling: return QStringLiteral("\u2193 falling");
    case Stable:  return QStringLiteral("\u2192 stable");
    }
    return {};
}

void MetricTileBase::createGearButton()
{
    mGearButton = new QToolButton(this);
    mGearButton->setObjectName("btnMetricGear");
    mGearButton->setFixedSize(24, 24);
    mGearButton->setIconSize(QSize(14, 14));
    mGearButton->setAutoRaise(true);
    mGearButton->setCursor(Qt::PointingHandCursor);
    mGearButton->setFocusPolicy(Qt::NoFocus);
    mGearButton->hide();
    mGearButton->raise();
    repositionGearButton();
}

void MetricTileBase::repositionGearButton()
{
    if (!mGearButton)
        return;

    QLabel *titleLabel = findChild<QLabel*>("metricTileTitle");
    if (!titleLabel)
        titleLabel = findChild<QLabel*>("diskTileTitle");
    if (!titleLabel)
        titleLabel = findChild<QLabel*>("networkTileTitle");

    if (titleLabel && titleLabel->width() > 0) {
        const QRect titleRect = titleLabel->geometry();
        const int textWidth = titleLabel->fontMetrics().horizontalAdvance(titleLabel->text());
        const int x = titleRect.left() + textWidth + 6;
        const int y = titleRect.top() + (titleRect.height() - mGearButton->height()) / 2;
        mGearButton->move(x, y);
    } else {
        mGearButton->move(8, 8);
    }
}

QToolButton *MetricTileBase::gearButton()
{
    return mGearButton;
}

void MetricTileBase::setGearVisible(bool visible)
{
    if (mGearButton)
        mGearButton->setVisible(visible);
}

void MetricTileBase::updateGearIcon()
{
    if (!mGearButton)
        return;
    QString theme = AppManager::ins()->resolveThemeName();
    mGearButton->setIcon(QIcon(
        QString(":/static/themes/%1/img/sidebar-icons/settings.svg").arg(theme)));
}

void MetricTileBase::createFooterLayout(QVBoxLayout *parent)
{
    auto *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 0, 0, 0);

    mLblSubtitle = new QLabel(this);
    mLblSubtitle->setObjectName("metricTileSubtitle");

    mLblTrend = new QLabel(this);
    mLblTrend->setObjectName("metricTileSubtitle");
    mLblTrend->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    mBtnAction = new QPushButton(this);
    mBtnAction->setObjectName("metricTileAction");
    mBtnAction->setCursor(Qt::PointingHandCursor);
    mBtnAction->setFocusPolicy(Qt::NoFocus);
    mBtnAction->hide();
    mBtnAction->setFixedHeight(22);

    footerLayout->addWidget(mLblSubtitle);
    footerLayout->addStretch();
    footerLayout->addWidget(mLblTrend);
    footerLayout->addWidget(mBtnAction);
    parent->addLayout(footerLayout);
}

void MetricTileBase::updateTrend()
{
    if (mDataBuffer.size() < 10) {
        setTrendDirection(Stable);
        return;
    }

    int size = mDataBuffer.size();
    double recentAvg = 0, oldAvg = 0;
    for (int i = size - 5; i < size; ++i)
        recentAvg += mDataBuffer.at(i);
    for (int i = size - 10; i < size - 5; ++i)
        oldAvg += mDataBuffer.at(i);
    recentAvg /= 5.0;
    oldAvg /= 5.0;

    if (oldAvg > 0.001) {
        double diff = (recentAvg - oldAvg) / oldAvg;
        if (diff > 0.05)
            setTrendDirection(Rising);
        else if (diff < -0.05)
            setTrendDirection(Falling);
        else
            setTrendDirection(Stable);
    } else {
        setTrendDirection(recentAvg > 0.5 ? Rising : Stable);
    }
}

void MetricTileBase::applyActionButtonStyle(const QColor &metricColor, const QColor &hoverTextColor)
{
    if (!mBtnAction)
        return;
    QString colorHex = metricColor.name();
    mBtnAction->setStyleSheet(
        "QPushButton#metricTileAction {"
        "  font-size: 8pt;"
        "  padding: 2px 8px;"
        "  border-radius: 10px;"
        "  border: 1px solid " + colorHex + ";"
        "  color: " + colorHex + ";"
        "  background: transparent;"
        "}"
        "QPushButton#metricTileAction:hover {"
        "  background-color: " + colorHex + ";"
        "  color: " + hoverTextColor.name() + ";"
        "}"
    );
}
