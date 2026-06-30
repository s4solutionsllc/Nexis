#include "health_score_tile.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QResizeEvent>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

static const QMap<QString, QString> kComponentColorTokens = {
    {"cpu",     "@cpuColor"},
    {"memory",  "@memoryColor"},
    {"disk",    "@diskColor"},
    {"temp",    "@tempColor"},
    {"battery", "@batteryColor"},
    {"smart",   "@diskHealthColor"}
};

HealthScoreTile::HealthScoreTile(const QString &colorToken, QWidget *parent)
    : MetricTileBase("Health Score", colorToken, parent),
      mCurrentScore(100)
{
    setObjectName("healthScoreTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &HealthScoreTile::refreshThemeColors);
}

void HealthScoreTile::buildLayout()
{
    auto *mainLayout = buildChrome();
    setSource(tr("Composite score"));

    mLblScore = new QLabel("--", this);
    mLblScore->setObjectName("healthScoreValue");
    mLblScore->setAlignment(Qt::AlignHCenter);
    mLblScore->setStyleSheet("font-size: 32px; font-weight: bold;");
    mainLayout->addWidget(mLblScore);

    mLblScoreLabel = new QLabel("", this);
    mLblScoreLabel->setObjectName("metricTileSubtitle");
    mLblScoreLabel->setAlignment(Qt::AlignHCenter);
    mainLayout->addWidget(mLblScoreLabel);

    mainLayout->addStretch();
}

void HealthScoreTile::setValue(int percent, const QString &valueText)
{
    mCurrentScore = qBound(0, percent, 100);
    mLblScore->setText(valueText);
    update();
}

void HealthScoreTile::addDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);
}

void HealthScoreTile::setSubtitle(const QString &text)
{
    mLblScoreLabel->setText(text);
}

void HealthScoreTile::setTrendDirection(TrendDirection)
{
}

void HealthScoreTile::setSecondaryValue(const QString &)
{
}

void HealthScoreTile::setQuickAction(const QString &, std::function<void()>)
{
}

void HealthScoreTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
    applyChromeForMode(mode);
    update();
}

void HealthScoreTile::recalculate()
{
    int score = mCalculator.compositeScore();
    QString label = mCalculator.scoreLabel();

    setValue(score, QString::number(score));
    setSubtitle(label);

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    QString colorHex;
    if (score >= 75)
        colorHex = sv->value("@successColor").toString();
    else if (score >= 40)
        colorHex = sv->value("@warningColor").toString();
    else
        colorHex = sv->value("@destructiveColor").toString();

    mLblScore->setStyleSheet(
        QStringLiteral("font-size: 32px; font-weight: bold; color: %1;").arg(colorHex));
    applyAccentColor(QColor(colorHex));

    update();
}

void HealthScoreTile::refreshThemeColors()
{
    recalculate();
}

void HealthScoreTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // Breakdown bars are only for the larger tiers. Normal AND Compact show
    // just the score (Compact is a small tile — no room for bars). (GH#191)
    if (mDisplayMode == Large || mDisplayMode == Hero) {
        QPainter painter(this);
        paintBreakdownBars(painter);
    }
}

void HealthScoreTile::paintBreakdownBars(QPainter &painter)
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    QList<HealthComponent> comps = mCalculator.components();
    if (comps.isEmpty())
        return;

    painter.setRenderHint(QPainter::Antialiasing);

    QString bgColor = sv->value("@color02").toString();

    int startY = mLblScoreLabel->geometry().bottom() + 6;
    int leftMargin = 12;
    int rightMargin = 12;
    int availWidth = width() - leftMargin - rightMargin;

    int barHeight = 14;
    int barSpacing = 3;
    int labelWidth = 28;
    int scoreWidth = 24;
    int barLeft = leftMargin + labelWidth + 4;
    int barWidth = availWidth - labelWidth - scoreWidth - 8;

    QFont labelFont = painter.font();
    labelFont.setPixelSize(10);
    painter.setFont(labelFont);

    for (int i = 0; i < comps.size(); ++i) {
        const HealthComponent &comp = comps[i];
        int y = startY + i * (barHeight + barSpacing);

        QString token = kComponentColorTokens.value(comp.id, "@cpuColor");
        QColor barColor(sv->value(token).toString());

        painter.setPen(QColor(sv->value("@color05").toString()));
        painter.drawText(leftMargin, y, labelWidth, barHeight, Qt::AlignLeft | Qt::AlignVCenter, comp.label);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(bgColor));
        painter.drawRoundedRect(barLeft, y, barWidth, barHeight, 3, 3);

        int fillWidth = static_cast<int>(barWidth * comp.score / 100.0);
        if (fillWidth > 0) {
            painter.setBrush(barColor);
            painter.drawRoundedRect(barLeft, y, fillWidth, barHeight, 3, 3);
        }

        painter.setPen(QColor(sv->value("@color05").toString()));
        painter.drawText(barLeft + barWidth + 4, y, scoreWidth, barHeight,
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(comp.score));
    }
}

void HealthScoreTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionGearButton();
    update();
}
