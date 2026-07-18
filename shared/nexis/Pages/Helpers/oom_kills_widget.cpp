#include "oom_kills_widget.h"

#include "signal_mapper.h"
#include "utilities.h"
#include <Managers/app_manager.h>

#include <QHBoxLayout>
#include <QLocale>
#include <QStyle>

namespace {

QString eventLine(const OomdEvent &ev, const QLocale &locale)
{
    QString head = ev.unit.isEmpty() ? QObject::tr("(unknown unit)") : ev.unit;
    QString reason = ev.reason.isEmpty() ? QObject::tr("unknown reason")
                                         : ev.reason;
    QString count = QObject::tr("%n task(s)", nullptr, ev.tasksKilled);
    QString when = ev.when.isValid() ? locale.toString(ev.when,
                                                       QLocale::ShortFormat)
                                     : QString();
    if (when.isEmpty())
        return QObject::tr("%1 — %2 — %3").arg(head, count, reason);
    return QObject::tr("%1 — %2 — %3 — %4").arg(when, head, count, reason);
}

} // namespace

OomKillsWidget::OomKillsWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &OomKillsWidget::refreshThemeColors);
    refreshThemeColors();

    // Start hidden — the parent reveals us after the first snapshot lands
    // with `available == true`. Avoids a flash of empty card on cold start.
    hide();
}

void OomKillsWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mCard = new QFrame(this);
    mCard->setObjectName("oomCard");
    root->addWidget(mCard);

    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 14, 16, 14);
    card->setSpacing(8);

    // DS §3 accent bar (style.qss "Section Header" recipe, NEX F2) — hidden
    // until setElevated() reveals + colors it, mirrors HistoryChart's
    // sectionHeaderAccent usage.
    mAccentBar = new QFrame(mCard);
    mAccentBar->setObjectName("sectionHeaderAccent");
    mAccentBar->setFixedWidth(3);
    mAccentBar->setFrameShape(QFrame::NoFrame);
    mAccentBar->setVisible(false);

    mTitle = new QLabel(tr("Out-of-Memory Kills"), mCard);
    mTitle->setObjectName("oomTitle");
    QFont titleFont = mTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    mTitle->setFont(titleFont);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);
    titleRow->addWidget(mAccentBar);
    titleRow->addWidget(mTitle);
    titleRow->addStretch();
    card->addLayout(titleRow);

    auto *intro = new QLabel(
        tr("systemd-oomd terminates processes before the kernel does when "
           "memory pressure spikes. This panel shows its current state and "
           "the most recent things it killed."),
        mCard);
    intro->setWordWrap(true);
    intro->setObjectName("oomIntro");
    card->addWidget(intro);

    mState = new QLabel(mCard);
    mState->setObjectName("oomState");
    mState->setWordWrap(true);
    card->addWidget(mState);

    mTotals = new QLabel(mCard);
    mTotals->setObjectName("oomTotals");
    mTotals->setWordWrap(true);
    card->addWidget(mTotals);

    mDefensiveTip = new QLabel(mCard);
    mDefensiveTip->setObjectName("oomDefensiveTip");
    mDefensiveTip->setWordWrap(true);
    mDefensiveTip->hide();
    card->addWidget(mDefensiveTip);

    auto *eventsHeader = new QLabel(tr("Recent kills"), mCard);
    eventsHeader->setObjectName("oomEventsHeader");
    QFont headerFont = eventsHeader->font();
    headerFont.setBold(true);
    eventsHeader->setFont(headerFont);
    card->addWidget(eventsHeader);

    auto *eventsHolder = new QWidget(mCard);
    mEventsLayout = new QVBoxLayout(eventsHolder);
    mEventsLayout->setContentsMargins(0, 0, 0, 0);
    mEventsLayout->setSpacing(2);
    card->addWidget(eventsHolder);

    mEventsEmpty = new QLabel(tr("No OOM kills recorded since the last journal rotation."),
                              mCard);
    mEventsEmpty->setObjectName("oomEventsEmpty");
    mEventsEmpty->setWordWrap(true);
    card->addWidget(mEventsEmpty);
}

