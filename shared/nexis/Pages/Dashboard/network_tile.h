#ifndef NETWORK_TILE_H
#define NETWORK_TILE_H

#include <QWidget>
#include <QLabel>
#include <QtCharts>
#include <Utils/format_util.h>

class NetworkTile : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkTile(const QColor &color, QWidget *parent = nullptr);
    ~NetworkTile() = default;

    void setValues(quint64 rxDelta, quint64 txDelta, quint64 rxTotal, quint64 txTotal);
    void setQuickAction(const QString &text, std::function<void()> callback);

private:
    void buildLayout();

    QColor mColor;

    QLabel *mLblTitle;
    QLabel *mLblDownValue;
    QLabel *mLblUpValue;
    QChartView *mChartView;
    QLineSeries *mRxSeries;
    QLineSeries *mTxSeries;
    QChart *mChart;
    QLabel *mLblFooter;
    QPushButton *mBtnAction;

    QValueAxis *mAxisY;

    static const int SPARKLINE_SIZE = 60;
    QList<double> mRxBuffer;
    QList<double> mTxBuffer;
    double mMaxSeen;
};

#endif // NETWORK_TILE_H
