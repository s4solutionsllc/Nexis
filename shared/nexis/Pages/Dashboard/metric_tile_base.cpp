#include "metric_tile_base.h"
#include "Managers/app_manager.h"

MetricTileBase::MetricTileBase(const QString &title, const QString &colorToken, QWidget *parent)
    : QWidget(parent),
      mTitle(title),
      mColorToken(colorToken)
{
    for (int i = 0; i < SPARKLINE_SIZE; ++i)
        mDataBuffer.append(0.0);
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
    for (int i = 0; i < SPARKLINE_SIZE; ++i)
        mDataBuffer.append(0.0);
    update();
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
    if (rangeId == "red-green")
        return { QColor("#E05454"), QColor("#FF6B1A"), QColor("#FFB347"), QColor("#2EC27E") };
    if (rangeId == "blue-red")
        return { QColor("#5B9BD5"), QColor("#9B59B6"), QColor("#FF6B1A"), QColor("#E05454") };
    if (rangeId == "teal-orange")
        return { QColor("#26A69A"), QColor("#8BC34A"), QColor("#FFB347"), QColor("#FF6B1A") };
    // Default: green-red
    return { QColor("#2EC27E"), QColor("#FFB347"), QColor("#FF6B1A"), QColor("#E05454") };
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