void OomKillsWidget::renderEmptyEvents()
{
    while (QLayoutItem *item = mEventsLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

QString OomKillsWidget::formatTimestamp(const QDateTime &when) const
{
    if (!when.isValid())
        return {};
    return QLocale().toString(when, QLocale::ShortFormat);
}

void OomKillsWidget::onOomdUpdated(const OomdSnapshot &snap)
{
    if (!snap.available) {
        hide();
        return;
    }
    show();

    // State line
    QString state;
    if (snap.loadState.isEmpty() && snap.activeState.isEmpty()) {
        state = tr("systemd-oomd is not installed on this host.");
    } else if (snap.loadState == QLatin1String("masked")) {
        state = tr("systemd-oomd is masked — the kernel OOM killer is the only "
                   "line of defence.");
    } else if (snap.loadState == QLatin1String("not-found")) {
        state = tr("systemd-oomd is not present in this systemd build.");
    } else if (snap.activeState == QLatin1String("active")) {
        state = tr("systemd-oomd: active.");
    } else if (snap.activeState == QLatin1String("inactive")) {
        state = tr("systemd-oomd: installed but inactive — start it with "
                   "<code>systemctl enable --now systemd-oomd</code> to get "
                   "early intervention.");
    } else if (snap.activeState == QLatin1String("failed")) {
        state = tr("systemd-oomd: failed — check <code>journalctl -u "
                   "systemd-oomd</code>.");
    } else {
        state = tr("systemd-oomd: %1 (%2).").arg(snap.activeState, snap.loadState);
    }
    mState->setText(state);

    // Totals line — show both oomd-attributed and kernel-attributed counts
    // so the user can tell what happened on hosts without oomd.
    QStringList totalsParts;
    totalsParts << tr("Total kills (oomd): <b>%1</b>")
                   .arg(QLocale().toString(qlonglong(snap.oomKills)));
    totalsParts << tr("Managed (oomd policy): <b>%1</b>")
                   .arg(QLocale().toString(qlonglong(snap.managedOomKills)));
    totalsParts << tr("Kernel oom_kill (cgroup v2): <b>%1</b>")
                   .arg(QLocale().toString(qlonglong(snap.systemOomKill)));
    mTotals->setText(totalsParts.join(QStringLiteral("  ·  ")));

    // Defensive tip — only surface the v1-is-gone reminder on a host where
    // the unified hierarchy *isn't* visible (rare on the 26.04 baseline).
    if (!snap.cgroupV2) {
        mDefensiveTip->setText(
            tr("⚠ This host does not expose the cgroup v2 unified hierarchy. "
               "systemd 259+ (Ubuntu 26.04) requires v2; per-process and "
               "memory.events readings will be unavailable."));
        mDefensiveTip->show();
    } else {
        mDefensiveTip->hide();
    }

    // Render events list
    renderEmptyEvents();
    if (snap.recentEvents.isEmpty()) {
        mEventsEmpty->show();
    } else {
        mEventsEmpty->hide();
        for (const OomdEvent &ev : snap.recentEvents) {
            auto *row = new QLabel(eventLine(ev, QLocale()), mCard);
            row->setObjectName("oomEventRow");
            row->setWordWrap(true);
            row->setToolTip(ev.cgroupPath);
            mEventsLayout->addWidget(row);
        }
    }
}

void OomKillsWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv || !mCard)
        return;

    const QString secondary = sv->value("@color04").toString();
    const QString warnCol   = sv->value("@warningColor").toString();

    // mCard's background/border/radius come from the global QSS
    // [cardRole="elevated"] recipe (set in setElevated()) — no per-widget
    // setStyleSheet() here (DS §9 item 1, QSS-first).

    if (mState)
        mState->setStyleSheet(QString());
    if (mTotals)
        mTotals->setStyleSheet(QString());
    if (mDefensiveTip)
        mDefensiveTip->setStyleSheet(QString("color: %1;").arg(warnCol));
    if (mEventsEmpty)
        mEventsEmpty->setStyleSheet(QString("color: %1;").arg(secondary));
}

void OomKillsWidget::setElevated(const QString &accentToken)
{
    mCard->setAttribute(Qt::WA_StyledBackground, true);
    mCard->setProperty("cardRole", "elevated");
    mCard->style()->unpolish(mCard);
    mCard->style()->polish(mCard);

    mAccentBar->setProperty("accentToken", accentToken);
    mAccentBar->setVisible(true);
    mAccentBar->style()->unpolish(mAccentBar);
    mAccentBar->style()->polish(mAccentBar);

    Utilities::addDropShadow(mCard, 90, 26);
}
