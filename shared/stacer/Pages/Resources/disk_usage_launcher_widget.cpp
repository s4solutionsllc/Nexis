#include "disk_usage_launcher_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QMessageBox>
#include <QtConcurrent>

#ifdef Q_OS_MACOS
#include <QFileInfo>
#include <QDir>
#endif

#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "Utils/command_util.h"
#include "Tools/package_tool.h"

DiskUsageLauncherWidget::DiskUsageLauncherWidget(QWidget *parent)
    : QWidget(parent),
      mState(NO_TOOL)
{
    // --- Title (matches HistoryChart's lblHistoryTitle) ---
    mTitleLabel = new QLabel(tr("Disk Usage Analysis"), this);
    mTitleLabel->setObjectName("lblHistoryTitle");

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
    iconLabel->setFixedSize(48, 48);
    iconLabel->setScaledContents(true);

    auto *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(12, 0, 12, 0);
    contentLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    contentLayout->addLayout(infoLayout, 1);

    // --- Action button ---
    mActionButton = new QPushButton(this);
    mActionButton->setCursor(Qt::PointingHandCursor);
    connect(mActionButton, &QPushButton::clicked, this, &DiskUsageLauncherWidget::onActionClicked);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(12, 0, 12, 12);
    buttonLayout->addStretch();
    buttonLayout->addWidget(mActionButton);
    buttonLayout->addStretch();

    // --- Main layout ---
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 6, 12, 0);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(mTitleLabel);
    mainLayout->addLayout(contentLayout);
    mainLayout->addLayout(buttonLayout);

    // Detect platform/tool and update UI
    detect();
    updateUi();

    // Set the icon after detection so we know which tool
    QIcon toolIcon;
#ifdef Q_OS_LINUX
    if (mState == LAUNCH_BAOBAB || mState == INSTALL_BAOBAB)
        toolIcon = QIcon::fromTheme("org.gnome.baobab", QIcon::fromTheme("baobab"));
    else
        toolIcon = QIcon::fromTheme("org.kde.filelight", QIcon::fromTheme("filelight"));
#elif defined(Q_OS_MACOS)
    toolIcon = QIcon::fromTheme("drive-harddisk");
#endif
    if (!toolIcon.isNull())
        iconLabel->setPixmap(toolIcon.pixmap(48, 48));
    else
        iconLabel->hide();

    // Theme change support
    applyThemeColors();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &DiskUsageLauncherWidget::applyThemeColors);
}

