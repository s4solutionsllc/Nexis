#ifndef POWER_DRAW_WIDGET_H
#define POWER_DRAW_WIDGET_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <Info/rapl_power_snapshot.h>

// SSO-15378: package power draw via Linux powercap/RAPL. Hides itself
// entirely on hosts without any RAPL zones (VMs, non-x86, unsupported CPUs)
// so the panel only ever appears when there is something to show — mirrors
// OomKillsWidget's availability-gated card pattern.
class PowerDrawWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PowerDrawWidget(QWidget *parent = nullptr);

    // DS §2/§3: opt this card into the shared elevated-card + accent-bar
    // header recipe (mirrors OomKillsWidget::setElevated()).
    void setElevated(const QString &accentToken);

public slots:
    void onPowerUpdated(const RaplPowerSnapshot &snap);

private slots:
    void refreshThemeColors();

private:
    void buildUI();

    QFrame      *mCard       = nullptr;
    QFrame      *mAccentBar  = nullptr;
    QLabel      *mTitle      = nullptr;
    QLabel      *mWattsLabel = nullptr;
    QLabel      *mBreakdown  = nullptr;
};

#endif // POWER_DRAW_WIDGET_H
