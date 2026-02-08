#ifndef GNOME_APPEARANCE_TAB_H
#define GNOME_APPEARANCE_TAB_H

#include <QWidget>

namespace Ui {
    class GnomeAppearanceTab;
}

class GnomeAppearanceTab : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeAppearanceTab(QWidget *parent = nullptr);
    ~GnomeAppearanceTab();

private:
    void loadSettings();

    Ui::GnomeAppearanceTab *ui;
    bool mLoading;
};

#endif // GNOME_APPEARANCE_TAB_H
