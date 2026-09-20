#ifndef SYSTEM_LOGS_PAGE_H
#define SYSTEM_LOGS_PAGE_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>

class LogProvider;
struct LogEntry;

class SystemLogsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SystemLogsPage(QWidget *parent = nullptr);
    ~SystemLogsPage();

    // Test-only: aborts any in-flight fetch, severs the provider so a
    // late-arriving result can't race back in, and clears the table to a
    // deterministic empty state. The live log stream can surface real host
    // data (hostnames, process/network internals) that must never be baked
    // into a committed screenshot baseline (SSO-24820).
    void resetForScreenshotTest();

private slots:
    void onRefreshClicked();
    void onLogsReady(const QList<LogEntry> &entries);
    void onError(const QString &message);
    void onSeverityFilterChanged(int index);
    void onSearchTextChanged(const QString &text);
    void refreshThemeColors();

private:
    void buildLayout();
    void applyFilters();
    void populateModel(const QList<LogEntry> &entries);

    LogProvider *mProvider;
    QStandardItemModel *mModel;
    QSortFilterProxyModel *mProxy;
    QWidget *mLogsContainer;

    QTableView *mTableView;
    QComboBox *mCmbSeverity;
    QLineEdit *mSearchField;
    QToolButton *mBtnRefresh;
    QLabel *mLblStatus;
    QLabel *mLblTitle;
    QLabel *mLblSource;

    QList<LogEntry> mCachedEntries;
    int mSeverityFilter;   // max severity to show (7=all, 3=Error+, 4=Warning+, 6=Info+)
};

#endif // SYSTEM_LOGS_PAGE_H
