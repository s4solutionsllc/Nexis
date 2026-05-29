#ifndef DISK_USAGE_LAUNCHER_WIDGET_H
#define DISK_USAGE_LAUNCHER_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class AppManager;
class SignalMapper;
class SettingManager;
class ToolManager;

class DiskUsageLauncherWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiskUsageLauncherWidget(QWidget *parent = nullptr,
                                     AppManager *appManager = nullptr,
                                     SignalMapper *signalMapper = nullptr,
                                     SettingManager *settingManager = nullptr,
                                     ToolManager *toolManager = nullptr);

private slots:
    void onActionClicked();

private:
    /// The resolved tool the widget will display and act on.
    enum State {
        // Auto-detected or user-selected known tools — installed
        LAUNCH_BAOBAB,
        LAUNCH_FILELIGHT,
        LAUNCH_QDIRSTAT,
        LAUNCH_NCDU,
#ifdef Q_OS_MACOS
        LAUNCH_GRANDPERSPECTIVE,
        LAUNCH_DAISYDISK,
        LAUNCH_OMNIDISKSWEEPER,
#endif
        LAUNCH_CUSTOM,

        // Auto-detected or user-selected known tools — not installed
        INSTALL_BAOBAB,
        INSTALL_FILELIGHT_FLATPAK,
        NO_FLATPAK,
#ifdef Q_OS_MACOS
        LINK_GRANDPERSPECTIVE,
        LINK_DAISYDISK,
        LINK_OMNIDISKSWEEPER,
#endif
        NOT_FOUND,   ///< Selected tool not found on the system
        NO_TOOL       ///< Unsupported platform / nothing available
    };

    void detect();
    bool resolveNamedTool(const QString &toolKey);
    void autoDetect();
    void updateUi();
    void applyThemeColors();

    State mState;
    QString mCustomDisplayName;
    QString mLaunchCmd;
    QStringList mLaunchArgs;

    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    SettingManager *mSettingManager;
    ToolManager *mToolManager;

    QLabel *mTitleLabel;
    QLabel *mToolNameLabel;
    QLabel *mDescriptionLabel;
    QLabel *mStatusLabel;
    QPushButton *mActionButton;
};

#endif // DISK_USAGE_LAUNCHER_WIDGET_H
