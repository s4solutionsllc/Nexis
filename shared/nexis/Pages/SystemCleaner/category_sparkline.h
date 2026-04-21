#ifndef CATEGORY_SPARKLINE_H
#define CATEGORY_SPARKLINE_H

#include <QColor>
#include <QList>
#include <QWidget>

// FR-114: tiny sparkline for the scan-size trend shown under each
// System Cleaner category. Flat QPainter polyline — no QChart overhead.
// Auto-scales to the max value in the window.
class CategorySparkline : public QWidget
{
    Q_OBJECT

public:
    explicit CategorySparkline(QWidget *parent = nullptr);

    void setSamples(const QList<quint64> &samples);
    void setLineColor(const QColor &color);
    QSize sizeHint() const override { return QSize(60, 16); }
    QSize minimumSizeHint() const override { return QSize(40, 12); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<quint64> mSamples;
    QColor mLineColor;
};

#endif // CATEGORY_SPARKLINE_H
