#ifndef DISK_ANALYZER_WIDGET_H
#define DISK_ANALYZER_WIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QtCharts>
#include <QFuture>

#include "Managers/info_manager.h"
#include "Info/disk_usage_worker.h"
#include "signal_mapper.h"

// ─── Custom sort for tree items (sort by raw bytes / percentage) ───

class DiskTreeItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        int col = treeWidget()->sortColumn();
        if (col == 1) {
            // Sort by raw byte count
            return data(1, Qt::UserRole).toULongLong()
                 < other.data(1, Qt::UserRole).toULongLong();
        }
        if (col == 2) {
            // Sort by percentage
            return data(2, Qt::UserRole).toDouble()
                 < other.data(2, Qt::UserRole).toDouble();
        }
        return text(col).toLower() < other.text(col).toLower();
    }
};

// ─── Percentage bar delegate for column 2 ──────────────────────────

class PercentBarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Draw default background / selection
        QStyledItemDelegate::paint(painter, option, index);

        double pct = index.data(Qt::UserRole).toDouble();
        if (pct <= 0) return;

        painter->save();

        QRect barRect = option.rect.adjusted(4, 4, -4, -4);
        int fillWidth = static_cast<int>(barRect.width() * pct / 100.0);

        // Bar background
        painter->fillRect(barRect, QColor(60, 60, 60, 40));
        // Bar fill (green from Stacer palette)
        painter->fillRect(QRect(barRect.x(), barRect.y(), fillWidth, barRect.height()),
                          QColor(0x2ec27e));

        // Text overlay
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(barRect, Qt::AlignCenter,
                          QString::asprintf("%.1f%%", pct));

        painter->restore();
    }
};

// ─── Main widget ───────────────────────────────────────────────────

class DiskAnalyzerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiskAnalyzerWidget(const QList<int> &chartColors, QWidget *parent = nullptr);
    ~DiskAnalyzerWidget();

private slots:
    void onDeviceChanged(int index);
    void onAnalyzeClicked();
    void onNavigateUp();
    void onScanFinished(const QString &path, const QList<DirEntry> &entries, quint64 totalSize);
    void onScanProgress(const QString &currentDir, int itemsScanned);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onThemeChanged();

private:
    void buildUi();
    void populateDeviceCombo();
    void updatePieChart(const QList<DirEntry> &entries, quint64 deviceTotal, quint64 deviceFree);
    void populateTree(const QList<DirEntry> &entries);
    void drillDown(const QString &path);
    void updateBreadcrumb();

private:
    InfoManager *mInfoManager;
    DiskUsageWorker *mWorker;
    QFuture<void> mScanFuture;

    // Current state
    QString mCurrentPath;       // Path currently displayed
    QString mMountPoint;        // Root mount point of selected device
    quint64 mDeviceTotal;       // Total bytes of selected device
    quint64 mDeviceFree;        // Free bytes of selected device

    // Toolbar
    QComboBox *mDeviceCombo;
    QPushButton *mAnalyzeBtn;
    QPushButton *mUpBtn;
    QLabel *mBreadcrumb;

    // Pie chart (left panel)
    QChart *mPieChart;
    QChartView *mPieChartView;
    QPieSeries *mPieSeries;

    // Tree (right panel)
    QTreeWidget *mTree;

    // Status bar
    QLabel *mStatusLabel;
    QProgressBar *mProgressBar;

    // Colors (shared with ResourcesPage)
    QList<int> mChartColors;

    // Filesystem types to exclude (virtual/pseudo filesystems)
    static const QStringList sExcludedFsTypes;
};

#endif // DISK_ANALYZER_WIDGET_H
