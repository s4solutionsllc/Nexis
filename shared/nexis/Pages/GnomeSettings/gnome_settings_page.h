#ifndef GNOME_SETTINGS_PAGE_H
#define GNOME_SETTINGS_PAGE_H

#include <QWidget>

#include "gnome_appearance_tab.h"
#include "gnome_wm_tab.h"
#include "gnome_mouse_tab.h"
#include "gnome_desktop_tab.h"

namespace Ui {
    class GnomeSettingsPage;
}

class GnomeSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeSettingsPage(QWidget *parent = nullptr);
    ~GnomeSettingsPage();

public slots:
    void showError(const QString &message);

private slots:
    void onTabButtonClicked(int index);

private:
    void init();

    Ui::GnomeSettingsPage *ui;

    GnomeAppearanceTab *mAppearanceTab;
    GnomeWmTab *mWmTab;
    GnomeMouseTab *mMouseTab;
    GnomeDesktopTab *mDesktopTab;
};

#endif // GNOME_SETTINGS_PAGE_H
