#ifndef GNOME_MOUSE_TAB_H
#define GNOME_MOUSE_TAB_H

#include <QWidget>

namespace Ui {
    class GnomeMouseTab;
}

class GnomeMouseTab : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeMouseTab(QWidget *parent = nullptr);
    ~GnomeMouseTab();

private:
    void loadSettings();

    Ui::GnomeMouseTab *ui;
    bool mLoading;
};

#endif // GNOME_MOUSE_TAB_H
