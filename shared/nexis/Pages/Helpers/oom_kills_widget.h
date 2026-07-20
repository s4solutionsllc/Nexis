#ifndef OOM_KILLS_WIDGET_H
#define OOM_KILLS_WIDGET_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <Info/oomd_snapshot.h>

// FW-11 (SSO-3739): OOM-kill observability panel. Shows systemd-oomd state +
// cumulative kill counts + a short list of recent kills (unit, cgroup, reason).
//
// Subscribes to DataRefreshService::Signal::Oomd via the parent page. Hides
// itself entirely on hosts without any oomd / cgroup v2 signal so the panel
// only ever appears when there is something to show.
class OomKillsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OomKillsWidget(QWidget *parent = nullptr);

    // DS §2/§3 (NEX-Resources, SSO-14437): opt this card into the shared
    // elevated-card + accent-bar header recipe (mirrors
    // HistoryChart::setElevated() — [cardRole="elevated"] fill/border on
    // mCard, reveals + colors #sectionHeaderAccent, 90/26 drop shadow).
    // Card content (title/intro/state/totals/events) is unchanged; only the
    // chrome around it is affected.
    void setElevated(const QString &accentToken);

public slots:
    void onOomdUpdated(const OomdSnapshot &snap);

private slots:
    void refreshThemeColors();

private:
    void buildUI();
    void renderEmptyEvents();
    QString formatTimestamp(const QDateTime &when) const;

    QFrame      *mCard         = nullptr;
    QFrame      *mAccentBar    = nullptr;
    QLabel      *mTitle        = nullptr;
    QLabel      *mState        = nullptr;
    QLabel      *mTotals       = nullptr;
    QLabel      *mDefensiveTip = nullptr;
    QVBoxLayout *mEventsLayout = nullptr;
    QLabel      *mEventsEmpty  = nullptr;
};

#endif // OOM_KILLS_WIDGET_H
