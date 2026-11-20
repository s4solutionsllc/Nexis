#ifndef DOC_CAPTURE_FRAME_H
#define DOC_CAPTURE_FRAME_H

#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>

// Draws neutral window chrome (title bar, rounded corners, drop shadow)
// around a QWidget::grab() of the window's client area, so documentation
// screenshots produced by `nexis --capture-docs` look like window captures
// on both platforms. Colors are supplied by the caller from theme tokens
// (BUG-47: no hardcoded colors in C++).
namespace DocCaptureFrame {

enum class ButtonLayout {
    MacOS,      // three colored dots on the left
    Linux,      // three neutral circles on the right
};

struct Style
{
    QColor titleBar;      // @sidebar
    QColor titleText;     // @textSecondary
    QColor border;        // @borderColor
    QColor shadow;        // @shadowColor
    QColor closeButton;   // @dangerFill
    QColor minButton;     // @warningColor
    QColor maxButton;     // @successColor
    ButtonLayout layout = ButtonLayout::Linux;
};

// Logical-pixel geometry; multiplied by the content's devicePixelRatio.
constexpr int kTitleBarHeight = 32;
constexpr int kShadowMargin = 40;
constexpr int kCornerRadius = 10;

// Returns a transparent-background image of size
// content + 2*margin (w), content + titleBar + 2*margin (h), in device
// pixels, with the content placed under the title bar. The returned image
// carries the content's devicePixelRatio.
QImage decorate(const QImage &content, const QString &title, const Style &style);

// Redact a rectangle (device pixels) in place: heavy downscale/upscale so
// text inside is unreadable while the area keeps its overall color and
// shape. The rectangle is clipped to the image.
void blurRect(QImage &image, const QRect &rect);

} // namespace DocCaptureFrame

#endif // DOC_CAPTURE_FRAME_H
