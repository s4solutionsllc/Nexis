#include "trim_widget.h"

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/command_util.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QThreadPool>
#include <QVBoxLayout>

TrimWidget::TrimWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &TrimWidget::statusFetched, this, &TrimWidget::onStatusFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &TrimWidget::refreshThemeColors);
    refreshThemeColors();
}

void TrimWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void TrimWidget::refresh()
{
    mLoaded = true;
    mLblLoading->show();
    if (mBtnToggle) mBtnToggle->setEnabled(false);
    if (mBtnRunNow) mBtnRunNow->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this]() {
        TrimStatus s = fetchStatus();
        emit statusFetched(s);
    });
}

#ifdef Q_OS_LINUX
namespace {

// Parse a line from `systemctl list-timers fstrim.timer --no-legend`. The
// column layout is variable but the first two columns are NEXT + LEFT and
// the fourth is LAST. We just return the raw next/last strings for display.
void parseListTimersLine(const QString &line, QString &nextOut, QString &lastOut)
{
    // Example output: "Sun 2026-04-27 00:00:00 EDT 4 days 3h Mon 2026-04-20 00:13:12 EDT 6 days ago fstrim.timer fstrim.service"
    // Skip tokens until we see a month weekday (Mon/Tue/…). Simpler: split
    // on "  " double-space — systemctl pads columns with multiple spaces.
    static const QRegularExpression twoSpaces(R"( {2,})");
    const QStringList cols = line.split(twoSpaces, Qt::SkipEmptyParts);
    if (cols.size() >= 1) nextOut = cols.at(0).trimmed();
    if (cols.size() >= 3) lastOut = cols.at(2).trimmed();
}

} // namespace
#endif

TrimStatus TrimWidget::fetchStatus()
{
    TrimStatus s;

#ifdef Q_OS_LINUX
    if (!CommandUtil::isExecutable("fstrim")) {
        s.errorMsg = tr("fstrim is not installed. Install the `util-linux` package.");
        return s;
    }
    s.available = true;

    ExecResult active   = CommandUtil::execWithStatus(
        "systemctl", {"is-active", "fstrim.timer"}, 3000);
    ExecResult enabled  = CommandUtil::execWithStatus(
        "systemctl", {"is-enabled", "fstrim.timer"}, 3000);

    s.timerActive  = (active.output.trimmed() == "active");
    s.timerEnabled = (enabled.output.trimmed() == "enabled");

    if (s.timerEnabled || s.timerActive) {
        ExecResult timers = CommandUtil::execWithStatus(
            "systemctl",
            {"list-timers", "fstrim.timer", "--all", "--no-legend"}, 3000);
        if (timers.exitCode == 0) {
            const QStringList lines =
                timers.output.split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty())
                parseListTimersLine(lines.first(), s.nextRun, s.lastRun);
        }
    }

    s.platformNote = tr("Managed by systemd (fstrim.timer)");

#elif defined(Q_OS_MAC)
    // macOS: read-only status. Parse `diskutil info -plist /` for TRIM state.
    ExecResult r = CommandUtil::execWithStatus(
        "diskutil", {"info", "-plist", "/"}, 5000);
    s.available = true;
    if (r.exitCode == 0) {
        // Cheap text search — plist XML is structured but we only want a bool.
        const QString &o = r.output;
        // TrimForce and Trim both appear in practice. If any <true/> directly
        // follows a <key>Trim*</key> treat TRIM as on.
        static const QRegularExpression re(
            R"(<key>Trim(Enabled|Support)?</key>\s*<(true|false)/>)");
        auto m = re.match(o);
        if (m.hasMatch()) {
            s.trimEnabled = (m.captured(2) == "true");
        } else {
            // Fallback: SPNVMeDataType "TRIM Support: Yes"
            ExecResult sp = CommandUtil::execWithStatus(
                "system_profiler", {"SPNVMeDataType"}, 5000);
            s.trimEnabled = sp.output.contains("TRIM Support: Yes");
        }
    }
    s.platformNote = tr("TRIM on macOS is managed by the OS.");
#else
    s.errorMsg = tr("SSD TRIM tuning isn't available on this platform.");
#endif

    return s;
}

#ifdef Q_OS_LINUX
bool TrimWidget::toggleTimer(bool enable)
{
    if (enable) {
        CommandUtil::sudoExec("systemctl", {"enable", "--now", "fstrim.timer"});
    } else {
        CommandUtil::sudoExec("systemctl", {"disable", "--now", "fstrim.timer"});
    }
    ExecResult verify = CommandUtil::execWithStatus(
        "systemctl", {"is-enabled", "fstrim.timer"}, 3000);
    const bool nowEnabled = (verify.output.trimmed() == "enabled");
    return nowEnabled == enable;
}

