#include "disk_usage_launcher_widget.h"
#include "disk_treemap_dialog.h"
#include "dpi.h"
#include "utilities.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStyle>
#include <QtConcurrent>

#ifdef Q_OS_MACOS
#include <QDir>
#endif

// ---------------------------------------------------------------------------
// findDesktopApp — locate a GUI app installed natively or via Flatpak.
//
// Tries in order:
//   1. Native binary in PATH (e.g. "baobab")
//   2. Flatpak app-ID wrapper in PATH (e.g. "org.gnome.baobab")
//   3. Flatpak app-ID wrapper on disk but not in PATH (AppImage non-login shell)
//
// Returns {launchCmd, launchArgs} ready to pass to QProcess::startDetached(),
// or an empty launchCmd if not found.
// ---------------------------------------------------------------------------
namespace {
struct AppLocation {
    QString launchCmd;
    QStringList launchArgs;
    bool isFound() const { return !launchCmd.isEmpty(); }
};

AppLocation findDesktopApp(const QString &nativeBin, const QString &flatpakId)
{
    if (!QStandardPaths::findExecutable(nativeBin).isEmpty())
        return {nativeBin, {}};

    if (!QStandardPaths::findExecutable(flatpakId).isEmpty())
        return {QStringLiteral("flatpak"), {QStringLiteral("run"), flatpakId}};

    const QStringList exportPaths = {
        QDir::homePath() + "/.local/share/flatpak/exports/bin/" + flatpakId,
        "/var/lib/flatpak/exports/bin/" + flatpakId,
    };
    for (const QString &p : exportPaths) {
        if (QFileInfo(p).isExecutable())
            return {QStringLiteral("flatpak"), {QStringLiteral("run"), flatpakId}};
    }

    return {};
}
} // namespace

#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"
#include "Utils/command_util.h"
#include "Tools/package_tool_shared.h"
#include "Managers/tool_manager.h"

DiskUsageLauncherWidget::DiskUsageLauncherWidget(QWidget *parent,
                                                   AppManager *appManager,
                                                   SignalMapper *signalMapper,
                                                   SettingManager *settingManager,
                                                   ToolManager *toolManager)
    : QWidget(parent),
      mState(NO_TOOL),
      mAppManager(appManager ? appManager : AppManager::ins()),
      mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
      mSettingManager(settingManager ? settingManager : SettingManager::ins()),
      mToolManager(toolManager ? toolManager : ToolManager::ins())
{
    // --- Title (matches HistoryChart's lblHistoryTitle) ---
    mTitleLabel = new QLabel(tr("Disk Usage Analysis"), this);
    mTitleLabel->setObjectName("lblHistoryTitle");

    // DS §3 accent bar (style.qss "Section Header" recipe, NEX F2) — hidden
    // until setElevated() reveals + colors it, mirrors HistoryChart's
    // sectionHeaderAccent usage.
    mAccentBar = new QFrame(this);
    mAccentBar->setObjectName("sectionHeaderAccent");
    mAccentBar->setFixedWidth(3);
    mAccentBar->setFrameShape(QFrame::NoFrame);
    mAccentBar->setVisible(false);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);
    titleRow->addWidget(mAccentBar);
    titleRow->addWidget(mTitleLabel);
    titleRow->addStretch();

    // --- Icon + info area ---
    mToolNameLabel = new QLabel(this);
    QFont nameFont = mToolNameLabel->font();
    nameFont.setPointSize(11);
    nameFont.setBold(true);
    mToolNameLabel->setFont(nameFont);

    mDescriptionLabel = new QLabel(this);
    mDescriptionLabel->setWordWrap(true);

    mStatusLabel = new QLabel(this);

    auto *infoLayout = new QVBoxLayout;
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->addWidget(mToolNameLabel);
    infoLayout->addWidget(mDescriptionLabel);
    infoLayout->addWidget(mStatusLabel);

    auto *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(Dpi::scale(48, 48));
    iconLabel->setScaledContents(true);

    auto *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(Dpi::scale(12), 0, Dpi::scale(12), 0);
    contentLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    contentLayout->addLayout(infoLayout, 1);

    // --- Action button ---
    mActionButton = new QPushButton(this);
    mActionButton->setAccessibleName("primary");
    mActionButton->setCursor(Qt::PointingHandCursor);
    connect(mActionButton, &QPushButton::clicked, this, &DiskUsageLauncherWidget::onActionClicked);

    // SSO-3737 / FW-09: built-in treemap entry point. Sits next to the
    // external-tool launcher so DaisyDisk/Baobab users can still launch
    // their preferred tool, but we now offer something out of the box.
    mBuiltinButton = new QPushButton(tr("Built-in Treemap"), this);
    mBuiltinButton->setCursor(Qt::PointingHandCursor);
    connect(mBuiltinButton, &QPushButton::clicked,
            this, &DiskUsageLauncherWidget::onBuiltinClicked);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(Dpi::scale(12), 0, Dpi::scale(12), Dpi::scale(12));
    buttonLayout->addStretch();
    buttonLayout->addWidget(mBuiltinButton);
    buttonLayout->addWidget(mActionButton);
    buttonLayout->addStretch();

    // --- Main layout ---
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(Dpi::scale(12), Dpi::scale(6), Dpi::scale(12), 0);
    mainLayout->setSpacing(Dpi::scale(8));
    mainLayout->addLayout(titleRow);
    mainLayout->addLayout(contentLayout);
    mainLayout->addLayout(buttonLayout);

    // Detect platform/tool and update UI
    detect();
    updateUi();

    // Set the icon after detection so we know which tool
    QIcon toolIcon;
    if (mState == LAUNCH_BAOBAB || mState == INSTALL_BAOBAB)
        toolIcon = QIcon(":/static/themes/common/img/disk-baobab.svg");
    else if (mState == LAUNCH_FILELIGHT || mState == INSTALL_FILELIGHT_FLATPAK)
        toolIcon = QIcon(":/static/themes/common/img/disk-filelight.svg");
    else if (mState == LAUNCH_QDIRSTAT)
        toolIcon = QIcon(":/static/themes/common/img/disk-qdirstat.svg");
    else
        toolIcon = QIcon(":/static/themes/common/img/disk-harddisk.svg");
    if (!toolIcon.isNull())
        iconLabel->setPixmap(toolIcon.pixmap(Dpi::scale(48), Dpi::scale(48)));
    else
        iconLabel->hide();

    // Theme change support
    applyThemeColors();
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &DiskUsageLauncherWidget::applyThemeColors);
}

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

