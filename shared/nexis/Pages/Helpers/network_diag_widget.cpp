#include "network_diag_widget.h"

#include <Utils/command_util.h>
#include <Managers/app_manager.h>
#include <Managers/info_manager.h>

#include <QBoxLayout>
#include <QElapsedTimer>
#include <QFrame>
#include <QHostInfo>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QThreadPool>

// ---------------------------------------------------------------------------
// Static parsers
// ---------------------------------------------------------------------------

QString NetworkDiagWidget::parseGatewayFromRoute(const QString &output)
{
    static const QRegularExpression re(R"(gateway:\s*([\d.]+))");
    QRegularExpressionMatch m = re.match(output);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString NetworkDiagWidget::parseGatewayFromIpRoute(const QString &output)
{
    static const QRegularExpression re(R"(default\s+via\s+([\d.]+))");
    QRegularExpressionMatch m = re.match(output);
    return m.hasMatch() ? m.captured(1) : QString();
}

double NetworkDiagWidget::parsePingLatency(const QString &output)
{
    static const QRegularExpression re(R"(time[=<]([\d.]+)\s*ms)");
    QRegularExpressionMatch m = re.match(output);
    if (m.hasMatch()) {
        bool ok = false;
        double val = m.captured(1).toDouble(&ok);
        return ok ? val : -1.0;
    }
    return -1.0;
}

QStringList NetworkDiagWidget::parseDnsFromScutilDns(const QString &output)
{
    QStringList servers;
    static const QRegularExpression re(R"(nameserver\[\d+\]\s*:\s*([\d.:a-fA-F]+))");
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    while (it.hasNext()) {
        QString addr = it.next().captured(1);
        if (!servers.contains(addr))
            servers << addr;
    }
    return servers;
}

QStringList NetworkDiagWidget::parseDnsFromResolvectl(const QString &output)
{
    QStringList servers;
    static const QRegularExpression re(
        R"(^\s*DNS\s+Servers\s*:\s*(.+)$)",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    while (it.hasNext()) {
        QString line = it.next().captured(1).trimmed();
        for (const QString &addr : line.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts)) {
            if (!servers.contains(addr))
                servers << addr;
        }
    }
    return servers;
}

QStringList NetworkDiagWidget::parseDnsFromResolvConf(const QString &output)
{
    QStringList servers;
    static const QRegularExpression re(R"(^nameserver\s+([\d.:a-fA-F]+))", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    while (it.hasNext()) {
        QString addr = it.next().captured(1);
        if (!servers.contains(addr))
            servers << addr;
    }
    return servers;
}

// ---------------------------------------------------------------------------
// Diagnostic execution (runs on worker thread)
// ---------------------------------------------------------------------------

QString NetworkDiagWidget::discoverGateway()
{
#ifdef Q_OS_MACOS
    ExecResult r = CommandUtil::execWithStatus("route", {"-n", "get", "default"}, 5000);
    if (r.exitCode == 0)
        return parseGatewayFromRoute(r.output);
#else
    if (CommandUtil::isExecutable("ip")) {
        ExecResult r = CommandUtil::execWithStatus("ip", {"route", "show", "default"}, 5000);
        if (r.exitCode == 0) {
            QString gw = parseGatewayFromIpRoute(r.output);
            if (!gw.isEmpty())
                return gw;
        }
    }
    ExecResult r = CommandUtil::execWithStatus("route", {"-n"}, 5000);
    if (r.exitCode == 0) {
        static const QRegularExpression re(R"(^0\.0\.0\.0\s+([\d.]+))", QRegularExpression::MultilineOption);
        QRegularExpressionMatch m = re.match(r.output);
        if (m.hasMatch())
            return m.captured(1);
    }
#endif
    return {};
}

DiagCheck NetworkDiagWidget::pingHost(const QString &label, const QString &host)
{
    DiagCheck check;
    check.label = label;

#ifdef Q_OS_MACOS
    ExecResult r = CommandUtil::execWithStatus("ping", {"-c", "1", "-W", "3000", host}, 5000);
#else
    ExecResult r = CommandUtil::execWithStatus("ping", {"-c", "1", "-W", "3", host}, 5000);
#endif

    if (r.exitCode == 0) {
        check.passed = true;
        check.latencyMs = parsePingLatency(r.output);
    } else {
        check.passed = false;
        check.errorMsg = r.error.isEmpty() ? tr("No response") : r.error.left(120);
    }
    return check;
}

QStringList NetworkDiagWidget::discoverDnsServers()
{
#ifdef Q_OS_MACOS
    ExecResult r = CommandUtil::execWithStatus("scutil", {"--dns"}, 5000);
    if (r.exitCode == 0)
        return parseDnsFromScutilDns(r.output);
#else
    if (CommandUtil::isExecutable("resolvectl")) {
        ExecResult r = CommandUtil::execWithStatus("resolvectl", {"status"}, 5000);
        if (r.exitCode == 0) {
            QStringList servers = parseDnsFromResolvectl(r.output);
            if (!servers.isEmpty())
                return servers;
        }
    }
    ExecResult r = CommandUtil::execWithStatus("cat", {"/etc/resolv.conf"}, 5000);
    if (r.exitCode == 0)
        return parseDnsFromResolvConf(r.output);
#endif
    return {};
}

DiagResult NetworkDiagWidget::runDiagnostics()
{
    DiagResult result;
    result.interface = InfoManager::ins()->getDefaultNetworkInterface();

    QString gateway = discoverGateway();

    if (!gateway.isEmpty()) {
        result.checks << pingHost(tr("Gateway (%1)").arg(gateway), gateway);
    } else {
        DiagCheck gwCheck;
        gwCheck.label = tr("Gateway");
        gwCheck.passed = false;
        gwCheck.errorMsg = tr("Could not determine default gateway");
        result.checks << gwCheck;
    }

    result.checks << pingHost(tr("Internet (1.1.1.1)"), "1.1.1.1");

    {
        DiagCheck dnsCheck;
        dnsCheck.label = tr("DNS Resolution (cloudflare.com)");
        QElapsedTimer timer;
        timer.start();
        QHostInfo info = QHostInfo::fromName("cloudflare.com");
        double elapsed = timer.elapsed();

        if (info.error() == QHostInfo::NoError && !info.addresses().isEmpty()) {
            dnsCheck.passed = true;
            dnsCheck.latencyMs = elapsed;
        } else {
            dnsCheck.passed = false;
            dnsCheck.errorMsg = info.errorString();
        }
        result.checks << dnsCheck;
    }

    result.dnsServers = discoverDnsServers();

    return result;
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

NetworkDiagWidget::NetworkDiagWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &NetworkDiagWidget::diagnosticsFinished,
            this, &NetworkDiagWidget::onDiagnosticsFinished);
}

void NetworkDiagWidget::buildUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("Network Diagnostics"));
    mLblTitle->setObjectName("netDiagTitle");
    root->addWidget(mLblTitle);

    QFrame *resultsCard = new QFrame;
    resultsCard->setObjectName("netDiagCard");
    mResultsLayout = new QVBoxLayout(resultsCard);
    mResultsLayout->setContentsMargins(14, 10, 14, 10);
    mResultsLayout->setSpacing(8);
    root->addWidget(resultsCard);

    mLblDnsHeader = new QLabel(tr("DNS Servers"));
    mLblDnsHeader->setObjectName("netDiagSubheader");
    mLblDnsHeader->hide();
    root->addWidget(mLblDnsHeader);

    QFrame *dnsCard = new QFrame;
    dnsCard->setObjectName("netDiagCard");
    mDnsLayout = new QVBoxLayout(dnsCard);
    mDnsLayout->setContentsMargins(14, 10, 14, 10);
    mDnsLayout->setSpacing(4);
    dnsCard->hide();
    root->addWidget(dnsCard);

    QHBoxLayout *footer = new QHBoxLayout;
    footer->setSpacing(12);
    mLblInterface = new QLabel;
    mLblInterface->setObjectName("netDiagSecondary");
    footer->addWidget(mLblInterface);

    footer->addStretch();

    mBtnRetest = new QPushButton(tr("Re-test"));
    mBtnRetest->setCursor(Qt::PointingHandCursor);
    mBtnRetest->setFocusPolicy(Qt::NoFocus);
    mBtnRetest->setObjectName("netDiagRetest");
    connect(mBtnRetest, &QPushButton::clicked, this, &NetworkDiagWidget::runTest);
    footer->addWidget(mBtnRetest);
    root->addLayout(footer);

    mLblLoading = new QLabel(tr("Running diagnostics..."));
    mLblLoading->setObjectName("netDiagLoading");
    mLblLoading->setAlignment(Qt::AlignCenter);
    mLblLoading->hide();
    root->addWidget(mLblLoading);

    root->addStretch();
}

