#include "open_ports_widget.h"

#include <Utils/command_util.h>
#include <Managers/app_manager.h>
#include <Managers/setting_manager.h>

#include <QBoxLayout>
#include <QFile>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QThreadPool>

#ifdef Q_OS_MAC
#include <libproc.h>
#endif

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

// FR-121: resolve the binary path that opened this PID.
QString OpenPortsWidget::resolveBinaryPath(int pid)
{
    if (pid <= 0)
        return QString();

#ifdef Q_OS_MAC
    char buf[PROC_PIDPATHINFO_MAXSIZE] = {0};
    int rc = proc_pidpath(pid, buf, sizeof(buf));
    if (rc <= 0)
        return QString();
    return QString::fromUtf8(buf, rc);
#else
    const QString link = QString("/proc/%1/exe").arg(pid);
    const QString target = QFile::symLinkTarget(link);
    return target;   // empty on EACCES / ENOENT — caller treats as "not assessed"
#endif
}

// FR-121: a path counts as trusted if it begins with any platform-default
// prefix OR any user-configured extra (TrustedBinderPrefixes setting).
bool OpenPortsWidget::isTrustedBinderPath(const QString &path)
{
    if (path.isEmpty())
        return true;   // unresolved → don't flag

    QStringList prefixes;
#ifdef Q_OS_MAC
    prefixes << "/usr/bin/"
             << "/usr/sbin/"
             << "/usr/libexec/"
             << "/System/"
             << "/Library/Apple/"
             << "/opt/homebrew/"
             << "/usr/local/bin/"
             << "/usr/local/sbin/"
             << "/Applications/";
#else
    prefixes << "/usr/bin/"
             << "/usr/sbin/"
             << "/bin/"
             << "/sbin/"
             << "/lib/"
             << "/lib64/"
             << "/usr/lib/"
             << "/usr/libexec/"
             << "/usr/local/bin/"
             << "/usr/local/sbin/"
             << "/snap/"
             << "/opt/";
#endif

    // User-configured extras — any non-empty string appended.
    const QJsonArray extras = QJsonDocument::fromJson(
        SettingManager::ins()->getTrustedBinderPrefixes().toUtf8()).array();
    for (const QJsonValue &v : extras) {
        const QString p = v.toString().trimmed();
        if (!p.isEmpty())
            prefixes << p;
    }

    for (const QString &prefix : prefixes) {
        if (path.startsWith(prefix))
            return true;
    }
    return false;
}

