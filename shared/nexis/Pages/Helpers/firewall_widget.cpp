#include "firewall_widget.h"

#include <Utils/command_util.h>
#include <Managers/app_manager.h>
#include "signal_mapper.h"

#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QThreadPool>
#include <QToolButton>

// ---------------------------------------------------------------------------
// Static parsers
// ---------------------------------------------------------------------------

FirewallStatus FirewallWidget::parseMacFirewallOutput(const QString &globalState,
                                                       const QString &stealthMode,
                                                       const QString &blockAll,
                                                       const QString &listApps)
{
    FirewallStatus s;
    s.available = true;
    s.backend = "macOS Application Firewall";

    static const QRegularExpression reState(R"(State\s*=\s*(\d+))");
    QRegularExpressionMatch m = reState.match(globalState);
    if (m.hasMatch())
        s.enabled = (m.captured(1) != "0");

    s.stealthMode = stealthMode.contains("is on", Qt::CaseInsensitive);
    s.blockAll = blockAll.contains("set to enabled", Qt::CaseInsensitive);

    static const QRegularExpression reApps(R"(Total number of apps\s*=\s*(\d+))");
    QRegularExpressionMatch am = reApps.match(listApps);
    if (am.hasMatch())
        s.appRuleCount = am.captured(1).toInt();

    return s;
}

FirewallStatus FirewallWidget::parseUfwOutput(const QString &output)
{
    FirewallStatus s;
    if (output.isEmpty())
        return s;

    s.available = true;
    s.backend = "ufw";
    s.enabled = output.contains("Status: active", Qt::CaseInsensitive);
    return s;
}

FirewallStatus FirewallWidget::parseFirewalldOutput(const QString &output)
{
    FirewallStatus s;
    if (output.isEmpty())
        return s;

    s.available = true;
    s.backend = "firewalld";
    s.enabled = output.trimmed().startsWith("running", Qt::CaseInsensitive);
    return s;
}

// ---------------------------------------------------------------------------
// Data fetching
// ---------------------------------------------------------------------------

static const QString kSocketFilterFw =
    "/usr/libexec/ApplicationFirewall/socketfilterfw";

FirewallStatus FirewallWidget::fetchStatus()
{
#ifdef Q_OS_MACOS
    ExecResult rState   = CommandUtil::execWithStatus(kSocketFilterFw, {"--getglobalstate"});
    ExecResult rStealth = CommandUtil::execWithStatus(kSocketFilterFw, {"--getstealthmode"});
    ExecResult rBlock   = CommandUtil::execWithStatus(kSocketFilterFw, {"--getblockall"});
    ExecResult rApps    = CommandUtil::execWithStatus(kSocketFilterFw, {"--listapps"});
    return parseMacFirewallOutput(rState.output, rStealth.output,
                                  rBlock.output, rApps.output);
#else
    if (CommandUtil::isExecutable("ufw")) {
        ExecResult r = CommandUtil::execWithStatus("ufw", {"status"});
        if (r.exitCode == 0 || !r.output.isEmpty())
            return parseUfwOutput(r.output);
    }
    if (CommandUtil::isExecutable("firewall-cmd")) {
        ExecResult r = CommandUtil::execWithStatus("firewall-cmd", {"--state"});
        if (r.exitCode == 0 || !r.output.isEmpty())
            return parseFirewalldOutput(r.output);
    }
    FirewallStatus s;
    s.available = false;
    return s;
#endif
}

void FirewallWidget::toggleFirewall(bool enable)
{
#ifdef Q_OS_MACOS
    CommandUtil::sudoExec(kSocketFilterFw,
                          {"--setglobalstate", enable ? "on" : "off"});
#else
    if (mCurrentStatus.backend == "ufw") {
        CommandUtil::sudoExec("ufw", {enable ? "--force enable" : "disable"});
    } else if (mCurrentStatus.backend == "firewalld") {
        CommandUtil::sudoExec("systemctl",
                              {enable ? "start" : "stop", "firewalld"});
    }
#endif
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

FirewallWidget::FirewallWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    refreshThemeColors();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &FirewallWidget::refreshThemeColors);
    connect(this, &FirewallWidget::statusFetched,
            this, &FirewallWidget::onStatusFetched);
}

