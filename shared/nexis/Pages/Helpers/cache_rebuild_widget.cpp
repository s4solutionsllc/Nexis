#include "cache_rebuild_widget.h"

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/command_util.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QThreadPool>
#include <QVBoxLayout>

CacheRebuildWidget::CacheRebuildWidget(QWidget *parent)
    : QWidget(parent)
{
    mRows.resize(kActionCount);
    buildUI();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &CacheRebuildWidget::refreshThemeColors);
    refreshThemeColors();
}

// ── Pure, testable command/gating logic ─────────────────────────────────────

QList<CacheRebuildWidget::Step> CacheRebuildWidget::commandsFor(Action action)
{
    switch (action) {
    case Action::DyldSharedCache:
        return {
            { QStringLiteral("update_dyld_shared_cache"), {QStringLiteral("-force")}, true,
              tr("Rebuilding dyld shared cache…") },
        };
    case Action::XpcCache:
        return {
            { QStringLiteral("rm"),
              {QStringLiteral("-f"), QStringLiteral("/var/db/xpcd/xpcd_cache.dylib")}, true,
              tr("Removing XPC cache…") },
        };
    case Action::FontCache:
        return {
            { QStringLiteral("atsutil"),
              {QStringLiteral("databases"), QStringLiteral("-remove")}, true,
              tr("Removing font database…") },
            { QStringLiteral("atsutil"),
              {QStringLiteral("server"), QStringLiteral("-shutdown")}, false,
              tr("Restarting font server…") },
            { QStringLiteral("atsutil"),
              {QStringLiteral("server"), QStringLiteral("-ping")}, false,
              tr("Restarting font server…") },
        };
    case Action::LaunchpadReset:
        return {
            { QStringLiteral("defaults"),
              {QStringLiteral("write"), QStringLiteral("com.apple.dock"),
               QStringLiteral("ResetLaunchPad"), QStringLiteral("-bool"), QStringLiteral("true")},
              false, tr("Resetting Launchpad layout…") },
            { QStringLiteral("killall"), {QStringLiteral("Dock")}, false,
              tr("Restarting Dock…") },
        };
    }
    return {};
}

CacheRebuildWidget::SupportInfo CacheRebuildWidget::supportInfo(Action action,
                                                                 QOperatingSystemVersion version)
{
    SupportInfo info;
    switch (action) {
    case Action::DyldSharedCache:
        // Big Sur (11.0) moved the shared cache onto the cryptographically
        // sealed System volume; `update_dyld_shared_cache` no longer ships
        // and there is no supported user-space rebuild path.
        if (version.majorVersion() >= 11) {
            info.available = false;
            info.reason = tr(
                "Not available on macOS 11 (Big Sur) or later — the dyld "
                "shared cache now lives on the signed system volume and "
                "can't be rebuilt from user space.");
        }
        break;
    case Action::XpcCache:
        // The on-disk xpcd cache was retired starting with Yosemite
        // (10.10); no macOS release since then reads it.
        if (version.majorVersion() > 10 ||
            (version.majorVersion() == 10 && version.minorVersion() >= 10)) {
            info.available = false;
            info.reason = tr(
                "Not available on macOS 10.10 (Yosemite) or later — the "
                "XPC service cache was retired and macOS resolves "
                "services directly.");
        }
        break;
    case Action::FontCache:
    case Action::LaunchpadReset:
        break;   // supported on every macOS version Nexis targets
    }
    return info;
}

QString CacheRebuildWidget::actionTitle(Action action)
{
    switch (action) {
    case Action::DyldSharedCache: return tr("Dyld Shared Cache");
    case Action::XpcCache:        return tr("XPC Cache");
    case Action::FontCache:       return tr("Font Cache");
    case Action::LaunchpadReset:  return tr("Launchpad Layout");
    }
    return QString();
}

QString CacheRebuildWidget::actionDescription(Action action)
{
    switch (action) {
    case Action::DyldSharedCache:
        return tr("Forces a rebuild of the dyld shared cache used to speed up app launches.");
    case Action::XpcCache:
        return tr("Removes the cached XPC service lookup table so it's regenerated fresh.");
    case Action::FontCache:
        return tr("Clears the ATS font database and restarts the font server — fixes garbled "
                   "or missing fonts.");
    case Action::LaunchpadReset:
        return tr("Deletes the Launchpad layout database. macOS regenerates a default grid "
                   "on next open.");
    }
    return QString();
}

QString CacheRebuildWidget::confirmText(Action action)
{
    switch (action) {
    case Action::DyldSharedCache:
        return tr("This will rebuild the dyld shared cache. The system may feel sluggish "
                   "for a moment afterward as apps repopulate it. Continue?");
    case Action::XpcCache:
        return tr("This will remove the cached XPC service lookup table. It will be "
                   "regenerated automatically. Continue?");
    case Action::FontCache:
        return tr("This will clear the font database and restart the font server. Open apps "
                   "may need to be relaunched to see the effect. Continue?");
    case Action::LaunchpadReset:
        return tr("This will reset your Launchpad layout to the default grid — custom folders "
                   "and page arrangement will be lost. The Dock will restart. Continue?");
    }
    return QString();
}

// ── UI ───────────────────────────────────────────────────────────────────────

void CacheRebuildWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Cache Rebuilds"), this);
    title->setObjectName("cacheRebuildTitle");
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *intro = new QLabel(
        tr("Rebuild individual macOS system caches. Each action asks for confirmation, "
           "reports progress while it runs, and shows a clear success/failure result."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    for (Action action : {Action::DyldSharedCache, Action::XpcCache,
                          Action::FontCache, Action::LaunchpadReset}) {
        root->addWidget(buildRow(action));
    }

    root->addStretch();
}

QFrame *CacheRebuildWidget::buildRow(Action action)
{
    ActionRow &row = mRows[static_cast<int>(action)];

    row.card = new QFrame(this);
    row.card->setObjectName("cacheRebuildCard");
    auto *card = new QVBoxLayout(row.card);
    card->setContentsMargins(16, 16, 16, 16);
    card->setSpacing(6);

    auto *lblTitle = new QLabel(actionTitle(action), row.card);
    QFont f = lblTitle->font();
    f.setBold(true);
    lblTitle->setFont(f);
    card->addWidget(lblTitle);

    auto *lblDesc = new QLabel(actionDescription(action), row.card);
    lblDesc->setWordWrap(true);
    card->addWidget(lblDesc);

    row.lblStatus = new QLabel(row.card);
    row.lblStatus->setWordWrap(true);
    row.lblStatus->hide();
    card->addWidget(row.lblStatus);

    auto *actionsRow = new QHBoxLayout();
    actionsRow->setSpacing(8);

    row.btnRun = new QPushButton(tr("Rebuild"), row.card);
    row.btnRun->setCursor(Qt::PointingHandCursor);
    connect(row.btnRun, &QPushButton::clicked, this, [this, action] { runAction(action); });
    actionsRow->addWidget(row.btnRun);

    row.lblProgress = new QLabel(row.card);
    row.lblProgress->hide();
    actionsRow->addWidget(row.lblProgress);

    actionsRow->addStretch();

    row.lblResult = new QLabel(row.card);
    row.lblResult->setObjectName("cacheRebuildResult");
    actionsRow->addWidget(row.lblResult);

    card->addLayout(actionsRow);

    const SupportInfo support = effectiveSupport(action);
    if (!support.available) {
        row.lblStatus->setText(support.reason);
        row.lblStatus->show();
        row.btnRun->setEnabled(false);
    }

    return row.card;
}

CacheRebuildWidget::SupportInfo CacheRebuildWidget::effectiveSupport(Action action) const
{
    SupportInfo info = supportInfo(action, QOperatingSystemVersion::current());
    if (!info.available)
        return info;

    const QList<Step> steps = commandsFor(action);
    if (!steps.isEmpty() && !CommandUtil::isExecutable(steps.first().cmd)) {
        info.available = false;
        info.reason = tr("%1 is not available on this system.").arg(steps.first().cmd);
    }
    return info;
}

void CacheRebuildWidget::setRowRunning(Action action, bool running)
{
    ActionRow &row = mRows[static_cast<int>(action)];
    row.btnRun->setEnabled(!running && effectiveSupport(action).available);
    row.lblProgress->setVisible(running);
    if (!running)
        row.lblProgress->clear();
}

void CacheRebuildWidget::runAction(Action action)
{
    if (QMessageBox::question(this, actionTitle(action), confirmText(action),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    ActionRow &row = mRows[static_cast<int>(action)];
    row.lblResult->clear();
    setRowRunning(action, true);
    row.lblProgress->show();

    const QList<Step> steps = commandsFor(action);
    const int total = steps.size();

    QThreadPool::globalInstance()->start([this, action, steps, total]() {
        bool ok = true;
        QString errorMsg;

        for (int i = 0; i < steps.size() && ok; ++i) {
            const Step &s = steps.at(i);
            QMetaObject::invokeMethod(this, [this, action, i, total, s]() {
                mRows[static_cast<int>(action)].lblProgress->setText(
                    tr("Step %1 of %2: %3").arg(i + 1).arg(total).arg(s.description));
            }, Qt::QueuedConnection);

            const ExecResult r = s.needsSudo
                ? CommandUtil::sudoExecWithStatus(s.cmd, s.args, QByteArray(), 300000)
                : CommandUtil::execWithStatus(s.cmd, s.args, 60000);
            if (!r.ok()) {
                ok = false;
                errorMsg = r.error.isEmpty() ? r.output : r.error;
            }
        }

        QMetaObject::invokeMethod(this, [this, action, ok, errorMsg]() {
            setRowRunning(action, false);
            ActionRow &row = mRows[static_cast<int>(action)];
            if (ok) {
                row.lblResult->setText(tr("✓ Done"));
            } else {
                row.lblResult->setText(tr("⚠ Failed — %1")
                    .arg(errorMsg.isEmpty() ? tr("did you cancel the password prompt?") : errorMsg));
            }
        }, Qt::QueuedConnection);
    });
}

void CacheRebuildWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg     = sv->value("@cardBg").toString();
    const QString border     = sv->value("@borderColor").toString();
    const QString successCol = sv->value("@successColor").toString();
    const QString warnCol    = sv->value("@warningColor").toString();

    const QString cardCss = QString(
        "QFrame#cacheRebuildCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}").arg(cardBg, border);

    for (const ActionRow &row : std::as_const(mRows)) {
        if (!row.card)
            continue;
        row.card->setStyleSheet(cardCss);

        const QString resultText = row.lblResult->text();
        if (resultText.startsWith(QStringLiteral("✓")))
            row.lblResult->setStyleSheet(QString("color: %1;").arg(successCol));
        else if (resultText.startsWith(QStringLiteral("⚠")))
            row.lblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
        else
            row.lblResult->setStyleSheet(QString());
    }
}
