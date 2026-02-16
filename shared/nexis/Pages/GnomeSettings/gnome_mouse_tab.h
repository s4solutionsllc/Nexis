#ifndef GNOME_MOUSE_TAB_H
#define GNOME_MOUSE_TAB_H

#include <QWidget>

class QTimer;

namespace Ui {
    class GnomeMouseTab;
}

class GnomeMouseTab : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeMouseTab(QWidget *parent = nullptr);
    ~GnomeMouseTab();

signals:
    void settingFailed(const QString &message);

private:
    void loadSettings();

    Ui::GnomeMouseTab *ui;
    bool mLoading;
    QTimer *mMouseSpeedTimer;
    QTimer *mTouchpadSpeedTimer;
};

#endif // GNOME_MOUSE_TAB_H