void FirewallWidget::buildUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(16);

    mLblTitle = new QLabel(tr("Firewall Status"));
    mLblTitle->setObjectName("fwTitle");
    root->addWidget(mLblTitle);

    // --- Status row ---
    QHBoxLayout *statusRow = new QHBoxLayout;
    statusRow->setSpacing(10);

    mLblDot = new QLabel;
    mLblDot->setFixedSize(14, 14);
    mLblDot->setAlignment(Qt::AlignCenter);
    statusRow->addWidget(mLblDot);

    mLblStatus = new QLabel;
    mLblStatus->setObjectName("fwStatusText");
    statusRow->addWidget(mLblStatus);

    statusRow->addStretch();

    mBtnToggle = new QPushButton;
    mBtnToggle->setCursor(Qt::PointingHandCursor);
    mBtnToggle->setFocusPolicy(Qt::NoFocus);
    mBtnToggle->setObjectName("fwToggle");
    connect(mBtnToggle, &QPushButton::clicked, this, &FirewallWidget::onToggleClicked);
    statusRow->addWidget(mBtnToggle);

    root->addLayout(statusRow);

    // --- Detail rows ---
    mDetailWidget = new QWidget;
    mDetailWidget->setObjectName("fwDetailCard");
    QFormLayout *form = new QFormLayout(mDetailWidget);
    form->setContentsMargins(16, 12, 16, 12);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    mLblBackend = new QLabel;
    mLblBackend->setObjectName("fwSecondary");
    form->addRow(tr("Backend:"), mLblBackend);

    mLblStealth = new QLabel;
    mLblStealth->setObjectName("fwSecondary");
    form->addRow(tr("Stealth Mode:"), mLblStealth);

    mLblBlockAll = new QLabel;
    mLblBlockAll->setObjectName("fwSecondary");
    form->addRow(tr("Block All Incoming:"), mLblBlockAll);

    mLblAppRules = new QLabel;
    mLblAppRules->setObjectName("fwSecondary");
    form->addRow(tr("App Rules:"), mLblAppRules);

    root->addWidget(mDetailWidget);

    // --- Not-available state ---
    mNotAvailWidget = new QWidget;
    QHBoxLayout *naRow = new QHBoxLayout(mNotAvailWidget);
    naRow->setContentsMargins(0, 0, 0, 0);
    naRow->setSpacing(8);

    mLblNotAvail = new QLabel;
    mLblNotAvail->setObjectName("fwWarning");
    naRow->addWidget(mLblNotAvail);

    mBtnHelp = new QToolButton;
    mBtnHelp->setText("?");
    mBtnHelp->setAutoRaise(true);
    mBtnHelp->setFocusPolicy(Qt::NoFocus);
    mBtnHelp->setCursor(Qt::WhatsThisCursor);
    mBtnHelp->setObjectName("fwHelpBtn");
    mBtnHelp->setToolTip(
        tr("Install one of the following to enable firewall management:\n\n"
           "\xe2\x80\xa2 ufw \xe2\x80\x94 sudo apt install ufw\n"
           "\xe2\x80\xa2 firewalld \xe2\x80\x94 sudo dnf install firewalld"));
    naRow->addWidget(mBtnHelp);

    naRow->addStretch();
    mNotAvailWidget->hide();
    root->addWidget(mNotAvailWidget);

    // --- Refresh + Loading ---
    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);

    mBtnRefresh = new QPushButton(tr("Refresh"));
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    mBtnRefresh->setFocusPolicy(Qt::NoFocus);
    mBtnRefresh->setObjectName("fwRefresh");
    connect(mBtnRefresh, &QPushButton::clicked, this, &FirewallWidget::refresh);
    bottomRow->addWidget(mBtnRefresh);

    mLblLoading = new QLabel(tr("Checking firewall status..."));
    mLblLoading->setObjectName("fwSecondary");
    mLblLoading->hide();
    bottomRow->addWidget(mLblLoading);

    bottomRow->addStretch();
    root->addLayout(bottomRow);

    root->addStretch();
}

// ---------------------------------------------------------------------------
// Load / refresh
// ---------------------------------------------------------------------------

void FirewallWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void FirewallWidget::refresh()
{
    mLoaded = true;
    mBtnRefresh->setEnabled(false);
    mBtnToggle->setEnabled(false);
    mLblLoading->show();

    QThreadPool::globalInstance()->start([this]() {
        FirewallStatus s = fetchStatus();
        emit statusFetched(s);
    });
}

void FirewallWidget::onStatusFetched(FirewallStatus status)
{
    mCurrentStatus = status;
    mLblLoading->hide();
    mBtnRefresh->setEnabled(true);

    if (!status.available) {
        mLblDot->hide();
        mLblStatus->hide();
        mBtnToggle->hide();
        mDetailWidget->hide();
        mNotAvailWidget->show();
        mLblNotAvail->setText(tr("\xe2\x9a\xa0 No supported firewall detected."));
        return;
    }

    mNotAvailWidget->hide();
    mLblDot->show();
    mLblStatus->show();
    mBtnToggle->show();
    mDetailWidget->show();
    mBtnToggle->setEnabled(true);

    QSettings *sv = AppManager::ins()->getStyleValues();
    QString successColor = sv ? sv->value("@successColor").toString() : "#2ec27e";
    QString failColor    = sv ? sv->value("@destructiveColor").toString() : "#E05454";

    if (status.enabled) {
        mLblDot->setText("\xe2\x97\x8f");
        mLblDot->setStyleSheet(QString("color: %1; font-size: 16px;").arg(successColor));
        mLblStatus->setText(tr("Firewall: Enabled"));
        mBtnToggle->setText(tr("Disable"));
    } else {
        mLblDot->setText("\xe2\x97\x8f");
        mLblDot->setStyleSheet(QString("color: %1; font-size: 16px;").arg(failColor));
        mLblStatus->setText(tr("Firewall: Disabled"));
        mBtnToggle->setText(tr("Enable"));
    }

    mLblBackend->setText(status.backend);

    bool isMac = (status.backend == "macOS Application Firewall");

    mLblStealth->parentWidget()->layout();
    QFormLayout *form = qobject_cast<QFormLayout *>(mDetailWidget->layout());
    if (form) {
        form->labelForField(mLblStealth)->setVisible(isMac);
        mLblStealth->setVisible(isMac);
        form->labelForField(mLblBlockAll)->setVisible(isMac);
        mLblBlockAll->setVisible(isMac);
        form->labelForField(mLblAppRules)->setVisible(isMac);
        mLblAppRules->setVisible(isMac);
    }

    if (isMac) {
        mLblStealth->setText(status.stealthMode ? tr("On") : tr("Off"));
        mLblBlockAll->setText(status.blockAll ? tr("Enabled") : tr("Disabled"));
        mLblAppRules->setText(status.appRuleCount >= 0
                                  ? QString::number(status.appRuleCount) : "\xe2\x80\x94");
    }
}

