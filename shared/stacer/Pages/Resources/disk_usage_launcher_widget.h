#ifndef DISK_USAGE_LAUNCHER_WIDGET_H
#define DISK_USAGE_LAUNCHER_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class DiskUsageLauncherWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiskUsageLauncherWidget(QWidget *parent = nullptr);

private slots:
    void onActionClicked();

private:
    enum State {
        LAUNCH_BAOBAB,
        INSTALL_BAOBAB,
        LAUNCH_FILELIGHT,
        INSTALL_FILELIGHT_FLATPAK,
        NO_FLATPAK,
#ifdef Q_OS_MACOS
        LAUNCH_GRANDPERSPECTIVE,
        LINK_GRANDPERSPECTIVE,
#endif
        NO_TOOL
    };

    void detect();
    void updateUi();
    void applyThemeColors();

    State mState;

    QLabel *mTitleLabel;
    QLabel *mToolNameLabel;
    QLabel *mDescriptionLabel;
    QLabel *mStatusLabel;
    QPushButton *mActionButton;
};

#endif // DISK_USAGE_LAUNCHER_WIDGET_H