QString TrimWidget::runFstrimNow()
{
    // fstrim emits one line per mount trimmed. Exit code 0 on success,
    // 64 on partial, 32 on other errors.
    ExecResult r = CommandUtil::execWithStatus("pkexec",
        {"fstrim", "-av"}, 60000);
    if (r.output.isEmpty() && !r.error.isEmpty())
        return r.error;
    return r.output;
}
#endif

void TrimWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("SSD TRIM"), this);
    QFont titleFont = mLblTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    mLblTitle->setFont(titleFont);
    root->addWidget(mLblTitle);

    mCard = new QFrame(this);
    mCard->setObjectName("trimCard");
    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 16, 16, 16);
    card->setSpacing(10);

    // Status row
    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);
    mLblDot = new QLabel(mCard);
    mLblDot->setFixedSize(12, 12);
    mLblDot->setObjectName("trimDot");
    statusRow->addWidget(mLblDot);
    mLblStatus = new QLabel(mCard);
    mLblStatus->setObjectName("trimStatus");
    statusRow->addWidget(mLblStatus);
    statusRow->addStretch();
    card->addLayout(statusRow);

    mLblLastRun  = new QLabel(mCard);
    mLblLastRun->setObjectName("trimLastRun");
    mLblLastRun->hide();
    card->addWidget(mLblLastRun);

    mLblNextRun  = new QLabel(mCard);
    mLblNextRun->setObjectName("trimNextRun");
    mLblNextRun->hide();
    card->addWidget(mLblNextRun);

    mLblPlatform = new QLabel(mCard);
    mLblPlatform->setObjectName("trimPlatform");
    mLblPlatform->setWordWrap(true);
    card->addWidget(mLblPlatform);

    // Buttons — only shown on Linux
#ifdef Q_OS_LINUX
    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    mBtnToggle = new QPushButton(mCard);
    mBtnToggle->setCursor(Qt::PointingHandCursor);
    mBtnToggle->setFocusPolicy(Qt::NoFocus);
    mBtnToggle->setAccessibleName("primary");
    connect(mBtnToggle, &QPushButton::clicked, this, &TrimWidget::onToggleTimer);
    actionRow->addWidget(mBtnToggle);

    mBtnRunNow = new QPushButton(tr("Run TRIM now"), mCard);
    mBtnRunNow->setCursor(Qt::PointingHandCursor);
    mBtnRunNow->setFocusPolicy(Qt::NoFocus);
    connect(mBtnRunNow, &QPushButton::clicked, this, &TrimWidget::onRunNow);
    actionRow->addWidget(mBtnRunNow);
    actionRow->addStretch();
    card->addLayout(actionRow);

    mTxtOutput = new QPlainTextEdit(mCard);
    mTxtOutput->setReadOnly(true);
    mTxtOutput->setMaximumHeight(120);
    mTxtOutput->hide();
    card->addWidget(mTxtOutput);
#endif

    root->addWidget(mCard);

    // Controls
    auto *controls = new QHBoxLayout();
    controls->setSpacing(8);
    mBtnRefresh = new QPushButton(tr("Refresh"), this);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    mBtnRefresh->setFocusPolicy(Qt::NoFocus);
    connect(mBtnRefresh, &QPushButton::clicked, this, &TrimWidget::refresh);
    controls->addWidget(mBtnRefresh);

    mLblLoading = new QLabel(tr("Loading…"), this);
    mLblLoading->hide();
    controls->addWidget(mLblLoading);
    controls->addStretch();

    mLblResult = new QLabel(this);
    mLblResult->setObjectName("trimResult");
    controls->addWidget(mLblResult);

    root->addLayout(controls);
    root->addStretch();
}

void TrimWidget::onStatusFetched(TrimStatus status)
{
    mCurrent = status;
    mLblLoading->hide();
    mBtnRefresh->setEnabled(true);
    renderStatus(status);
}

