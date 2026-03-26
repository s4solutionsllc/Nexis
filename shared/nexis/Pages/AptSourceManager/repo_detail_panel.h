#ifndef REPO_DETAIL_PANEL_H
#define REPO_DETAIL_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <Tools/repo_health_types.h>
#include <Tools/repo_repair_engine.h>
#include <Tools/apt_source_tool.h>

class SignalMapper;

class RepoDetailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RepoDetailPanel(QWidget *parent = nullptr);

    void showRepo(const APTSourcePtr &source, const RepoHealthResult &result,
                  const DiagnoseResult *diagnoseResult = nullptr);
    void clear();
    void showDiagnoseResult(const DiagnoseResult &result, QVBoxLayout *targetLayout);
    RepoHealthResult currentResult() const { return mCurrentResult; }

signals:
    void editRequested(const APTSourcePtr &source);
    void disableRequested(const APTSourcePtr &source);
    void repairActionRequested(const RepoRepairAction &action, const APTSourcePtr &source);
    void closeRequested();

private:
    void setupUi();
    void refreshThemeColors();
    void addIssueWidget(const RepoHealthIssue &issue);
    void clearIssues();

    // Header
    QLabel *mLblName = nullptr;
    QLabel *mLblStatusBadge = nullptr;
    QLabel *mLblDescription = nullptr;

    // Metadata
    QWidget *mMetadataWidget = nullptr;
    QLabel *mLblStatus = nullptr;
    QLabel *mLblLastChecked = nullptr;
    QLabel *mLblFile = nullptr;
    QLabel *mLblSuite = nullptr;
    QLabel *mLblFormat = nullptr;

    // Issues
    QWidget *mIssuesContainer = nullptr;
    QVBoxLayout *mIssuesLayout = nullptr;

    // Actions
    QPushButton *mBtnEdit = nullptr;
    QPushButton *mBtnOpenUri = nullptr;
    QPushButton *mBtnDisable = nullptr;
    QPushButton *mBtnClose = nullptr;

    APTSourcePtr mCurrentSource;
    RepoHealthResult mCurrentResult;
    SignalMapper *mSignalMapper = nullptr;
};

#endif // REPO_DETAIL_PANEL_H
