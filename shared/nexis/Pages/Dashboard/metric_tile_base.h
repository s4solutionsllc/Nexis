#ifndef METRIC_TILE_BASE_H
#define METRIC_TILE_BASE_H

#include <QWidget>
#include <QToolButton>
#include <QLabel>
#include <QPointF>
#include <QPushButton>
#include <QColor>
#include <functional>

class QVBoxLayout;
class QHBoxLayout;
class QFrame;

class MetricTileBase : public QWidget
{
    Q_OBJECT

public:
    enum TrendDirection { Rising, Falling, Stable };
    enum DisplayMode { Normal, Hero, Large, Compact };

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
    virtual void refreshThemeColors() = 0;

    QToolButton *gearButton();
    void setGearVisible(bool visible);
    void setInputName(const QString &friendly, const QString &model = QString());

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
    void setDriveHealthSegment(const QString &verdict, bool healthy);

protected:
    QString mTitle;
    QString mColorToken;
    QString mColorOverride;
    QString mColorRange;
    DisplayMode mDisplayMode = Normal;

    static const int SPARKLINE_SIZE = 60;
    QList<double> mDataBuffer;
    QList<QPointF> mPointsCache;   // mirrors mDataBuffer for QLineSeries::replace()

    // Shared UI members (created by helper methods below)
    QToolButton *mGearButton = nullptr;
    QLabel *mLblSubtitle = nullptr;
    QLabel *mLblTrend = nullptr;
    QPushButton *mBtnAction = nullptr;
    TrendDirection mCurrentTrend = Stable;

    // Unified tile chrome (GH#191 follow-up): a shared two-line header band
    // (type + input/source) and a footer band (hero value + trend) so tiles of
    // different body styles line up and always show what they are monitoring.
    QWidget *mHeaderWidget = nullptr;
    QWidget *mFooterWidget = nullptr;
    QHBoxLayout *mTitleRow = nullptr;
    QFrame *mAccentBar = nullptr;
    QLabel *mLblTitle = nullptr;     // type label, e.g. "CPU"
    QLabel *mLblInput = nullptr;     // muted input/source name in the title row (e.g. "Data-02")
    QLabel *mLblSource = nullptr;    // input/source label, e.g. "AMD Ryzen 7 5700X"
    QLabel *mLblHealth = nullptr;     // color-coded health verdict after the sub-header
    QLabel *mLblHealthSep = nullptr;  // "·" separator shown only with a verdict
    QLabel *mLblValue = nullptr;     // hero value shown in the footer
    QLabel *mLblValueSub = nullptr;  // muted secondary value beside the hero value
    QString mSourceFull;             // full (un-elided) source text

    // Shared helpers for subclass buildLayout()
    void createGearButton();
    void repositionGearButton();
    void createFooterLayout(QVBoxLayout *parent);

    // Unified chrome helpers. buildChrome() creates the root layout on `this`,
    // adds the header band (incl. gear) and returns the root so the subclass
    // can append its body; appendFooter() adds the shared footer band.
    QVBoxLayout *buildChrome();
    void appendFooter(QVBoxLayout *root);
    void setSource(const QString &text);
    void setHeroValue(const QString &text);
    void setHeroSecondary(const QString &text);
    void setTrendLabel(TrendDirection dir);   // sets text + hides the pill when empty
    void applyAccentColor(const QColor &color);
    void applyChromeForMode(DisplayMode mode);
    int  bodyTop() const;     // y just below the header band
    int  bodyBottom() const;  // y just above the footer band (or tile bottom)

    // Shared behavior
    virtual void updateTrend();
    void shiftDataPoint(double value);   // advances mDataBuffer and mPointsCache together
    void updateGearIcon();
    void applyActionButtonStyle(const QColor &metricColor, const QColor &hoverTextColor);

    QString trendText(TrendDirection dir) const;   // "↑ rising" (full)
    QString trendArrow(TrendDirection dir) const;   // "↑" only (shown in the pill)
    QString trendWord(TrendDirection dir) const;    // "Rising" (shown in the tooltip)
    QColor resolvedColor() const;
};

#endif // METRIC_TILE_BASE_H
