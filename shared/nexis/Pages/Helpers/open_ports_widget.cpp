#include "open_ports_widget.h"

#include <Utils/command_util.h>
#include <Managers/app_manager.h>
#include "signal_mapper.h"

#include <QBoxLayout>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QThreadPool>

// ---------------------------------------------------------------------------
// Static parsers
// ---------------------------------------------------------------------------

QList<ConnectionEntry> OpenPortsWidget::parseLsofOutput(const QString &output)
{
    QList<ConnectionEntry> entries;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return entries;

    for (int i = 1; i < lines.size(); ++i) {
        const QString &line = lines[i];
        QStringList fields = line.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
        if (fields.size() < 9)
            continue;

        ConnectionEntry e;
        e.processName = fields[0];
        e.pid = fields[1].toInt();
        e.protocol = fields[7];

        QString name = fields[8];
        for (int f = 9; f < fields.size(); ++f)
            name += " " + fields[f];

        static const QRegularExpression reState(R"(\((\w[\w-]*)\)\s*$)");
        QRegularExpressionMatch sm = reState.match(name);
        if (sm.hasMatch())
            e.state = sm.captured(1);

        static const QRegularExpression reArrow(R"(->)");
        if (name.contains(reArrow)) {
            static const QRegularExpression reEstab(
                R"((.+):(\d+)->(.+):(\d+))");
            QRegularExpressionMatch m = reEstab.match(name);
            if (m.hasMatch()) {
                e.localAddress = m.captured(1);
                e.localPort = m.captured(2).toInt();
                e.remoteAddress = m.captured(3);
                e.remotePort = m.captured(4).toInt();
            }
        } else {
            static const QRegularExpression reListen(R"((.+):(\d+))");
            QRegularExpressionMatch m = reListen.match(name);
            if (m.hasMatch()) {
                e.localAddress = m.captured(1);
                e.localPort = m.captured(2).toInt();
                e.remoteAddress = "*";
                e.remotePort = 0;
            }
        }

        entries << e;
    }
    return entries;
}

QList<ConnectionEntry> OpenPortsWidget::parseSsOutput(const QString &output, const QString &protocol)
{
    QList<ConnectionEntry> entries;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return entries;

    for (int i = 1; i < lines.size(); ++i) {
        const QString &line = lines[i];
        QStringList fields = line.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
        if (fields.size() < 5)
            continue;

        ConnectionEntry e;
        e.protocol = protocol;
        e.state = fields[0];

        auto parseAddrPort = [](const QString &raw, QString &addr, int &port) {
            QString s = raw;
            int pctIdx = s.indexOf('%');
            if (pctIdx != -1) {
                int colonAfter = s.indexOf(':', pctIdx);
                if (colonAfter != -1)
                    s = s.left(pctIdx) + s.mid(colonAfter);
                else
                    s.remove(pctIdx, s.length() - pctIdx);
            }

            int lastColon = s.lastIndexOf(':');
            if (lastColon == -1) {
                addr = s;
                port = 0;
                return;
            }
            addr = s.left(lastColon);
            QString portStr = s.mid(lastColon + 1);
            if (portStr == "*")
                port = 0;
            else
                port = portStr.toInt();
        };

        parseAddrPort(fields[3], e.localAddress, e.localPort);
        parseAddrPort(fields[4], e.remoteAddress, e.remotePort);

        if (e.remoteAddress == "0.0.0.0" && e.remotePort == 0)
            e.remoteAddress = "*";
        if (e.remoteAddress == "[::]" && e.remotePort == 0)
            e.remoteAddress = "*";
        if (e.remoteAddress == "*" && e.remotePort != 0)
            e.remotePort = 0;

        static const QRegularExpression reProc(R"re("([\w][\w.-]*)",pid=(\d+))re");
        QString rest;
        for (int f = 5; f < fields.size(); ++f)
            rest += fields[f];
        QRegularExpressionMatch pm = reProc.match(rest);
        if (pm.hasMatch()) {
            e.processName = pm.captured(1);
            e.pid = pm.captured(2).toInt();
        }

        entries << e;
    }
    return entries;
}

// ---------------------------------------------------------------------------
// Data fetching (runs on worker thread)
// ---------------------------------------------------------------------------

