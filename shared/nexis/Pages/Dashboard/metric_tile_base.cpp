#include "metric_tile_base.h"

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

QString MetricTileBase::trendText(TrendDirection dir) const
{
    switch (dir) {
    case Rising:  return QStringLiteral("\u2191 rising");
    case Falling: return QStringLiteral("\u2193 falling");
    case Stable:  return QStringLiteral("\u2192 stable");
    }
    return {};
}
