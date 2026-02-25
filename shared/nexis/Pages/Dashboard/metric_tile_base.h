#ifndef METRIC_TILE_BASE_H
#define METRIC_TILE_BASE_H

#include <QWidget>
#include <QToolButton>
#include <QColor>
#include <functional>

class MetricTileBase : public QWidget
{
    Q_OBJECT

public:
    enum TrendDirection { Rising, Falling, Stable };
    enum DisplayMode { Normal, Hero, Large };

    explicit MetricTileBase(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    virtual ~MetricTileBase() = default;

    // Core metric API (pure virtual)
    virtual void setValue(int percent, const QString &valueText) = 0;
    virtual void addDataPoint(double value) = 0;
    virtual void setSubtitle(const QString &text) = 0;
    virtual void setTrendDirection(TrendDirection dir) = 0;
    virtual void setSecondaryValue(const QString &text) = 0;
    virtual void setDisplayMode(DisplayMode mode) = 0;
    virtual void setQuickAction(const QString &text, std::function<void()> callback) = 0;
    virtual QToolButton *gearButton() = 0;
    virtual void setGearVisible(bool visible) = 0;
    virtual void refreshThemeColors() = 0;

    virtual void clearDataPoints();

    virtual void setColorOverride(const QString &hexColor);
    QString colorOverride() const { return mColorOverride; }

    virtual void setColorRange(const QString &rangeId);
    QString colorRange() const { return mColorRange; }

    static QList<QColor> rangeColors(const QString &rangeId);
    static QStringList availableRangeIds();
    static QString rangeDisplayName(const QString &rangeId);

    // Disk-specific (optional overrides with defaults)
    virtual void setDiskInfo(int percent, const QString &usedText, const QString &totalText);
    virtual void setDriveHealth(const QString &driveName, const QString &status, int healthPercent, bool healthy);
    virtual void clearDriveHealth();

protected:
    QString mTitle;
    QString mColorToken;
    QString mColorOverride;
    QString mColorRange;
    DisplayMode mDisplayMode = Normal;

    static const int SPARKLINE_SIZE = 60;
    QList<double> mDataBuffer;

    QString trendText(TrendDirection dir) const;
    QColor resolvedColor() const;
};

#endif // METRIC_TILE_BASE_H