void TrimWidget::renderStatus(const TrimStatus &s)
{
    if (!s.available) {
        mCard->hide();
        if (mBtnToggle)  mBtnToggle->hide();
        if (mBtnRunNow)  mBtnRunNow->hide();
        mLblResult->setText(s.errorMsg);
        return;
    }

    mCard->show();

#ifdef Q_OS_LINUX
    if (s.timerEnabled && s.timerActive) {
        mLblStatus->setText(tr("Scheduled · enabled and running"));
    } else if (s.timerEnabled) {
        mLblStatus->setText(tr("Scheduled · not yet active"));
    } else {
        mLblStatus->setText(tr("Not scheduled"));
    }

    if (!s.lastRun.isEmpty()) {
        mLblLastRun->setText(tr("Last run: %1").arg(s.lastRun));
        mLblLastRun->show();
    } else {
        mLblLastRun->hide();
    }
    if (!s.nextRun.isEmpty()) {
        mLblNextRun->setText(tr("Next run: %1").arg(s.nextRun));
        mLblNextRun->show();
    } else {
        mLblNextRun->hide();
    }

    if (mBtnToggle) {
        mBtnToggle->setText(s.timerEnabled ? tr("Disable weekly TRIM")
                                           : tr("Enable weekly TRIM"));
        mBtnToggle->setEnabled(true);
    }
    if (mBtnRunNow)
        mBtnRunNow->setEnabled(true);

#elif defined(Q_OS_MAC)
    mLblStatus->setText(s.trimEnabled
        ? tr("TRIM: Enabled")
        : tr("TRIM: Not reported"));
#endif

    mLblPlatform->setText(s.platformNote);
    refreshThemeColors();
}

void TrimWidget::onToggleTimer()
{
#ifdef Q_OS_LINUX
    const bool enable = !mCurrent.timerEnabled;
    mBtnToggle->setEnabled(false);
    mBtnRunNow->setEnabled(false);
    mLblLoading->show();
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this, enable]() {
        const bool ok = toggleTimer(enable);
        QMetaObject::invokeMethod(this, [this, ok, enable]() {
            mLblLoading->hide();
            if (ok) {
                mLblResult->setText(enable ? tr("✓ Weekly TRIM enabled")
                                           : tr("✓ Weekly TRIM disabled"));
                refresh();
            } else {
                mLblResult->setText(tr("⚠ Toggle failed — did you cancel the password prompt?"));
                mBtnToggle->setEnabled(true);
                mBtnRunNow->setEnabled(true);
            }
            refreshThemeColors();
        }, Qt::QueuedConnection);
    });
#endif
}

void TrimWidget::onRunNow()
{
#ifdef Q_OS_LINUX
    mBtnToggle->setEnabled(false);
    mBtnRunNow->setEnabled(false);
    mLblLoading->show();
    mLblResult->clear();
    mTxtOutput->clear();

    QThreadPool::globalInstance()->start([this]() {
        const QString out = runFstrimNow();
        QMetaObject::invokeMethod(this, [this, out]() {
            mLblLoading->hide();
            mBtnToggle->setEnabled(true);
            mBtnRunNow->setEnabled(true);
            if (out.isEmpty()) {
                mLblResult->setText(tr("⚠ fstrim returned no output"));
            } else {
                mLblResult->setText(tr("✓ TRIM complete"));
                mTxtOutput->setPlainText(out);
                mTxtOutput->show();
            }
            refreshThemeColors();
        }, Qt::QueuedConnection);
    });
#endif
}

void TrimWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg     = sv->value("@cardBg").toString();
    const QString border     = sv->value("@borderColor").toString();
    const QString secondary  = sv->value("@color04").toString();
    const QString successCol = sv->value("@successColor").toString();
    const QString warnCol    = sv->value("@warningColor").toString();

    mCard->setStyleSheet(QString(
        "QFrame#trimCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}").arg(cardBg, border));

    mLblPlatform->setStyleSheet(QString("color: %1;").arg(secondary));
    mLblLastRun->setStyleSheet(QString("color: %1;").arg(secondary));
    mLblNextRun->setStyleSheet(QString("color: %1;").arg(secondary));

    // Status dot color
    QString dotColor;
#ifdef Q_OS_LINUX
    dotColor = mCurrent.timerEnabled ? successCol : secondary;
#else
    dotColor = mCurrent.trimEnabled ? successCol : secondary;
#endif
    mLblDot->setStyleSheet(QString(
        "background-color: %1; border-radius: 6px;").arg(dotColor));

    const QString resultText = mLblResult->text();
    if (resultText.startsWith(QStringLiteral("✓")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(successCol));
    else if (resultText.startsWith(QStringLiteral("⚠")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
    else
        mLblResult->setStyleSheet(QString());
}
