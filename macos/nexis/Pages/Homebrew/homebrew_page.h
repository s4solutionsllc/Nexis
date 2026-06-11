#ifndef HOMEBREW_PAGE_H
#define HOMEBREW_PAGE_H

#include <QWidget>
#include <QLabel>
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
    void onUpgradeStarted(const QString &label);                 // SSO-3741
    void onUpgradeFinished(const QString &label, bool ok, const QString &error);  // SSO-3741
    void onRepoHealthChecked(const RepoHealthCache &cache);

private:
    void buildUI();
    void fetchPackages();
    QStringList getSelectedPackages() const;
    void updateUninstallButton();
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
    QLabel *mLblUpdatesTitle = nullptr;
    QLabel *mLblUpgradeProgress = nullptr;  // SSO-3741
    QPushButton *mBtnCheckNow = nullptr;
    QPushButton *mBtnUpgradeAll = nullptr;  // SSO-3741
    QTreeWidget *mUpdatesTree = nullptr;

    QList<Package> mPackages;
};

#endif // HOMEBREW_PAGE_H
