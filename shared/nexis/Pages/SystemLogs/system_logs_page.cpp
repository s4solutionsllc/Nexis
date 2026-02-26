#include "system_logs_page.h"
#include "log_provider.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTimer>

#include "Managers/app_manager.h"
#include "signal_mapper.h"

SystemLogsPage::SystemLogsPage(QWidget *parent)
    : QWidget(parent)
    , mProvider(LogProvider::createForPlatform(this))
    , mModel(nullptr)
    , mProxy(nullptr)
    , mTableView(nullptr)
    , mCmbSeverity(nullptr)
    , mSearchField(nullptr)
    , mBtnRefresh(nullptr)
    , mLblStatus(nullptr)
    , mSeverityFilter(7)
{
    connect(mProvider, &LogProvider::logsReady, this, &SystemLogsPage::onLogsReady);
    connect(mProvider, &LogProvider::errorOccurred, this, &SystemLogsPage::onError);

    buildLayout();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &SystemLogsPage::refreshThemeColors);
    refreshThemeColors();

    QTimer::singleShot(100, this, &SystemLogsPage::onRefreshClicked);
}

SystemLogsPage::~SystemLogsPage()
{
    mProvider->cancel();
}

void SystemLogsPage::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(8);

    // Filter toolbar
    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    mCmbSeverity = new QComboBox(this);
    mCmbSeverity->addItems({
        tr("All Severities"),
        tr("Error && Above"),
        tr("Warning && Above"),
        tr("Info && Above")
    });
    mCmbSeverity->setFixedWidth(160);
    mCmbSeverity->setObjectName("cmbLogSeverity");
    connect(mCmbSeverity, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SystemLogsPage::onSeverityFilterChanged);
    filterLayout->addWidget(mCmbSeverity);

    mSearchField = new QLineEdit(this);
    mSearchField->setPlaceholderText(tr("Search logs..."));
    mSearchField->setObjectName("searchLogField");
    connect(mSearchField, &QLineEdit::textChanged,
            this, &SystemLogsPage::onSearchTextChanged);
    filterLayout->addWidget(mSearchField);

    mBtnRefresh = new QToolButton(this);
    mBtnRefresh->setObjectName("btnLogRefresh");
    mBtnRefresh->setText(QString::fromUtf8("\u21BB"));
    mBtnRefresh->setToolTip(tr("Refresh logs"));
    mBtnRefresh->setFixedSize(32, 32);
    mBtnRefresh->setAutoRaise(true);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    connect(mBtnRefresh, &QToolButton::clicked,
            this, &SystemLogsPage::onRefreshClicked);
    filterLayout->addWidget(mBtnRefresh);

    mainLayout->addLayout(filterLayout);

    // Table view
    mModel = new QStandardItemModel(0, 4, this);
    mModel->setHorizontalHeaderLabels({
        tr("Timestamp"), tr("Severity"), tr("Unit"), tr("Message")
    });

    mProxy = new QSortFilterProxyModel(this);
    mProxy->setSourceModel(mModel);
    mProxy->setFilterKeyColumn(-1);
    mProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    mTableView = new QTableView(this);
    mTableView->setObjectName("logTableView");
    mTableView->setModel(mProxy);
    mTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    mTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTableView->verticalHeader()->hide();
    mTableView->horizontalHeader()->setStretchLastSection(true);
    mTableView->setAlternatingRowColors(true);
    mTableView->setSortingEnabled(true);
    mTableView->setColumnWidth(0, 170);
    mTableView->setColumnWidth(1, 70);
    mTableView->setColumnWidth(2, 150);

    mainLayout->addWidget(mTableView, 1);

    // Status bar
    auto *statusLayout = new QHBoxLayout();
    mLblStatus = new QLabel(tr("Ready"), this);
    mLblStatus->setObjectName("logStatusLabel");
    statusLayout->addWidget(mLblStatus);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);
}

void SystemLogsPage::onRefreshClicked()
{
    if (mProvider->isBusy())
        return;

    mBtnRefresh->setEnabled(false);
    mLblStatus->setText(tr("Loading logs..."));
    mProvider->fetchLogs(500);
}

void SystemLogsPage::onLogsReady(const QList<LogEntry> &entries)
{
    mCachedEntries = entries;
    applyFilters();
    mBtnRefresh->setEnabled(true);
}

void SystemLogsPage::onError(const QString &message)
{
    mLblStatus->setText(tr("Error: %1").arg(message));
    mBtnRefresh->setEnabled(true);
}

void SystemLogsPage::onSeverityFilterChanged(int index)
{
    static const int severityMap[] = { 7, 3, 4, 6 };
    mSeverityFilter = severityMap[index];
    applyFilters();
}

void SystemLogsPage::onSearchTextChanged(const QString &text)
{
    mProxy->setFilterFixedString(text);
}

void SystemLogsPage::applyFilters()
{
    QList<LogEntry> filtered;
    filtered.reserve(mCachedEntries.size());
    for (const LogEntry &entry : mCachedEntries) {
        if (entry.severity <= mSeverityFilter)
            filtered.append(entry);
    }
    populateModel(filtered);
    mLblStatus->setText(tr("Showing %1 entries").arg(mProxy->rowCount()));
}

void SystemLogsPage::populateModel(const QList<LogEntry> &entries)
{
    mModel->removeRows(0, mModel->rowCount());

    QSettings *sv = AppManager::ins()->getStyleValues();

    QColor errorColor(sv ? sv->value("@destructiveColor").toString() : "#E05454");
    QColor warningColor(sv ? sv->value("@warningColor").toString() : "#FFB347");
    QColor noticeColor(sv ? sv->value("@infoColor").toString() : "#5B9BD5");

    for (const LogEntry &entry : entries) {
        auto *timestampItem = new QStandardItem(
            entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
        timestampItem->setData(entry.timestamp, Qt::UserRole);

        auto *severityItem = new QStandardItem(
            LogEntry::severityString(entry.severity));
        severityItem->setData(entry.severity, Qt::UserRole);
        if (entry.severity <= 3) {
            severityItem->setForeground(errorColor);
        } else if (entry.severity == 4) {
            severityItem->setForeground(warningColor);
        } else if (entry.severity == 5) {
            severityItem->setForeground(noticeColor);
        }

        auto *unitItem = new QStandardItem(entry.unit);
        auto *messageItem = new QStandardItem(entry.message);

        mModel->appendRow({ timestampItem, severityItem, unitItem, messageItem });
    }
}

void SystemLogsPage::refreshThemeColors()
{
    if (!mCachedEntries.isEmpty())
        applyFilters();

    update();
}
