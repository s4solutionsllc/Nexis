#ifndef GNOME_DESKTOP_TAB_H
#define GNOME_DESKTOP_TAB_H

#include <QWidget>

namespace Ui {
    class GnomeDesktopTab;
}

class GnomeDesktopTab : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeDesktopTab(QWidget *parent = nullptr);
    ~GnomeDesktopTab();

private:
    void loadSettings();

    Ui::GnomeDesktopTab *ui;
    bool mLoading;
};

#endif // GNOME_DESKTOP_TAB_H