void DiskUsageLauncherWidget::detect()
{
#ifdef Q_OS_LINUX
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toUpper();
    bool isGnome = desktop.contains("GNOME");

    if (isGnome) {
        if (!QStandardPaths::findExecutable("baobab").isEmpty())
            mState = LAUNCH_BAOBAB;
        else
            mState = INSTALL_BAOBAB;
    } else {
        if (!QStandardPaths::findExecutable("filelight").isEmpty())
            mState = LAUNCH_FILELIGHT;
        else if (!QStandardPaths::findExecutable("flatpak").isEmpty())
            mState = INSTALL_FILELIGHT_FLATPAK;
        else
            mState = NO_FLATPAK;
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

void DiskUsageLauncherWidget::updateUi()
{
    switch (mState) {
    case LAUNCH_BAOBAB:
        mToolNameLabel->setText(tr("Disk Usage Analyzer (Baobab)"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a graphical treemap."));
        mStatusLabel->setText(tr("Status: Installed"));
        mStatusLabel->setStyleSheet("color: #2ec27e; font-weight: bold;");
        mActionButton->setText(tr("Launch Disk Usage Analyzer"));
        break;
    case INSTALL_BAOBAB:
        mToolNameLabel->setText(tr("Disk Usage Analyzer (Baobab)"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a graphical treemap. "
                                      "Baobab is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
        mStatusLabel->setStyleSheet("color: #77767b; font-weight: bold;");
        mActionButton->setText(tr("Install Disk Usage Analyzer"));
        break;
    case LAUNCH_FILELIGHT:
        mToolNameLabel->setText(tr("Filelight"));
        mDescriptionLabel->setText(tr("Visualize disk usage with interactive sunburst charts."));
        mStatusLabel->setText(tr("Status: Installed"));
        mStatusLabel->setStyleSheet("color: #2ec27e; font-weight: bold;");
        mActionButton->setText(tr("Launch Filelight"));
        break;
    case INSTALL_FILELIGHT_FLATPAK:
        mToolNameLabel->setText(tr("Filelight"));
        mDescriptionLabel->setText(tr("Visualize disk usage with interactive sunburst charts. "
                                      "Filelight is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
        mStatusLabel->setStyleSheet("color: #77767b; font-weight: bold;");
        mActionButton->setText(tr("Install Filelight"));
        break;
    case NO_FLATPAK:
        mToolNameLabel->setText(tr("Filelight"));
        mDescriptionLabel->setText(tr("Visualize disk usage with interactive sunburst charts. "
                                      "Flatpak is required to install Filelight on this system."));
        mStatusLabel->setText(tr("Status: Not installed"));
        mStatusLabel->setStyleSheet("color: #77767b; font-weight: bold;");
        mActionButton->setText(tr("Install Flatpak"));
        break;
#ifdef Q_OS_MACOS
    case LAUNCH_GRANDPERSPECTIVE:
        mToolNameLabel->setText(tr("GrandPerspective"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a treemap."));
        mStatusLabel->setText(tr("Status: Installed"));
        mStatusLabel->setStyleSheet("color: #2ec27e; font-weight: bold;");
        mActionButton->setText(tr("Launch GrandPerspective"));
        break;
    case LINK_GRANDPERSPECTIVE:
        mToolNameLabel->setText(tr("GrandPerspective"));
        mDescriptionLabel->setText(tr("Visualize disk usage as a treemap. "
                                      "GrandPerspective is not currently installed."));
        mStatusLabel->setText(tr("Status: Not installed"));
        mStatusLabel->setStyleSheet("color: #77767b; font-weight: bold;");
        mActionButton->setText(tr("Get GrandPerspective"));
        break;
#endif
    case NO_TOOL:
        mToolNameLabel->setText(tr("No disk usage tool available"));
        mDescriptionLabel->setText(tr("No supported disk usage analyzer was found for this platform."));
        mStatusLabel->hide();
        mActionButton->hide();
        break;
    }
}

void DiskUsageLauncherWidget::onActionClicked()
{
    switch (mState) {
    case LAUNCH_BAOBAB:
        QProcess::startDetached("baobab", {});
        break;

    case INSTALL_BAOBAB: {
        mActionButton->setEnabled(false);
        mActionButton->setText(tr("Installing..."));
        // sudoExec blocks but uses pkexec for auth, so it's acceptable
        CommandUtil::sudoExec("apt-get", {"install", "-y", "baobab"});
        detect();
        updateUi();
        mActionButton->setEnabled(true);
        break;
    }

    case LAUNCH_FILELIGHT:
        QProcess::startDetached("filelight", {});
        break;

    case INSTALL_FILELIGHT_FLATPAK: {
        mActionButton->setEnabled(false);
        mActionButton->setText(tr("Installing..."));
        (void)QtConcurrent::run([this]() {
            CommandUtil::exec("flatpak", {"install", "--user", "-y",
                                          "flathub", "org.kde.filelight"});
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
        // Determine the right package manager install command
        QString pkg = "flatpak";
        switch (PackageTool::currentPackageTool) {
        case APT:
            CommandUtil::sudoExec("apt-get", {"install", "-y", pkg});
            break;
        case DNF:
        case YUM:
            CommandUtil::sudoExec("dnf", {"install", "-y", pkg});
            break;
        case PACMAN:
            CommandUtil::sudoExec("pacman", {"-S", "--noconfirm", pkg});
            break;
        case ZYPPER:
            CommandUtil::sudoExec("zypper", {"install", "-y", pkg});
            break;
        default:
            QMessageBox::warning(this, tr("Unsupported Package Manager"),
                                 tr("Could not detect a supported package manager to install Flatpak."));
            break;
        }
        detect();
        updateUi();
        mActionButton->setEnabled(true);
        break;
    }

#ifdef Q_OS_MACOS
    case LAUNCH_GRANDPERSPECTIVE:
        QProcess::startDetached("open", {"-a", "GrandPerspective"});
        break;

    case LINK_GRANDPERSPECTIVE:
        QDesktopServices::openUrl(QUrl("https://grandperspectiv.sourceforge.net"));
        break;
#endif

    case NO_TOOL:
        break;
    }
}

void DiskUsageLauncherWidget::applyThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    QString textColor = sv->value("@color12").toString();
    mToolNameLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    mDescriptionLabel->setStyleSheet(QString("color: %1;").arg(textColor));
}
