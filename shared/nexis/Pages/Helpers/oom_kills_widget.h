#ifndef OOM_KILLS_WIDGET_H
#define OOM_KILLS_WIDGET_H

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <Info/oomd_snapshot.h>

// FW-11 (SSO-3739), folded into a compact status line by SSO-26719: shows
// systemd-oomd state + cumulative kill count on one line ("systemd-oomd
// active · 0 kills"), expanding to the recent-kills list only when there is
// one. Lives in the Resources page's Memory card extras row rather than its
// own card. Hides itself entirely on hosts without any oomd / cgroup v2
// signal so the line only ever appears when there is something to show.
class OomKillsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OomKillsWidget(QWidget *parent = nullptr);

public slots:
    void onOomdUpdated(const OomdSnapshot &snap);

private slots:
    void refreshThemeColors();

private:
    void buildUI();
    void renderEmptyEvents();

    QLabel      *mStatusLine   = nullptr;
    QLabel      *mDefensiveTip = nullptr;
    QLabel      *mEventsHeader = nullptr;
    QWidget     *mEventsHolder = nullptr;
    QVBoxLayout *mEventsLayout = nullptr;
};

#endif // OOM_KILLS_WIDGET_H