void DiskUsageLauncherWidget::detect()
{
    QString pref = mSettingManager->getDiskAnalyzerTool();

    if (pref.isEmpty() || pref == "auto") {
        autoDetect();
    } else {
        if (!resolveNamedTool(pref)) {
            autoDetect();
        }
    }
}

bool DiskUsageLauncherWidget::resolveNamedTool(const QString &toolKey)
{
    // --- Custom executable ---
    if (toolKey == "custom") {
        QString customPath = mSettingManager->getDiskAnalyzerCustomPath();
        if (customPath.isEmpty()) {
            mState = NOT_FOUND;
            mCustomDisplayName = tr("Custom");
            return true;
        }
        bool found = !QStandardPaths::findExecutable(customPath).isEmpty()
                     || QFileInfo(customPath).isExecutable();
        if (found) {
            mState = LAUNCH_CUSTOM;
            mCustomDisplayName = QFileInfo(customPath).fileName();
        } else {
            mState = NOT_FOUND;
            mCustomDisplayName = QFileInfo(customPath).fileName();
        }
        return true;
    }

    // --- Known Linux tools ---
    if (toolKey == "baobab") {
        AppLocation loc = findDesktopApp("baobab", "org.gnome.baobab");
        if (loc.isFound()) {
            mLaunchCmd = loc.launchCmd; mLaunchArgs = loc.launchArgs;
            mState = LAUNCH_BAOBAB;
        } else {
            mState = INSTALL_BAOBAB;
        }
        return true;
    }
    if (toolKey == "filelight") {
        AppLocation loc = findDesktopApp("filelight", "org.kde.filelight");
        if (loc.isFound()) {
            mLaunchCmd = loc.launchCmd; mLaunchArgs = loc.launchArgs;
            mState = LAUNCH_FILELIGHT;
        } else if (!QStandardPaths::findExecutable("flatpak").isEmpty()) {
            mState = INSTALL_FILELIGHT_FLATPAK;
        } else {
            mState = NO_FLATPAK;
        }
        return true;
    }
    if (toolKey == "qdirstat") {
        AppLocation loc = findDesktopApp("qdirstat", "io.github.jeena.qdirstat");
        if (loc.isFound()) {
            mLaunchCmd = loc.launchCmd; mLaunchArgs = loc.launchArgs;
            mState = LAUNCH_QDIRSTAT;
        } else {
            mState = NOT_FOUND;
        }
        return true;
    }
    if (toolKey == "ncdu") {
        if (!QStandardPaths::findExecutable("ncdu").isEmpty())
            mState = LAUNCH_NCDU;
        else
            mState = NOT_FOUND;
        return true;
    }

#ifdef Q_OS_MACOS
    // --- Known macOS tools ---
    if (toolKey == "grandperspective") {
        if (QFileInfo("/Applications/GrandPerspective.app").exists() ||
            QFileInfo(QDir::homePath() + "/Applications/GrandPerspective.app").exists())
            mState = LAUNCH_GRANDPERSPECTIVE;
        else
            mState = LINK_GRANDPERSPECTIVE;
        return true;
    }
    if (toolKey == "daisydisk") {
        if (QFileInfo("/Applications/DaisyDisk.app").exists() ||
            QFileInfo(QDir::homePath() + "/Applications/DaisyDisk.app").exists())
            mState = LAUNCH_DAISYDISK;
        else
            mState = LINK_DAISYDISK;
        return true;
    }
    if (toolKey == "omnidisksweeper") {
        if (QFileInfo("/Applications/OmniDiskSweeper.app").exists() ||
            QFileInfo(QDir::homePath() + "/Applications/OmniDiskSweeper.app").exists())
            mState = LAUNCH_OMNIDISKSWEEPER;
        else
            mState = LINK_OMNIDISKSWEEPER;
        return true;
    }
#endif

    return false; // unknown key
}