// ---------------------------------------------------------------------------
// Run / result handling
// ---------------------------------------------------------------------------

void NetworkDiagWidget::runTestIfNeeded()
{
    if (!mHasRun)
        runTest();
}

void NetworkDiagWidget::runTest()
{
    mHasRun = true;
    mBtnRetest->setEnabled(false);
    mLblLoading->show();
    clearResults();

    QThreadPool::globalInstance()->start([this]() {
        DiagResult result = runDiagnostics();
        emit diagnosticsFinished(result);
    });
}

void NetworkDiagWidget::clearResults()
{
    while (mResultsLayout->count() > 0) {
        QLayoutItem *item = mResultsLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
    while (mDnsLayout->count() > 0) {
        QLayoutItem *item = mDnsLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
    mLblDnsHeader->hide();
    mLblDnsHeader->parentWidget(); // no-op, just for clarity
    if (QFrame *dnsCard = qobject_cast<QFrame *>(mDnsLayout->parentWidget()))
        dnsCard->hide();
    mLblInterface->clear();
}

void NetworkDiagWidget::onDiagnosticsFinished(DiagResult result)
{
    mLblLoading->hide();
    mBtnRetest->setEnabled(true);

    QSettings *sv = AppManager::ins()->getStyleValues();
    QString successColor = sv ? sv->value("@successColor").toString() : "#2ec27e";
    QString failColor    = sv ? sv->value("@destructiveColor").toString() : "#E05454";
    QString textColor    = sv ? sv->value("@color05").toString() : "#F0F2F5";
    QString secColor     = sv ? sv->value("@color04").toString() : "#9A9DA6";

    for (const DiagCheck &check : result.checks) {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(8);

        QLabel *icon = new QLabel;
        if (check.passed) {
            icon->setText("\xe2\x9c\x93");
            icon->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;").arg(successColor));
        } else {
            icon->setText("\xe2\x9c\x97");
            icon->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;").arg(failColor));
        }
        icon->setFixedWidth(20);
        icon->setAlignment(Qt::AlignCenter);
        row->addWidget(icon);

        QLabel *label = new QLabel(check.label);
        label->setStyleSheet(QString("color: %1; font-size: 13px;").arg(textColor));
        row->addWidget(label, 1);

        QLabel *value = new QLabel;
        if (check.passed && check.latencyMs >= 0) {
            value->setText(QString("%1 ms").arg(check.latencyMs, 0, 'f', 1));
            value->setStyleSheet(QString("color: %1; font-size: 13px;").arg(secColor));
        } else if (!check.passed) {
            QString msg = check.errorMsg.isEmpty() ? tr("FAILED") : check.errorMsg;
            value->setText(msg);
            value->setStyleSheet(QString("color: %1; font-size: 13px;").arg(failColor));
        }
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(value);

        QWidget *rowWidget = new QWidget;
        rowWidget->setLayout(row);
        mResultsLayout->addWidget(rowWidget);
    }

    if (!result.dnsServers.isEmpty()) {
        mLblDnsHeader->show();
        if (QFrame *dnsCard = qobject_cast<QFrame *>(mDnsLayout->parentWidget()))
            dnsCard->show();

        for (const QString &server : result.dnsServers) {
            QLabel *lbl = new QLabel(server);
            lbl->setStyleSheet(QString("color: %1; font-size: 13px;").arg(textColor));
            mDnsLayout->addWidget(lbl);
        }
    }

    if (!result.interface.isEmpty())
        mLblInterface->setText(tr("Interface: %1").arg(result.interface));
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
