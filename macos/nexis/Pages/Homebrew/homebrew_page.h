#ifndef HOMEBREW_PAGE_H
#define HOMEBREW_PAGE_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QLineEdit>
#include <QTreeWidget>
#include <Tools/package_tool_shared.h>
#include <Tools/repo_health_types.h>
#include <Info/update_info.h>

class ToolManager;
class SignalMapper;
class DataRefreshService;

// macOS counterpart to APTSourceManagerPage. Renders installed Homebrew
// packages (formulae + casks) grouped by section in a tree widget, with
// install/uninstall and the shared update/health surface. Uses PackageTool
// for the package list and RepositoryTool::addRepository for installs —
// no APTSource field overloading.
class HomebrewPage : public QWidget
{
    Q_OBJECT

public:
    explicit HomebrewPage(QWidget *parent = nullptr,
                          ToolManager *toolManager = nullptr,
                          SignalMapper *signalMapper = nullptr,
                          DataRefreshService *refreshService = nullptr);

signals:
    void packagesLoaded();

private slots:
    void onPackagesLoaded();
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onSearchTextChanged(const QString &val);
    void onInstallClicked();
    void onCancelClicked();
    void onUninstallClicked();
    void onSystemUpdatesChecked(const UpdateCheckResult &result);
    void onRepoHealthChecked(const RepoHealthCache &cache);
    void onUpdatesTreeItemChanged(QTreeWidgetItem *item, int column);
    void onSelectAllToggled(bool checked);
    void onUpdateSelectedClicked();

private:
    void buildUI();
    void fetchPackages();
    QStringList getSelectedPackages() const;
    QStringList getSelectedCaskUpdates() const;
    void updateUninstallButton();
    void updateUpdateButton();
    void setInstallFieldsVisible(bool visible);

    ToolManager *mToolManager = nullptr;
    SignalMapper *mSignalMapper = nullptr;
    DataRefreshService *mRefresh = nullptr;

    QLabel *mLblTitle = nullptr;
    QLineEdit *mTxtSearch = nullptr;
    QLineEdit *mTxtInstall = nullptr;
    QPushButton *mBtnInstall = nullptr;
    QPushButton *mBtnCancel = nullptr;
    QPushButton *mBtnUninstall = nullptr;
    QTreeWidget *mTreeWidget = nullptr;

    // Available Updates section
    QWidget *mUpdatesSection = nullptr;
    QProgressBar *mUpdatesProgress = nullptr;
    QLabel *mLblUpdatesTitle = nullptr;
    QPushButton *mBtnCheckNow = nullptr;
    QCheckBox *mChkSelectAll = nullptr;
    QPushButton *mBtnUpdateSelected = nullptr;
    QTreeWidget *mUpdatesTree = nullptr;
    bool mUpdatesRunning = false;

    QList<Package> mPackages;
};

#endif // HOMEBREW_PAGE_H