QList<ConnectionEntry> OpenPortsWidget::fetchConnections(bool listenOnly)
{
#ifdef Q_OS_MACOS
    QStringList args = {"-iTCP", "-P", "-n"};
    if (listenOnly)
        args << "-sTCP:LISTEN";
    ExecResult r = CommandUtil::execWithStatus("lsof", args, 10000);
    if (r.exitCode == 0 || !r.output.isEmpty())
        return parseLsofOutput(r.output);
#else
    QStringList args = {"-tnp"};
    if (listenOnly)
        args = {"-tlnp"};
    if (CommandUtil::isExecutable("ss")) {
        ExecResult r = CommandUtil::execWithStatus("ss", args, 10000);
        if (r.exitCode == 0 || !r.output.isEmpty())
            return parseSsOutput(r.output, "TCP");
    }
    QStringList netArgs = {"-tnp"};
    if (listenOnly)
        netArgs = {"-tlnp"};
    ExecResult r = CommandUtil::execWithStatus("netstat", netArgs, 10000);
    if (r.exitCode == 0 || !r.output.isEmpty())
        return parseSsOutput(r.output, "TCP");
#endif
    return {};
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

OpenPortsWidget::OpenPortsWidget(QWidget *parent)
    : QWidget(parent),
      mModel(new QStandardItemModel(this)),
      mProxy(new QSortFilterProxyModel(this))
{
    buildUI();
    refreshThemeColors();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &OpenPortsWidget::refreshThemeColors);
    connect(this, &OpenPortsWidget::connectionsFetched,
            this, &OpenPortsWidget::onConnectionsFetched);
}

void OpenPortsWidget::buildUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("Open Ports & Connections"));
    mLblTitle->setObjectName("portsTitle");
    root->addWidget(mLblTitle);

    QHBoxLayout *filterBar = new QHBoxLayout;
    filterBar->setSpacing(8);

    mTxtSearch = new QLineEdit;
    mTxtSearch->setPlaceholderText(tr("Filter by process, address, or port..."));
    mTxtSearch->setClearButtonEnabled(true);
    mTxtSearch->setObjectName("portsSearch");
    connect(mTxtSearch, &QLineEdit::textChanged, this, &OpenPortsWidget::onFilterChanged);
    filterBar->addWidget(mTxtSearch, 1);

    mBtnListenOnly = new QPushButton(tr("Listening Only"));
    mBtnListenOnly->setCheckable(true);
    mBtnListenOnly->setChecked(true);
    mBtnListenOnly->setCursor(Qt::PointingHandCursor);
    mBtnListenOnly->setFocusPolicy(Qt::NoFocus);
    mBtnListenOnly->setObjectName("portsListenToggle");
    connect(mBtnListenOnly, &QPushButton::toggled, this, &OpenPortsWidget::onListenOnlyToggled);
    filterBar->addWidget(mBtnListenOnly);

    mBtnRefresh = new QPushButton(tr("Refresh"));
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    mBtnRefresh->setFocusPolicy(Qt::NoFocus);
    mBtnRefresh->setObjectName("portsRefresh");
    connect(mBtnRefresh, &QPushButton::clicked, this, &OpenPortsWidget::refresh);
    filterBar->addWidget(mBtnRefresh);

    root->addLayout(filterBar);

    QStringList headers = {
        tr("Protocol"), tr("Local Address"), tr("Port"),
        tr("Remote Address"), tr("Remote Port"),
        tr("PID"), tr("Process"), tr("State")
    };
    mModel->setHorizontalHeaderLabels(headers);

    mProxy->setSourceModel(mModel);
    mProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    mProxy->setFilterKeyColumn(-1);

    mTable = new QTableView;
    mTable->setModel(mProxy);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->verticalHeader()->setVisible(false);
    mTable->horizontalHeader()->setStretchLastSection(true);
    mTable->horizontalHeader()->setSectionsClickable(true);
    mTable->horizontalHeader()->setSortIndicatorShown(true);
    mTable->setSortingEnabled(true);
    mTable->setObjectName("portsTable");
    root->addWidget(mTable, 1);

    QHBoxLayout *footer = new QHBoxLayout;
    mLblCount = new QLabel;
    mLblCount->setObjectName("portsSecondary");
    footer->addWidget(mLblCount);
    footer->addStretch();
    root->addLayout(footer);

    mLblLoading = new QLabel(tr("Loading connections..."));
    mLblLoading->setObjectName("portsSecondary");
    mLblLoading->setAlignment(Qt::AlignCenter);
    mLblLoading->hide();
    root->addWidget(mLblLoading);
}

// ---------------------------------------------------------------------------
// Load / refresh
// ---------------------------------------------------------------------------

void OpenPortsWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void OpenPortsWidget::refresh()
{
    mLoaded = true;
    mBtnRefresh->setEnabled(false);
    mLblLoading->show();

    bool listenOnly = mBtnListenOnly->isChecked();

    QThreadPool::globalInstance()->start([this, listenOnly]() {
        QList<ConnectionEntry> entries = fetchConnections(listenOnly);
        emit connectionsFetched(entries);
    });
}

