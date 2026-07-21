#include "system_logs_page.h"
#include "log_provider.h"
#include "severity_pill_delegate.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTimer>
#include <QFrame>

#include "signal_mapper.h"
#include "utilities.h"

SystemLogsPage::SystemLogsPage(QWidget *parent)
    : QWidget(parent)
    , mProvider(LogProvider::createForPlatform(this))
    , mModel(nullptr)
    , mProxy(nullptr)
    , mLogsContainer(nullptr)
    , mTableView(nullptr)
    , mCmbSeverity(nullptr)
    , mSearchField(nullptr)
    , mBtnRefresh(nullptr)
    , mLblStatus(nullptr)
    , mLblTitle(nullptr)
    , mLblSource(nullptr)
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

    // Toolbar header row (DS \u00A73, SSO-14314): accent bar + "System Logs" /
    // "Live log stream" title block, structurally identical to the Processes/
    // Boot Analysis .page-header recipe, followed by the existing filter
    // controls (style.qss "Section Header" recipe).
    auto *headerRow = new QWidget(this);
    headerRow->setObjectName("sectionHeaderRow");
    auto *filterLayout = new QHBoxLayout(headerRow);
    filterLayout->setContentsMargins(14, 12, 14, 10);
    filterLayout->setSpacing(8);

    auto *accentBar = new QFrame(headerRow);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("accentToken", "accent");
    accentBar->setFrameShape(QFrame::NoFrame);
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(26);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    filterLayout->addWidget(accentBar);

    auto *headerTextCol = new QVBoxLayout;
    headerTextCol->setContentsMargins(0, 0, 0, 0);
    headerTextCol->setSpacing(0);

    mLblTitle = new QLabel(tr("System Logs"), headerRow);
    mLblTitle->setObjectName("sectionHeaderTitle");
    headerTextCol->addWidget(mLblTitle);

    mLblSource = new QLabel(tr("Live log stream"), headerRow);
    mLblSource->setObjectName("sectionHeaderSource");
    headerTextCol->addWidget(mLblSource);

    filterLayout->addLayout(headerTextCol);

    mCmbSeverity = new QComboBox(headerRow);
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

    mSearchField = new QLineEdit(headerRow);
    mSearchField->setPlaceholderText(tr("Search logs..."));
    mSearchField->setObjectName("searchLogField");
    connect(mSearchField, &QLineEdit::textChanged,
            this, &SystemLogsPage::onSearchTextChanged);
    filterLayout->addWidget(mSearchField);

    mBtnRefresh = new QToolButton(headerRow);
    mBtnRefresh->setObjectName("btnLogRefresh");
    mBtnRefresh->setText(QString::fromUtf8("\u21BB"));
    mBtnRefresh->setToolTip(tr("Refresh logs"));
    mBtnRefresh->setFixedSize(24, 24);   // DS \u00A73 action-button convention (metric_tile_base.cpp:171-180)
    mBtnRefresh->setAutoRaise(true);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    connect(mBtnRefresh, &QToolButton::clicked,
            this, &SystemLogsPage::onRefreshClicked);
    filterLayout->addWidget(mBtnRefresh);

    mainLayout->addWidget(headerRow);

    // Elevated container (DS \u00A72): shadow lives on the container only, the
    // log rows inside stay flat (DS \u00A77).
    mLogsContainer = new QWidget(this);
    mLogsContainer->setObjectName("logsContainer");
    mLogsContainer->setAttribute(Qt::WA_StyledBackground, true);
    mLogsContainer->setProperty("cardRole", "elevated");
    auto *containerLayout = new QVBoxLayout(mLogsContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // Table view
    mModel = new QStandardItemModel(0, 4, this);
    mModel->setHorizontalHeaderLabels({
        tr("Timestamp"), tr("Severity"), tr("Unit"), tr("Message")
    });

    mProxy = new QSortFilterProxyModel(this);
    mProxy->setSourceModel(mModel);
    mProxy->setFilterKeyColumn(-1);
    mProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    mTableView = new QTableView(mLogsContainer);
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
    mTableView->setItemDelegateForColumn(1, new SeverityPillDelegate(mTableView));

    containerLayout->addWidget(mTableView);
    mainLayout->addWidget(mLogsContainer, 1);
    Utilities::addDropShadow(mLogsContainer, 90, 26);

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
    mProvider->fetchLogs(500, mSeverityFilter);
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

    if (!mProvider->isBusy()) {
        mBtnRefresh->setEnabled(false);
        mLblStatus->setText(tr("Loading logs..."));
        mProvider->fetchLogs(500, mSeverityFilter);
    } else {
        applyFilters();
    }
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

    for (const LogEntry &entry : entries) {
        auto *timestampItem = new QStandardItem(
            entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
        timestampItem->setData(entry.timestamp, Qt::UserRole);

        // Text stays on the item (search still filters on it); SeverityPillDelegate
        // (DS §5) paints the pill badge and picks the status color at paint time.
        auto *severityItem = new QStandardItem(
            LogEntry::severityString(entry.severity));
        severityItem->setData(entry.severity, Qt::UserRole);

        auto *unitItem = new QStandardItem(
            entry.unit.isEmpty() ? QString::fromUtf8("—") : entry.unit);
        auto *messageItem = new QStandardItem(entry.message);

        mModel->appendRow({ timestampItem, severityItem, unitItem, messageItem });
    }
}

void SystemLogsPage::refreshThemeColors()
{
    // Severity pill colors are resolved live at paint time (SeverityPillDelegate);
    // a viewport repaint is enough to pick up a theme change.
    if (mTableView)
        mTableView->viewport()->update();

    update();
}