QList<ConnectionEntry> OpenPortsWidget::fetchConnections(bool listenOnly)
{
    QList<ConnectionEntry> entries;
#ifdef Q_OS_MACOS
    QStringList args = {"-iTCP", "-P", "-n"};
    if (listenOnly)
        args << "-sTCP:LISTEN";
    ExecResult r = CommandUtil::execWithStatus("lsof", args, 10000);
    if (r.ok() || !r.output.isEmpty())
        entries = parseLsofOutput(r.output);
#else
    QStringList args = {"-tnp"};
    if (listenOnly)
        args = {"-tlnp"};
    if (CommandUtil::isExecutable("ss")) {
        ExecResult r = CommandUtil::execWithStatus("ss", args, 10000);
        if (r.ok() || !r.output.isEmpty())
            entries = parseSsOutput(r.output, "TCP");
    }
    if (entries.isEmpty()) {
        QStringList netArgs = {"-tnp"};
        if (listenOnly)
            netArgs = {"-tlnp"};
        ExecResult r = CommandUtil::execWithStatus("netstat", netArgs, 10000);
        if (r.ok() || !r.output.isEmpty())
            entries = parseSsOutput(r.output, "TCP");
    }
#endif

    // FR-121: resolve binary path and flag unexpected binders.
    for (ConnectionEntry &e : entries) {
        if (e.pid <= 0)
            continue;
        e.binaryPath = resolveBinaryPath(e.pid);
        if (!e.binaryPath.isEmpty())
            e.isUnexpected = !isTrustedBinderPath(e.binaryPath);
    }

    return entries;
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
    mBtnListenOnly->setObjectName("portsListenToggle");
    connect(mBtnListenOnly, &QPushButton::toggled, this, &OpenPortsWidget::onListenOnlyToggled);
    filterBar->addWidget(mBtnListenOnly);

    mBtnRefresh = new QPushButton(tr("Refresh"));
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    mBtnRefresh->setObjectName("portsRefresh");
    connect(mBtnRefresh, &QPushButton::clicked, this, &OpenPortsWidget::refresh);
    filterBar->addWidget(mBtnRefresh);

#ifdef Q_OS_MAC
    // FR-121: lazy codesign check. Fork-per-row is expensive, so don't run
    // on every refresh — require an explicit click.
    mBtnVerifySigs = new QPushButton(tr("Verify Signatures"));
    mBtnVerifySigs->setCursor(Qt::PointingHandCursor);
    mBtnVerifySigs->setToolTip(
        tr("Run codesign on each binary to flag unsigned ones. "
           "Results are cached by path."));
    connect(mBtnVerifySigs, &QPushButton::clicked,
            this, &OpenPortsWidget::onVerifySignatures);
    filterBar->addWidget(mBtnVerifySigs);
#endif

    root->addLayout(filterBar);

    QStringList headers = {
        tr("Protocol"), tr("Local Address"), tr("Port"),
        tr("Remote Address"), tr("Remote Port"),
        tr("PID"), tr("Process"), tr("State"), tr("Path")   // FR-121
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

    mEntries = entries;   // cached for lazy Verify Signatures (FR-121).

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

        // FR-121: Process + Path columns carry the unexpected/unsigned flag
        // via theme-color foreground + tooltip. Warning glyph on the Process
        // cell for at-a-glance visibility.
        auto *procItem = new QStandardItem(e.processName);
        auto *stateItem = new QStandardItem(e.state);
        if (e.state == "LISTEN")
            stateItem->setForeground(QColor(successColor));
        else if (e.state == "ESTABLISHED" || e.state == "ESTAB")
            stateItem->setForeground(QColor(warningColor));
        else if (e.state == "CLOSE_WAIT" || e.state == "CLOSE-WAIT" ||
                 e.state == "TIME_WAIT" || e.state == "TIME-WAIT")
            stateItem->setForeground(QColor(failColor));

        auto *pathItem = new QStandardItem(
            e.binaryPath.isEmpty() ? QStringLiteral("—") : e.binaryPath);
        pathItem->setToolTip(e.binaryPath);

        if (e.isUnsigned) {
            procItem->setText(QStringLiteral("⚠ %1").arg(e.processName));
            procItem->setForeground(QColor(failColor));
            procItem->setToolTip(tr("Unsigned binary."));
            pathItem->setForeground(QColor(failColor));
        } else if (e.isUnexpected) {
            procItem->setText(QStringLiteral("⚠ %1").arg(e.processName));
            procItem->setForeground(QColor(warningColor));
            procItem->setToolTip(tr("Binary path not in trusted prefixes."));
            pathItem->setForeground(QColor(warningColor));
            pathItem->setToolTip(
                tr("%1\n\nNot under a trusted system prefix.").arg(e.binaryPath));
        }

        row << procItem;
        row << stateItem;
        row << pathItem;

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

#ifdef Q_OS_MAC
void OpenPortsWidget::onVerifySignatures()
{
    // FR-121: run codesign on the binary of each visible row. Cache results
    // by path — the cache survives for the widget's lifetime. "Signed" means
    // `codesign -dv` returned exit code 0.
    mBtnVerifySigs->setEnabled(false);
    mBtnVerifySigs->setText(tr("Verifying…"));

    // Collect unique paths that aren't already cached.
    QStringList needed;
    QSet<QString> seen;
    for (const ConnectionEntry &e : mEntries) {
        if (e.binaryPath.isEmpty())
            continue;
        if (mCodesignCache.contains(e.binaryPath))
            continue;
        if (seen.contains(e.binaryPath))
            continue;
        seen.insert(e.binaryPath);
        needed << e.binaryPath;
    }

    QThreadPool::globalInstance()->start([this, needed]() {
        QHash<QString, bool> results;
        for (const QString &path : needed) {
            ExecResult r = CommandUtil::execWithStatus(
                "codesign", {"-dv", "--verbose=0", path}, 3000);
            results.insert(path, r.ok());
        }
        QMetaObject::invokeMethod(this, [this, results]() {
            for (auto it = results.constBegin(); it != results.constEnd(); ++it)
                mCodesignCache.insert(it.key(), it.value());

            // Apply isUnsigned to mEntries from the merged cache.
            for (ConnectionEntry &e : mEntries) {
                if (e.binaryPath.isEmpty())
                    continue;
                if (mCodesignCache.contains(e.binaryPath))
                    e.isUnsigned = !mCodesignCache.value(e.binaryPath);
            }
            onConnectionsFetched(mEntries);   // re-render with new flags.

            mBtnVerifySigs->setEnabled(true);
            mBtnVerifySigs->setText(tr("Verify Signatures"));
        }, Qt::QueuedConnection);
    });
}
#endif

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
