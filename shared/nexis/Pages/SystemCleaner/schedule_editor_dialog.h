#ifndef SCHEDULE_EDITOR_DIALOG_H
#define SCHEDULE_EDITOR_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>

#include <Managers/schedule_manager.h>

class ScheduleEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScheduleEditorDialog(QWidget *parent = nullptr);
    explicit ScheduleEditorDialog(const ScheduleManager::CleaningSchedule &schedule, QWidget *parent = nullptr);

    ScheduleManager::CleaningSchedule getSchedule() const;

signals:
    void scheduleCreated(ScheduleManager::CleaningSchedule schedule);
    void scheduleUpdated(ScheduleManager::CleaningSchedule schedule);

private slots:
    void onFrequencyChanged();
    void onSave();

private:
    void buildUI();
    void populateFromSchedule(const ScheduleManager::CleaningSchedule &schedule);
    bool validate();

    bool mEditMode = false;
    QString mScheduleId;
    bool mDryRunCompleted = false;

    QLineEdit *mTxtName;
    QRadioButton *mRadDaily;
    QRadioButton *mRadEveryNDays;
    QRadioButton *mRadWeekly;
    QRadioButton *mRadMonthly;
    QButtonGroup *mFrequencyGroup;

    QSpinBox *mSpnEveryNDays;
    QComboBox *mCmbDayOfWeek;
    QSpinBox *mSpnDayOfMonth;
    QLabel *mLblEveryNDays;
    QLabel *mLblDayOfWeek;
    QLabel *mLblDayOfMonth;

    QSpinBox *mSpnHour;
    QSpinBox *mSpnMinute;

    QCheckBox *mChkPackageCache;
    QCheckBox *mChkCrashReports;
    QCheckBox *mChkAppLogs;
    QCheckBox *mChkAppCaches;
    QCheckBox *mChkTrash;
    QCheckBox *mChkDevToolCaches;
    QLabel *mLblTrashWarning;

    QCheckBox *mChkSkipRecent;
    QSpinBox *mSpnMinFileAge;

    QLabel *mLblError;
    QPushButton *mBtnSave;
    QPushButton *mBtnCancel;
};

#endif // SCHEDULE_EDITOR_DIALOG_H
