#include "category_sparkline.h"

#include <QPainter>
#include <QPainterPath>

CategorySparkline::CategorySparkline(QWidget *parent)
    : QWidget(parent),
      mLineColor(QColor(0x5b, 0x9b, 0xd5))   // sensible fallback; callers override.
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CategorySparkline::setSamples(const QList<quint64> &samples)
{
    mSamples = samples;
    update();
}

void CategorySparkline::setLineColor(const QColor &color)
{
    mLineColor = color;
    update();
}

void CategorySparkline::paintEvent(QPaintEvent *)
{
    if (mSamples.size() < 2)
        return;   // nothing meaningful to draw; caller shows an em-dash label.

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    if (r.width() <= 0 || r.height() <= 0)
        return;

    quint64 maxVal = 0;
    for (quint64 v : mSamples)
        if (v > maxVal) maxVal = v;
    if (maxVal == 0)
        maxVal = 1;   // avoid divide-by-zero on an all-empty history.

    const double xStep = r.width() / static_cast<double>(mSamples.size() - 1);

    QPainterPath path;
    for (int i = 0; i < mSamples.size(); ++i) {
        const double x = r.left() + i * xStep;
        const double norm = static_cast<double>(mSamples.at(i)) / static_cast<double>(maxVal);
        const double y = r.bottom() - norm * r.height();
        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }

    QPen pen(mLineColor, 1.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(path);
}
