#ifndef GNOME_WM_TAB_H
#define GNOME_WM_TAB_H

#include <QWidget>

namespace Ui {
    class GnomeWmTab;
}

class GnomeWmTab : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeWmTab(QWidget *parent = nullptr);
    ~GnomeWmTab();

signals:
    void settingFailed(const QString &message);

private:
    void loadSettings();

    Ui::GnomeWmTab *ui;
    bool mLoading;
};

#endif // GNOME_WM_TAB_H
