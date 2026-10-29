#include "oom_kills_widget.h"

#include "signal_mapper.h"
#include <Managers/app_manager.h>

#include <QLocale>

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
    // with `available == true`. Avoids a flash of an empty line on cold start.
    hide();
}

void OomKillsWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    mStatusLine = new QLabel(this);
    mStatusLine->setObjectName("oomStatusLine");
    mStatusLine->setWordWrap(true);
    root->addWidget(mStatusLine);

    mDefensiveTip = new QLabel(this);
    mDefensiveTip->setObjectName("oomDefensiveTip");
    mDefensiveTip->setWordWrap(true);
    mDefensiveTip->hide();
    root->addWidget(mDefensiveTip);

    mEventsHeader = new QLabel(tr("Recent kills"), this);
    mEventsHeader->setObjectName("oomEventsHeader");
    QFont headerFont = mEventsHeader->font();
    headerFont.setBold(true);
    mEventsHeader->setFont(headerFont);
    mEventsHeader->hide();
    root->addWidget(mEventsHeader);

    mEventsHolder = new QWidget(this);
    mEventsLayout = new QVBoxLayout(mEventsHolder);
    mEventsLayout->setContentsMargins(0, 0, 0, 0);
    mEventsLayout->setSpacing(2);
    mEventsHolder->hide();
    root->addWidget(mEventsHolder);
}

void OomKillsWidget::renderEmptyEvents()
{
    while (QLayoutItem *item = mEventsLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

void OomKillsWidget::onOomdUpdated(const OomdSnapshot &snap)
{
    if (!snap.available) {
        hide();
        return;
    }
    show();

    // Status line — one sentence covering both whether oomd is the one
    // intervening and how many times it (or the kernel) has.
    QString state;
    if (snap.loadState.isEmpty() && snap.activeState.isEmpty())
        state = tr("systemd-oomd not installed");
    else if (snap.loadState == QLatin1String("masked"))
        state = tr("systemd-oomd masked");
    else if (snap.loadState == QLatin1String("not-found"))
        state = tr("systemd-oomd not present");
    else if (snap.activeState == QLatin1String("active"))
        state = tr("systemd-oomd active");
    else if (snap.activeState == QLatin1String("inactive"))
        state = tr("systemd-oomd inactive");
    else if (snap.activeState == QLatin1String("failed"))
        state = tr("systemd-oomd failed");
    else
        state = tr("systemd-oomd: %1").arg(snap.activeState);

    QString line = tr("%1 · %2 kills").arg(state).arg(QLocale().toString(qlonglong(snap.oomKills)));
    if (snap.systemOomKill > 0)
        line += tr(" (kernel: %1)").arg(QLocale().toString(qlonglong(snap.systemOomKill)));
    mStatusLine->setText(line);

    // Defensive tip — only surface the v1-is-gone reminder on a host where
    // the unified hierarchy *isn't* visible (rare on the 26.04 baseline).
    if (!snap.cgroupV2) {
        mDefensiveTip->setText(
            tr("⚠ No cgroup v2 unified hierarchy — systemd 259+ (Ubuntu "
               "26.04) needs it; per-process OOM readings are unavailable."));
        mDefensiveTip->show();
    } else {
        mDefensiveTip->hide();
    }

    // Expand to the kill list only when there is one.
    renderEmptyEvents();
    if (snap.recentEvents.isEmpty()) {
        mEventsHeader->hide();
        mEventsHolder->hide();
    } else {
        mEventsHeader->show();
        mEventsHolder->show();
        for (const OomdEvent &ev : snap.recentEvents) {
            auto *row = new QLabel(eventLine(ev, QLocale()), mEventsHolder);
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
    if (!sv)
        return;

    const QString warnCol = sv->value("@warningText").toString();
    if (mDefensiveTip)
        mDefensiveTip->setStyleSheet(QString("color: %1;").arg(warnCol));
}