void DiskUsageLauncherWidget::autoDetect()
{
#ifdef Q_OS_LINUX
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toUpper();
    bool isGnome = desktop.contains("GNOME");

    if (isGnome) {
        AppLocation loc = findDesktopApp("baobab", "org.gnome.baobab");
        if (loc.isFound()) {
            mLaunchCmd = loc.launchCmd; mLaunchArgs = loc.launchArgs;
            mState = LAUNCH_BAOBAB;
        } else {
            mState = INSTALL_BAOBAB;
        }
    } else {
        AppLocation loc = findDesktopApp("filelight", "org.kde.filelight");
        if (loc.isFound()) {
            mLaunchCmd = loc.launchCmd; mLaunchArgs = loc.launchArgs;
            mState = LAUNCH_FILELIGHT;
        } else if (!QStandardPaths::findExecutable("flatpak").isEmpty()) {
            mState = INSTALL_FILELIGHT_FLATPAK;
        } else {
            mState = NO_FLATPAK;
        }
    }
#elif defined(Q_OS_MACOS)
    if (QFileInfo("/Applications/GrandPerspective.app").exists() ||
        QFileInfo(QDir::homePath() + "/Applications/GrandPerspective.app").exists())
        mState = LAUNCH_GRANDPERSPECTIVE;
    else
        mState = LINK_GRANDPERSPECTIVE;
#else
    mState = NO_TOOL;
#endif
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void DiskUsageLauncherWidget::updateUi()
{
    switch (mState) {
    // ---- Launchable tools ----
    case LAUNCH_BAOBAB:
        mToolNameLabel->setText(tr("Disk Usage Analyzer (Baobab)"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a graphical treemap."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch Disk Usage Analyzer"));
        break;
    case LAUNCH_FILELIGHT:
        mToolNameLabel->setText(tr("Filelight"));
        mDescriptionLabel->setText(tr("Visualize disk usage with interactive sunburst charts."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch Filelight"));
        break;
    case LAUNCH_QDIRSTAT:
        mToolNameLabel->setText(tr("QDirStat"));
        mDescriptionLabel->setText(tr("Qt-based directory statistics with treemap visualization."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch QDirStat"));
        break;
    case LAUNCH_NCDU:
        mToolNameLabel->setText(tr("ncdu"));
        mDescriptionLabel->setText(tr("NCurses disk usage analyzer. Opens in a terminal."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch ncdu"));
        break;
#ifdef Q_OS_MACOS
    case LAUNCH_GRANDPERSPECTIVE:
        mToolNameLabel->setText(tr("GrandPerspective"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a treemap."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch GrandPerspective"));
        break;
    case LAUNCH_DAISYDISK:
        mToolNameLabel->setText(tr("DaisyDisk"));
        mDescriptionLabel->setText(tr("Visualize disk usage with an interactive sunburst chart."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch DaisyDisk"));
        break;
    case LAUNCH_OMNIDISKSWEEPER:
        mToolNameLabel->setText(tr("OmniDiskSweeper"));
        mDescriptionLabel->setText(tr("Quickly find and delete large files."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch OmniDiskSweeper"));
        break;
#endif
    case LAUNCH_CUSTOM: {
        mToolNameLabel->setText(mCustomDisplayName);
        mDescriptionLabel->setText(tr("User-configured disk usage analyzer."));
        mStatusLabel->setText(tr("Status: Installed"));
                mActionButton->setText(tr("Launch %1").arg(mCustomDisplayName));
        break;
    }

    // ---- Installable / link tools ----
    case INSTALL_BAOBAB:
        mToolNameLabel->setText(tr("Disk Usage Analyzer (Baobab)"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a graphical treemap. "
                                      "Baobab is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->setText(tr("Install Disk Usage Analyzer"));
        break;
    case INSTALL_FILELIGHT_FLATPAK:
        mToolNameLabel->setText(tr("Filelight"));
        mDescriptionLabel->setText(tr("Visualize disk usage with interactive sunburst charts. "
                                      "Filelight is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->setText(tr("Install Filelight"));
        break;
    case NO_FLATPAK:
        mToolNameLabel->setText(tr("Filelight"));
        mDescriptionLabel->setText(tr("Visualize disk usage with interactive sunburst charts. "
                                      "Flatpak is required to install Filelight on this system."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->setText(tr("Install Flatpak"));
        break;
#ifdef Q_OS_MACOS
    case LINK_GRANDPERSPECTIVE:
        mToolNameLabel->setText(tr("GrandPerspective"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a treemap. "
                                      "GrandPerspective is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->setText(tr("Get GrandPerspective"));
        break;
    case LINK_DAISYDISK:
        mToolNameLabel->setText(tr("DaisyDisk"));
        mDescriptionLabel->setText(tr("Visualize disk usage with an interactive sunburst chart. "
                                      "DaisyDisk is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->setText(tr("Get DaisyDisk"));
        break;
    case LINK_OMNIDISKSWEEPER:
        mToolNameLabel->setText(tr("OmniDiskSweeper"));
        mDescriptionLabel->setText(tr("Quickly find and delete large files. "
                                      "OmniDiskSweeper is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->setText(tr("Get OmniDiskSweeper"));
        break;
#endif

    // ---- Fallbacks ----
    case NOT_FOUND:
        mToolNameLabel->setText(mCustomDisplayName.isEmpty()
                                    ? tr("Selected tool") : mCustomDisplayName);
        mDescriptionLabel->setText(tr("The selected disk usage analyzer was not found on this system. "
                                      "Install it or choose a different tool in Settings."));
        mStatusLabel->setText(tr("Status: Not installed"));
                mActionButton->hide();
        break;
    case NO_TOOL:
        mToolNameLabel->setText(tr("No disk usage tool available"));
        mDescriptionLabel->setText(tr("No supported disk usage analyzer was found for this platform."));
        mStatusLabel->hide();
        mActionButton->hide();
        break;
    }

    applyThemeColors();
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void DiskUsageLauncherWidget::onActionClicked()
{
    switch (mState) {
    case LAUNCH_BAOBAB:
        QProcess::startDetached(mLaunchCmd, mLaunchArgs);
        break;

    case INSTALL_BAOBAB: {
        mActionButton->setEnabled(false);
        mActionButton->setText(tr("Installing..."));
        const ExecResult result = CommandUtil::sudoExecWithStatus("apt-get", {"install", "-y", "baobab"});
        if (!result.ok())
            qWarning() << "DiskUsageLauncherWidget: apt-get install baobab failed:" << result.error;
        detect();
        updateUi();
        mActionButton->setEnabled(true);
        break;
    }

    case LAUNCH_FILELIGHT:
        QProcess::startDetached(mLaunchCmd, mLaunchArgs);
        break;

    case INSTALL_FILELIGHT_FLATPAK: {
        mActionButton->setEnabled(false);
        mActionButton->setText(tr("Installing..."));
        (void)QtConcurrent::run([this]() {
            const ExecResult result = CommandUtil::execWithStatus("flatpak", {"install", "--user", "-y",
                                          "flathub", "org.kde.filelight"}, {}, 300000);
            if (!result.ok())
                qWarning() << "DiskUsageLauncherWidget: flatpak install filelight failed:" << result.error;
            QMetaObject::invokeMethod(this, [this]() {
                detect();
                updateUi();
                mActionButton->setEnabled(true);
            });
        });
        break;
    }

    case NO_FLATPAK: {
        mActionButton->setEnabled(false);
        mActionButton->setText(tr("Installing..."));
        QString pkg = "flatpak";
        ExecResult result;
        switch (mToolManager->packageTool()->currentPackageTool) {
        case APT_RPM:
        case APT:
            result = CommandUtil::sudoExecWithStatus("apt-get", {"install", "-y", pkg});
            break;
        case DNF:
        case YUM:
            result = CommandUtil::sudoExecWithStatus("dnf", {"install", "-y", pkg});
            break;
        case PACMAN:
            result = CommandUtil::sudoExecWithStatus("pacman", {"-S", "--noconfirm", pkg});
            break;
        case ZYPPER:
            result = CommandUtil::sudoExecWithStatus("zypper", {"install", "-y", pkg});
            break;
        default:
            QMessageBox::warning(this, tr("Unsupported Package Manager"),
                                 tr("Could not detect a supported package manager to install Flatpak."));
            break;
        }
        if (!result.ok() && !result.error.isEmpty())
            qWarning() << "DiskUsageLauncherWidget: flatpak install failed:" << result.error;
        detect();
        updateUi();
        mActionButton->setEnabled(true);
        break;
    }

    case LAUNCH_QDIRSTAT:
        QProcess::startDetached(mLaunchCmd, mLaunchArgs);
        break;

    case LAUNCH_NCDU: {
        // ncdu is a TUI app — launch inside the default terminal emulator
#ifdef Q_OS_LINUX
        QStringList terminals = {"x-terminal-emulator", "gnome-terminal",
                                 "konsole", "xfce4-terminal", "xterm"};
        for (const QString &term : terminals) {
            if (!QStandardPaths::findExecutable(term).isEmpty()) {
                if (term == "gnome-terminal")
                    QProcess::startDetached(term, {"--", "ncdu", "/"});
                else
                    QProcess::startDetached(term, {"-e", "ncdu /"});
                break;
            }
        }
#elif defined(Q_OS_MACOS)
        QProcess::startDetached("open", {"-a", "Terminal", "ncdu"});
#endif
        break;
    }

#ifdef Q_OS_MACOS
    case LAUNCH_GRANDPERSPECTIVE:
        QProcess::startDetached("open", {"-a", "GrandPerspective"});
        break;

    case LINK_GRANDPERSPECTIVE:
        QDesktopServices::openUrl(QUrl("https://grandperspectiv.sourceforge.net"));
        break;

    case LAUNCH_DAISYDISK:
        QProcess::startDetached("open", {"-a", "DaisyDisk"});
        break;

    case LINK_DAISYDISK:
        QDesktopServices::openUrl(QUrl("https://daisydiskapp.com"));
        break;

    case LAUNCH_OMNIDISKSWEEPER:
        QProcess::startDetached("open", {"-a", "OmniDiskSweeper"});
        break;

    case LINK_OMNIDISKSWEEPER:
        QDesktopServices::openUrl(QUrl("https://www.omnigroup.com/more"));
        break;
#endif

    case LAUNCH_CUSTOM: {
        QString customPath = mSettingManager->getDiskAnalyzerCustomPath();
        if (!customPath.isEmpty())
            QProcess::startDetached(customPath, {});
        break;
    }

    case NOT_FOUND:
    case NO_TOOL:
        break;
    }
}

void DiskUsageLauncherWidget::onBuiltinClicked()
{
    // Dialog parents to the launcher's top-level window so it raises and
    // closes with Nexis. It owns its own scanner thread; multiple opens
    // create independent dialogs.
    auto *dlg = new DiskTreemapDialog(window(), mAppManager, mSignalMapper);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void DiskUsageLauncherWidget::applyThemeColors()
{
    QSettings *sv = mAppManager->getStyleValues();
    if (!sv)
        return;

    QString textColor = sv->value("@color12").toString();
    mToolNameLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    mDescriptionLabel->setStyleSheet(QString("color: %1;").arg(textColor));

    bool installed = (mState == LAUNCH_BAOBAB || mState == LAUNCH_FILELIGHT ||
                      mState == LAUNCH_QDIRSTAT || mState == LAUNCH_NCDU ||
                      mState == LAUNCH_CUSTOM
#ifdef Q_OS_MACOS
                      || mState == LAUNCH_GRANDPERSPECTIVE || mState == LAUNCH_DAISYDISK
                      || mState == LAUNCH_OMNIDISKSWEEPER
#endif
                      );
    QString statusColor = sv->value(installed ? "@successColor" : "@tertiaryText").toString();
    mStatusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(statusColor));
}

void DiskUsageLauncherWidget::setElevated(const QString &accentToken)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("cardRole", "elevated");
    style()->unpolish(this);
    style()->polish(this);

    mAccentBar->setProperty("accentToken", accentToken);
    mAccentBar->setVisible(true);
    style()->unpolish(mAccentBar);
    style()->polish(mAccentBar);

    Utilities::addDropShadow(this, 90, 26);
}
