#include "metric_tile_base.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QStyle>
#include <QFontMetrics>
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
    // SSO-3399 / BUG-47: route through values.ini theme tokens (with the
    // previous hardcoded hex as a build-time fallback) so swatches respect
    // light/dark themes like every other metric color.
    QColor purple  = c("@rangePurpleColor", "#9B59B6");
    QColor lime    = c("@rangeLimeColor",   "#8BC34A");

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
    mGearButton->hide();
}

// The gear now lives in the header layout (top-right), so it no longer needs
// manual positioning. This hook still fires from each tile's resizeEvent, so we
// reuse it to re-elide the source line to the tile's current width.
void MetricTileBase::repositionGearButton()
{
    if (!mLblSource)
        return;
    QFontMetrics fm(mLblSource->font());
    const int avail = qMax(0, mLblSource->width() - 1);
    mLblSource->setText(fm.elidedText(mSourceFull, Qt::ElideRight, avail));
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
    mBtnAction->hide();
    mBtnAction->setFixedHeight(22);

    footerLayout->addWidget(mLblSubtitle);
    footerLayout->addStretch();
    footerLayout->addWidget(mLblTrend);
    footerLayout->addWidget(mBtnAction);
    parent->addLayout(footerLayout);
}

QVBoxLayout *MetricTileBase::buildChrome()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 10);
    root->setSpacing(6);

    // Header band: accent bar + [ title row (title, gear) / source line ]
    mHeaderWidget = new QWidget(this);
    mHeaderWidget->setObjectName("metricTileHeader");
    auto *headerLayout = new QHBoxLayout(mHeaderWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    mAccentBar = new QFrame(mHeaderWidget);
    mAccentBar->setObjectName("metricTileAccent");
    mAccentBar->setFixedWidth(3);
    mAccentBar->setMinimumHeight(26);
    mAccentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    headerLayout->addWidget(mAccentBar);

    auto *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(0);

    mTitleRow = new QHBoxLayout();
    mTitleRow->setContentsMargins(0, 0, 0, 0);
    mTitleRow->setSpacing(6);

    mLblTitle = new QLabel(mTitle, mHeaderWidget);
    mLblTitle->setObjectName("metricTileTitle");
    mTitleRow->addWidget(mLblTitle);

    mLblInput = new QLabel(mHeaderWidget);
    mLblInput->setObjectName("metricTileInput");
    mLblInput->hide();   // shown only when setInputName() is called (disk tiles)
    mTitleRow->addWidget(mLblInput);

    mTitleRow->addStretch();

    createGearButton();
    mTitleRow->addWidget(mGearButton, 0, Qt::AlignTop | Qt::AlignRight);

    textCol->addLayout(mTitleRow);

    mLblSource = new QLabel(mHeaderWidget);
    mLblSource->setObjectName("metricTileSource");
    mLblSource->setMinimumHeight(13);
    textCol->addWidget(mLblSource);

    headerLayout->addLayout(textCol, 1);

    root->addWidget(mHeaderWidget);
    return root;
}

void MetricTileBase::appendFooter(QVBoxLayout *root)
{
    mFooterWidget = new QWidget(this);
    mFooterWidget->setObjectName("metricTileFooter");
    auto *footerLayout = new QHBoxLayout(mFooterWidget);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(6);

    mLblValue = new QLabel(this);
    mLblValue->setObjectName("metricTileValue");
    mLblValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    mLblValueSub = new QLabel(this);
    mLblValueSub->setObjectName("metricTileSubtitle");
    mLblValueSub->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    mLblValueSub->hide();

    mLblTrend = new QLabel(this);
    mLblTrend->setObjectName("metricTileTrend");
    mLblTrend->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mLblTrend->hide();   // shown only once a trend value is set (no empty pill)

    mBtnAction = new QPushButton(this);
    mBtnAction->setObjectName("metricTileAction");
    mBtnAction->setCursor(Qt::PointingHandCursor);
    mBtnAction->hide();
    mBtnAction->setFixedHeight(22);

    footerLayout->addWidget(mLblValue);
    footerLayout->addWidget(mLblValueSub);
    footerLayout->addStretch();
    footerLayout->addWidget(mLblTrend);
    footerLayout->addWidget(mBtnAction);

    root->addWidget(mFooterWidget);
}

void MetricTileBase::setSource(const QString &text)
{
    mSourceFull = text;
    if (!mLblSource)
        return;
    mLblSource->setToolTip(text);
    QFontMetrics fm(mLblSource->font());
    const int avail = qMax(0, mLblSource->width() - 1);
    mLblSource->setText(avail > 0 ? fm.elidedText(text, Qt::ElideRight, avail) : text);
}

void MetricTileBase::setInputName(const QString &friendly, const QString &model)
{
    if (!mLblInput)
        return;
    mLblInput->setText(friendly);
    mLblInput->setVisible(!friendly.isEmpty());
    if (!model.isEmpty()) {
        mLblInput->setToolTip(model);
        setToolTip(model);
    }
}

void MetricTileBase::setHeroValue(const QString &text)
{
    if (mLblValue)
        mLblValue->setText(text);
}

void MetricTileBase::setHeroSecondary(const QString &text)
{
    if (!mLblValueSub)
        return;
    mLblValueSub->setText(text);
    mLblValueSub->setVisible(!text.isEmpty());
}

void MetricTileBase::setTrendLabel(TrendDirection dir)
{
    mCurrentTrend = dir;
    if (!mLblTrend)
        return;
    const QString t = trendText(dir);
    mLblTrend->setText(t);
    // Hide an empty pill; also stay hidden while a quick-action button occupies
    // the footer's right side.
    const bool actionActive = mBtnAction && mBtnAction->isVisible();
    mLblTrend->setVisible(!t.isEmpty() && !actionActive);
}

void MetricTileBase::applyAccentColor(const QColor &color)
{
    if (mAccentBar)
        mAccentBar->setStyleSheet(
            QString("#metricTileAccent{background-color:%1;border-radius:1px;}").arg(color.name()));
}

void MetricTileBase::applyChromeForMode(DisplayMode mode)
{
    const bool compact = (mode == Compact);
    // The footer carries the numeric reading in every mode, so it stays visible.
    // On a 1x1 (compact) tile we collapse the source line to reclaim height.
    if (mLblSource)
        mLblSource->setVisible(!compact);
    if (mLblValue) {
        mLblValue->setProperty("heroMode", mode == Hero ? "true" : "false");
        mLblValue->setProperty("largeMode", mode == Large ? "true" : "false");
        mLblValue->style()->unpolish(mLblValue);
        mLblValue->style()->polish(mLblValue);
    }
}

int MetricTileBase::bodyTop() const
{
    if (mHeaderWidget)
        return mHeaderWidget->geometry().bottom() + 6;
    return 8;
}

int MetricTileBase::bodyBottom() const
{
    if (mFooterWidget && mFooterWidget->isVisible())
        return mFooterWidget->geometry().top() - 6;
    return height() - 8;
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
