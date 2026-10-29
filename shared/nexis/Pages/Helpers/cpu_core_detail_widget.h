#ifndef CPU_CORE_DETAIL_WIDGET_H
#define CPU_CORE_DETAIL_WIDGET_H

#include <QFrame>
#include <QLabel>
#include <QList>
#include <QListView>
#include <QVBoxLayout>
#include <QWidget>

class CpuCoreListModel;
class CpuCoreItemDelegate;

// SSO-15378: compact per-core utilization/frequency detail. Leapfrogs the
// one-row-widget-per-core pattern (see CpuTuningWidget::buildPerCoreGrid())
// by backing a QListView with a QAbstractListModel — the delegate paints
// only the rows the viewport can actually show, so this stays legible and
// responsive whether the host has 4 cores or 128.
//
// SSO-26719: no longer its own Resources card — embedded as the CPU card's
// "per-core" toggle view (HistoryChart::addBodyPage()), so its own title/
// accent-bar chrome is dropped; the CPU card's own header already names it.
class CpuCoreDetailWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CpuCoreDetailWidget(QWidget *parent = nullptr);

public slots:
    void onCpuUpdated(const QList<int> &percents, const QList<double> &clocks);

private:
    void buildUI();

    QListView    *mListView  = nullptr;
    CpuCoreListModel     *mModel    = nullptr;
    CpuCoreItemDelegate  *mDelegate = nullptr;
};

#endif // CPU_CORE_DETAIL_WIDGET_H
