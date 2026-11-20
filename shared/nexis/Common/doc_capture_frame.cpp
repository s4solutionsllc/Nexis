#include "doc_capture_frame.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace DocCaptureFrame {

QImage decorate(const QImage &content, const QString &title, const Style &style)
{
    const qreal dpr = content.devicePixelRatio() > 0 ? content.devicePixelRatio() : 1.0;
    const int margin = static_cast<int>(std::lround(kShadowMargin * dpr));
    const int bar = static_cast<int>(std::lround(kTitleBarHeight * dpr));
    const qreal radius = kCornerRadius * dpr;

    const int winW = content.width();
    const int winH = content.height() + bar;

    QImage out(winW + 2 * margin, winH + 2 * margin, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF win(margin, margin, winW, winH);

    // Soft drop shadow: concentric rounded rects with falling alpha, offset
    // slightly downward like a desktop compositor shadow.
    const int layers = margin;
    for (int i = layers; i > 0; --i) {
        QColor c = style.shadow;
        const qreal t = 1.0 - static_cast<qreal>(i) / layers;   // 0 at the outside
        c.setAlphaF(style.shadow.alphaF() * t * t * 0.35);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal grow = i;
        p.drawRoundedRect(win.adjusted(-grow, -grow + dpr * 6, grow, grow + dpr * 6),
                          radius + grow, radius + grow);
    }

    // Window body clipped to rounded corners.
    QPainterPath body;
    body.addRoundedRect(win, radius, radius);
    p.save();
    p.setClipPath(body);
    p.fillRect(QRectF(win.left(), win.top(), winW, bar), style.titleBar);
    QImage contentCopy = content;
    contentCopy.setDevicePixelRatio(1.0);
    p.drawImage(QPointF(win.left(), win.top() + bar), contentCopy);
    p.restore();

    // Title bar separator and outer border.
    p.setPen(QPen(style.border, std::max<qreal>(1.0, dpr * 0.75)));
    p.drawLine(QPointF(win.left(), win.top() + bar - 0.5), QPointF(win.right(), win.top() + bar - 0.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(win.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // Window buttons.
    const qreal d = 12 * dpr;
    const qreal gap = 8 * dpr;
    const qreal cy = win.top() + bar / 2.0;
    p.setPen(Qt::NoPen);
    if (style.layout == ButtonLayout::MacOS) {
        const QColor colors[3] = {style.closeButton, style.minButton, style.maxButton};
        qreal x = win.left() + 14 * dpr;
        for (const QColor &c : colors) {
            p.setBrush(c);
            p.drawEllipse(QRectF(x, cy - d / 2, d, d));
            x += d + gap;
        }
    } else {
        QColor c = style.titleText;
        c.setAlphaF(0.35);
        p.setBrush(c);
        qreal x = win.right() - 14 * dpr - d;
        for (int i = 0; i < 3; ++i) {
            p.drawEllipse(QRectF(x, cy - d / 2, d, d));
            x -= d + gap;
        }
    }

    // Title.
    if (!title.isEmpty()) {
        QFont f = p.font();
        f.setPixelSize(static_cast<int>(std::lround(13 * dpr)));
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(style.titleText);
        p.drawText(QRectF(win.left(), win.top(), winW, bar), Qt::AlignCenter, title);
    }

    p.end();
    out.setDevicePixelRatio(dpr);
    return out;
}

} // namespace DocCaptureFrame
