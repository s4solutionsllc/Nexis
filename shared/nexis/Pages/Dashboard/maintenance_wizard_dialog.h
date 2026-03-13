#ifndef MAINTENANCE_WIZARD_DIALOG_H
#define MAINTENANCE_WIZARD_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QAtomicInt>

#include <Managers/cleaner_service.h>
#include <Info/update_info.h>
#include <Tools/package_tool_shared.h>

class QVBoxLayout;
class AppManager;
class InfoManager;
class ToolManager;
class SignalMapper;

class MaintenanceWizardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MaintenanceWizardDialog(QWidget *parent = nullptr,
                                     AppManager *appManager = nullptr,
                                     InfoManager *infoManager = nullptr,
                                     ToolManager *toolManager = nullptr,
                                     SignalMapper *signalMapper = nullptr);

    void runChecks();

private slots:
    void onScanFinished(CleanerService::ScanResult result);
    void onOrphansFinished(QList<OrphanPackage> orphans);
    void onUpdatesFinished(UpdateCheckResult result);
    void onHealthScoreFinished(int score, QString label);
    void onAllChecksComplete();
    void onCleanSafeItems();
    void onCleanFinished(quint64 bytesFreed);

private:
    void buildUI();
    void refreshThemeColors();
    void setStepStatus(QLabel *icon, QLabel *detail, const QString &status, const QString &detailText);
    void navigateToPage(const QString &pageTitle);

    AppManager *mAppManager;
    InfoManager *mInfoManager;
    ToolManager *mToolManager;
    SignalMapper *mSignalMapper;

    // Step rows
    QLabel *mIconJunk, *mLblJunkDetail;
    QLabel *mIconOrphans, *mLblOrphansDetail;
    QLabel *mIconUpdates, *mLblUpdatesDetail;
    QLabel *mIconHealth, *mLblHealthDetail;

    // Results
    QVBoxLayout *mResultsLayout;
    QWidget *mResultsWidget;

    // Buttons
    QPushButton *mBtnClean;
    QPushButton *mBtnClose;

    // State
    QAtomicInt mChecksComplete;
    CleanerService::ScanResult mScanResult;
    QList<OrphanPackage> mOrphanResult;
    UpdateCheckResult mUpdateResult;
    int mHealthScore;
    QString mHealthLabel;
    bool mCleaningDone;
};

#endif // MAINTENANCE_WIZARD_DIALOG_H