// ---------------------------------------------------------------------------
// Toggle
// ---------------------------------------------------------------------------

void FirewallWidget::onToggleClicked()
{
    bool willEnable = !mCurrentStatus.enabled;
    QString action = willEnable ? tr("enable") : tr("disable");

    if (QMessageBox::question(this, tr("Firewall"),
            tr("This will %1 the firewall. Administrator privileges are required.\n\nContinue?").arg(action),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    mBtnToggle->setEnabled(false);
    mLblLoading->show();
    mLblLoading->setText(willEnable ? tr("Enabling firewall...") : tr("Disabling firewall..."));

    QThreadPool::globalInstance()->start([this, willEnable]() {
        toggleFirewall(willEnable);
        FirewallStatus s = fetchStatus();
        emit statusFetched(s);
    });
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void FirewallWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString cardBg   = sv ? sv->value("@cardBg").toString() : "#2A2C32";
    QString textPri  = sv ? sv->value("@color05").toString() : "#F0F2F5";
    QString textSec  = sv ? sv->value("@color04").toString() : "#9A9DA6";
    QString border   = sv ? sv->value("@borderColor").toString() : "#4A4D5A";
    QString accent   = sv ? sv->value("@accentColor").toString() : "#FF6B1A";
    QString warnColor = sv ? sv->value("@warningColor").toString() : "#FFB347";

    if (mLblTitle)
        mLblTitle->setStyleSheet(
            QString("color: %1; font-size: 16px; font-weight: bold;").arg(textPri));

    if (mLblStatus)
        mLblStatus->setStyleSheet(
            QString("color: %1; font-size: 14px; font-weight: bold;").arg(textPri));

    if (mBtnToggle)
        mBtnToggle->setStyleSheet(QString(
            "QPushButton#fwToggle {"
            "  background-color: %1;"
            "  color: #ffffff;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 6px 16px;"
            "  font-size: 12px;"
            "}"
            "QPushButton#fwToggle:hover {"
            "  opacity: 0.9;"
            "}"
            "QPushButton#fwToggle:disabled {"
            "  background-color: %2;"
            "  color: %3;"
            "}").arg(accent, cardBg, textSec));

    if (mDetailWidget)
        mDetailWidget->setStyleSheet(QString(
            "#fwDetailCard {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 6px;"
            "}").arg(cardBg, border));

    for (QLabel *lbl : findChildren<QLabel *>("fwSecondary"))
        lbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(textSec));

    QList<QWidget *> formLabels;
    if (mDetailWidget) {
        QFormLayout *form = qobject_cast<QFormLayout *>(mDetailWidget->layout());
        if (form) {
            for (int i = 0; i < form->rowCount(); ++i) {
                QLayoutItem *labelItem = form->itemAt(i, QFormLayout::LabelRole);
                if (labelItem && labelItem->widget())
                    labelItem->widget()->setStyleSheet(
                        QString("color: %1; font-size: 12px;").arg(textSec));
            }
        }
    }

    if (mLblNotAvail)
        mLblNotAvail->setStyleSheet(
            QString("color: %1; font-size: 13px;").arg(warnColor));

    if (mBtnHelp)
        mBtnHelp->setStyleSheet(QString(
            "QToolButton#fwHelpBtn {"
            "  color: %1;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  border: 1px solid %1;"
            "  border-radius: 10px;"
            "  min-width: 20px;"
            "  max-width: 20px;"
            "  min-height: 20px;"
            "  max-height: 20px;"
            "}").arg(textSec));

    if (mBtnRefresh)
        mBtnRefresh->setStyleSheet(QString(
            "QPushButton#fwRefresh {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "  padding: 6px 14px;"
            "  font-size: 12px;"
            "}"
            "QPushButton#fwRefresh:hover {"
            "  border-color: %4;"
            "}"
            "QPushButton#fwRefresh:disabled {"
            "  color: %5;"
            "}").arg(cardBg, textPri, border, accent, textSec));

    if (mLoaded)
        onStatusFetched(mCurrentStatus);
}