void OpenPortsWidget::onConnectionsFetched(QList<ConnectionEntry> entries)
{
    mLblLoading->hide();
    mBtnRefresh->setEnabled(true);

    mModel->blockSignals(true);
    mModel->removeRows(0, mModel->rowCount());

    QSettings *sv = AppManager::ins()->getStyleValues();
    QString successColor = sv ? sv->value("@successColor").toString() : "#2ec27e";
    QString warningColor = sv ? sv->value("@warningColor").toString() : "#FFB347";
    QString failColor    = sv ? sv->value("@destructiveColor").toString() : "#E05454";

    for (const ConnectionEntry &e : entries) {
        QList<QStandardItem *> row;

        row << new QStandardItem(e.protocol);
        row << new QStandardItem(e.localAddress);

        auto *portItem = new QStandardItem(QString::number(e.localPort));
        portItem->setData(e.localPort, Qt::UserRole);
        row << portItem;

        row << new QStandardItem(e.remoteAddress);

        auto *rPortItem = new QStandardItem(e.remotePort > 0 ? QString::number(e.remotePort) : "—");
        rPortItem->setData(e.remotePort, Qt::UserRole);
        row << rPortItem;

        row << new QStandardItem(e.pid >= 0 ? QString::number(e.pid) : "—");
        row << new QStandardItem(e.processName);

        auto *stateItem = new QStandardItem(e.state);
        if (e.state == "LISTEN")
            stateItem->setForeground(QColor(successColor));
        else if (e.state == "ESTABLISHED" || e.state == "ESTAB")
            stateItem->setForeground(QColor(warningColor));
        else if (e.state == "CLOSE_WAIT" || e.state == "CLOSE-WAIT" ||
                 e.state == "TIME_WAIT" || e.state == "TIME-WAIT")
            stateItem->setForeground(QColor(failColor));
        row << stateItem;

        for (auto *item : row)
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        mModel->appendRow(row);
    }

    mModel->blockSignals(false);
    mModel->layoutChanged();

    mTable->resizeColumnsToContents();

    int visible = mProxy->rowCount();
    mLblCount->setText(tr("%n connection(s)", "", visible));
}

// ---------------------------------------------------------------------------
// Filter / toggle
// ---------------------------------------------------------------------------

void OpenPortsWidget::onFilterChanged(const QString &text)
{
    mProxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
    int visible = mProxy->rowCount();
    mLblCount->setText(tr("%n connection(s)", "", visible));
}

void OpenPortsWidget::onListenOnlyToggled(bool /*checked*/)
{
    if (mLoaded)
        refresh();
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void OpenPortsWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString cardBg   = sv ? sv->value("@cardBg").toString() : "#2A2C32";
    QString textPri  = sv ? sv->value("@color05").toString() : "#F0F2F5";
    QString textSec  = sv ? sv->value("@color04").toString() : "#9A9DA6";
    QString border   = sv ? sv->value("@borderColor").toString() : "#4A4D5A";
    QString accent   = sv ? sv->value("@accentColor").toString() : "#FF6B1A";

    if (mLblTitle)
        mLblTitle->setStyleSheet(
            QString("color: %1; font-size: 16px; font-weight: bold;").arg(textPri));

    if (mTxtSearch)
        mTxtSearch->setStyleSheet(QString(
            "QLineEdit#portsSearch {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "  padding: 6px 10px;"
            "  font-size: 12px;"
            "}"
            "QLineEdit#portsSearch:focus {"
            "  border-color: %4;"
            "}").arg(cardBg, textPri, border, accent));

    if (mBtnListenOnly)
        mBtnListenOnly->setStyleSheet(QString(
            "QPushButton#portsListenToggle {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "  padding: 6px 14px;"
            "  font-size: 12px;"
            "}"
            "QPushButton#portsListenToggle:checked {"
            "  background-color: %4;"
            "  color: #ffffff;"
            "  border-color: %4;"
            "}"
            "QPushButton#portsListenToggle:hover:!checked {"
            "  border-color: %4;"
            "}").arg(cardBg, textSec, border, accent));

    if (mBtnRefresh)
        mBtnRefresh->setStyleSheet(QString(
            "QPushButton#portsRefresh {"
            "  background-color: %1;"
            "  color: #ffffff;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 6px 16px;"
            "  font-size: 12px;"
            "}"
            "QPushButton#portsRefresh:hover {"
            "  opacity: 0.9;"
            "}"
            "QPushButton#portsRefresh:disabled {"
            "  background-color: %2;"
            "  color: %3;"
            "}").arg(accent, cardBg, textSec));

    if (mTable) {
        mTable->setStyleSheet(QString(
            "QTableView#portsTable {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 6px;"
            "  gridline-color: %3;"
            "  font-size: 12px;"
            "}"
            "QTableView#portsTable::item {"
            "  padding: 4px 8px;"
            "}"
            "QTableView#portsTable::item:selected {"
            "  background-color: %4;"
            "  color: #ffffff;"
            "}"
            "QHeaderView::section {"
            "  background-color: %1;"
            "  color: %5;"
            "  border: none;"
            "  border-bottom: 1px solid %3;"
            "  padding: 6px 8px;"
            "  font-size: 12px;"
            "  font-weight: bold;"
            "}").arg(cardBg, textPri, border, accent, textSec));
    }

    for (QLabel *lbl : findChildren<QLabel *>("portsSecondary"))
        lbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(textSec));
}
