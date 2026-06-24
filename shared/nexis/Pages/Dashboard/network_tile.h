#ifndef NETWORK_TILE_H
#define NETWORK_TILE_H

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QtCharts>
#include <Utils/format_util.h>

class NetworkTile : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkTile(const QString &colorToken, QWidget *parent = nullptr);
    ~NetworkTile() = default;

    void setValues(quint64 rxDelta, quint64 txDelta, quint64 rxTotal, quint64 txTotal);
    void setQuickAction(const QString &text, std::function<void()> callback);
    void setInterfaceName(const QString &name);
    void setColorOverride(const QString &hexColor);
    void setCompact(bool compact);
    QString colorOverride() const { return mColorOverride; }

private:
    void buildLayout();
    void refreshThemeColors();

    QString mColorToken;
    QString mColorOverride;

    QLabel *mLblTitle;
    QLabel *mLblDownLabel;
    QLabel *mLblDownValue;
    QLabel *mLblUpLabel;
    QLabel *mLblUpValue;
    QChartView *mRxChartView;
    QChartView *mTxChartView;
    QLineSeries *mRxSeries;
    QLineSeries *mTxSeries;
    QAreaSeries *mRxAreaSeries;
    QAreaSeries *mTxAreaSeries;
    QChart *mRxChart;
    QChart *mTxChart;
    QFrame *mDivider;
    QLabel *mLblFooter;
    QLabel *mLblInterface;
    QPushButton *mBtnAction;

    QValueAxis *mRxAxisY;
    QValueAxis *mTxAxisY;

    static const int SPARKLINE_SIZE = 60;
    QList<double> mRxBuffer;
    QList<double> mTxBuffer;
    double mMaxSeen;
};

#endif // NETWORK_TILE_H
