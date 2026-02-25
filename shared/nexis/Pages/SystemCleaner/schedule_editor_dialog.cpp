#include "schedule_editor_dialog.h"
#include <Managers/cleaner_service.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>

ScheduleEditorDialog::ScheduleEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUI();
    setWindowTitle(tr("New Cleaning Schedule"));
    mLblDialogTitle->setText(tr("New Cleaning Schedule"));

    // Defaults
    mTxtName->setText(tr("Weekly Cleanup"));
    mRadWeekly->setChecked(true);
    mSpnHour->setValue(3);
    mSpnMinute->setValue(0);
    mChkPackageCache->setChecked(true);
    mChkCrashReports->setChecked(true);
    mChkAppLogs->setChecked(true);
    mChkAppCaches->setChecked(true);
    mChkDevToolCaches->setChecked(true);
    mChkSkipRecent->setChecked(true);
    mSpnMinFileAge->setValue(24);

    onFrequencyChanged();
}

ScheduleEditorDialog::ScheduleEditorDialog(const ScheduleManager::CleaningSchedule &schedule, QWidget *parent)
    : QDialog(parent), mEditMode(true)
{
    buildUI();
    setWindowTitle(tr("Edit Cleaning Schedule"));
    mLblDialogTitle->setText(tr("Edit Cleaning Schedule"));
    populateFromSchedule(schedule);
    onFrequencyChanged();
}

void ScheduleEditorDialog::buildUI()
{
    setMinimumSize(450, 480);
    setObjectName("scheduleEditorDialog");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 15, 20, 15);

    // Dialog title (themed via QSS accessibleName="dialog-title")
    mLblDialogTitle = new QLabel;
    mLblDialogTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(mLblDialogTitle);

    // Schedule name
    QFormLayout *nameForm = new QFormLayout;
    mTxtName = new QLineEdit;
    mTxtName->setPlaceholderText(tr("e.g. Weekly Cleanup"));
    nameForm->addRow(tr("Schedule Name:"), mTxtName);
    mainLayout->addLayout(nameForm);

    // Frequency
    QGroupBox *freqGroup = new QGroupBox(tr("Frequency"));
    QVBoxLayout *freqLayout = new QVBoxLayout(freqGroup);
    freqLayout->setSpacing(6);

    mFrequencyGroup = new QButtonGroup(this);
    mRadDaily = new QRadioButton(tr("Daily"));
    mRadEveryNDays = new QRadioButton(tr("Every N Days"));
    mRadWeekly = new QRadioButton(tr("Weekly"));
    mRadMonthly = new QRadioButton(tr("Monthly"));
    mFrequencyGroup->addButton(mRadDaily, 0);
    mFrequencyGroup->addButton(mRadEveryNDays, 1);
    mFrequencyGroup->addButton(mRadWeekly, 2);
    mFrequencyGroup->addButton(mRadMonthly, 3);

    QHBoxLayout *radRow = new QHBoxLayout;
    radRow->addWidget(mRadDaily);
    radRow->addWidget(mRadEveryNDays);
    radRow->addWidget(mRadWeekly);
    radRow->addWidget(mRadMonthly);
    freqLayout->addLayout(radRow);

    // Conditional fields
    QHBoxLayout *condRow = new QHBoxLayout;

    mLblEveryNDays = new QLabel(tr("Every"));
    mSpnEveryNDays = new QSpinBox;
    mSpnEveryNDays->setRange(2, 90);
    mSpnEveryNDays->setValue(3);
    mSpnEveryNDays->setSuffix(tr(" days"));
    condRow->addWidget(mLblEveryNDays);
    condRow->addWidget(mSpnEveryNDays);

    mLblDayOfWeek = new QLabel(tr("Day:"));
    mCmbDayOfWeek = new QComboBox;
    mCmbDayOfWeek->addItems({tr("Sunday"), tr("Monday"), tr("Tuesday"),
                             tr("Wednesday"), tr("Thursday"), tr("Friday"),
                             tr("Saturday")});
    condRow->addWidget(mLblDayOfWeek);
    condRow->addWidget(mCmbDayOfWeek);

    mLblDayOfMonth = new QLabel(tr("Day of month:"));
    mSpnDayOfMonth = new QSpinBox;
    mSpnDayOfMonth->setRange(1, 31);
    condRow->addWidget(mLblDayOfMonth);
    condRow->addWidget(mSpnDayOfMonth);

    condRow->addStretch();
    freqLayout->addLayout(condRow);

    mainLayout->addWidget(freqGroup);

    // Time
    QHBoxLayout *timeRow = new QHBoxLayout;
    timeRow->addWidget(new QLabel(tr("Time:")));
    mSpnHour = new QSpinBox;
    mSpnHour->setRange(0, 23);
    mSpnHour->setWrapping(true);
    mSpnHour->setPrefix(tr(""));
    mSpnHour->setSuffix(tr(" h"));
    timeRow->addWidget(mSpnHour);
    timeRow->addWidget(new QLabel(":"));
    mSpnMinute = new QSpinBox;
    mSpnMinute->setRange(0, 59);
    mSpnMinute->setWrapping(true);
    mSpnMinute->setSuffix(tr(" m"));
    timeRow->addWidget(mSpnMinute);
    timeRow->addStretch();
    mainLayout->addLayout(timeRow);

    // Categories
    QGroupBox *catGroup = new QGroupBox(tr("Categories to Clean"));
    QGridLayout *catGrid = new QGridLayout(catGroup);
    mChkPackageCache = new QCheckBox(tr("Package Caches"));
    mChkCrashReports = new QCheckBox(tr("Crash Reports"));
    mChkAppLogs = new QCheckBox(tr("Application Logs"));
    mChkAppCaches = new QCheckBox(tr("Application Caches"));
    mChkTrash = new QCheckBox(tr("Trash"));
    mChkDevToolCaches = new QCheckBox(tr("Dev Tool Caches"));
    mChkBrowserPrivacy = new QCheckBox(tr("Browser Privacy"));

    catGrid->addWidget(mChkPackageCache, 0, 0);
    catGrid->addWidget(mChkCrashReports, 0, 1);
    catGrid->addWidget(mChkAppLogs, 1, 0);
    catGrid->addWidget(mChkAppCaches, 1, 1);
    catGrid->addWidget(mChkTrash, 2, 0);
    catGrid->addWidget(mChkDevToolCaches, 2, 1);
    catGrid->addWidget(mChkBrowserPrivacy, 3, 0);

    mLblTrashWarning = new QLabel(tr("\xe2\x9a\xa0 Trash is permanently deleted and cannot be recovered"));
    mLblTrashWarning->setObjectName("lblTrashWarning");
    mLblTrashWarning->setVisible(false);
    catGrid->addWidget(mLblTrashWarning, 4, 0, 1, 2);

    connect(mChkTrash, &QCheckBox::toggled, mLblTrashWarning, &QLabel::setVisible);

    mainLayout->addWidget(catGroup);

    // Min file age
    QHBoxLayout *ageRow = new QHBoxLayout;
    mChkSkipRecent = new QCheckBox(tr("Skip files newer than"));
    mSpnMinFileAge = new QSpinBox;
    mSpnMinFileAge->setRange(1, 168);
    mSpnMinFileAge->setValue(24);
    mSpnMinFileAge->setSuffix(tr(" hours"));
    ageRow->addWidget(mChkSkipRecent);
    ageRow->addWidget(mSpnMinFileAge);
    ageRow->addStretch();
    mainLayout->addLayout(ageRow);

    connect(mChkSkipRecent, &QCheckBox::toggled, mSpnMinFileAge, &QSpinBox::setEnabled);

    // Error label
    mLblError = new QLabel;
    mLblError->setObjectName("lblErrorMsg");
    mLblError->setVisible(false);
    mainLayout->addWidget(mLblError);

    // Buttons
    mainLayout->addStretch();
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    mBtnCancel = new QPushButton(tr("Cancel"));
    mBtnSave = new QPushButton(tr("Save"));
    mBtnSave->setDefault(true);
    mBtnSave->setProperty("accessibleName", "primary");
    btnRow->addWidget(mBtnCancel);
    btnRow->addWidget(mBtnSave);
    mainLayout->addLayout(btnRow);

    // Connections
    connect(mFrequencyGroup, &QButtonGroup::idClicked, this, &ScheduleEditorDialog::onFrequencyChanged);
    connect(mBtnSave, &QPushButton::clicked, this, &ScheduleEditorDialog::onSave);
    connect(mBtnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void ScheduleEditorDialog::populateFromSchedule(const ScheduleManager::CleaningSchedule &schedule)
{
    mScheduleId = schedule.id;
    mDryRunCompleted = schedule.dryRunCompleted;
    mTxtName->setText(schedule.name);

    switch (schedule.frequency) {
    case ScheduleManager::Daily:      mRadDaily->setChecked(true); break;
    case ScheduleManager::EveryNDays: mRadEveryNDays->setChecked(true); break;
    case ScheduleManager::Weekly:     mRadWeekly->setChecked(true); break;
    case ScheduleManager::Monthly:    mRadMonthly->setChecked(true); break;
    }

    mSpnEveryNDays->setValue(schedule.everyNDays);
    mCmbDayOfWeek->setCurrentIndex(schedule.dayOfWeek);
    mSpnDayOfMonth->setValue(schedule.dayOfMonth);
    mSpnHour->setValue(schedule.hour);
    mSpnMinute->setValue(schedule.minute);

    for (int cat : schedule.categories) {
        switch (cat) {
        case CleanerService::PACKAGE_CACHE:     mChkPackageCache->setChecked(true); break;
        case CleanerService::CRASH_REPORTS:     mChkCrashReports->setChecked(true); break;
        case CleanerService::APPLICATION_LOGS:  mChkAppLogs->setChecked(true); break;
        case CleanerService::APPLICATION_CACHES:mChkAppCaches->setChecked(true); break;
        case CleanerService::TRASH:             mChkTrash->setChecked(true); break;
        case CleanerService::DEV_TOOL_CACHES:   mChkDevToolCaches->setChecked(true); break;
        case CleanerService::BROWSER_PRIVACY:   mChkBrowserPrivacy->setChecked(true); break;
        }
    }

    if (schedule.minFileAgeSecs > 0) {
        mChkSkipRecent->setChecked(true);
        mSpnMinFileAge->setValue(schedule.minFileAgeSecs / 3600);
    } else {
        mChkSkipRecent->setChecked(false);
    }
}

void ScheduleEditorDialog::onFrequencyChanged()
{
    bool everyN = mRadEveryNDays->isChecked();
    bool weekly = mRadWeekly->isChecked();
    bool monthly = mRadMonthly->isChecked();

    mLblEveryNDays->setVisible(everyN);
    mSpnEveryNDays->setVisible(everyN);
    mLblDayOfWeek->setVisible(weekly);
    mCmbDayOfWeek->setVisible(weekly);
    mLblDayOfMonth->setVisible(monthly);
    mSpnDayOfMonth->setVisible(monthly);
}

bool ScheduleEditorDialog::validate()
{
    if (mTxtName->text().trimmed().isEmpty()) {
        mLblError->setText(tr("Schedule name is required."));
        mLblError->setVisible(true);
        return false;
    }

    bool anyCat = mChkPackageCache->isChecked() || mChkCrashReports->isChecked() ||
                  mChkAppLogs->isChecked() || mChkAppCaches->isChecked() ||
                  mChkTrash->isChecked() || mChkDevToolCaches->isChecked() ||
                  mChkBrowserPrivacy->isChecked();
    if (!anyCat) {
        mLblError->setText(tr("Select at least one category."));
        mLblError->setVisible(true);
        return false;
    }

    mLblError->setVisible(false);
    return true;
}

ScheduleManager::CleaningSchedule ScheduleEditorDialog::getSchedule() const
{
    ScheduleManager::CleaningSchedule s;
    s.id = mScheduleId;
    s.name = mTxtName->text().trimmed();
    s.enabled = true;
    s.dryRunCompleted = mDryRunCompleted;

    if (mRadDaily->isChecked())        s.frequency = ScheduleManager::Daily;
    else if (mRadEveryNDays->isChecked()) s.frequency = ScheduleManager::EveryNDays;
    else if (mRadWeekly->isChecked())  s.frequency = ScheduleManager::Weekly;
    else if (mRadMonthly->isChecked()) s.frequency = ScheduleManager::Monthly;

    s.everyNDays = mSpnEveryNDays->value();
    s.dayOfWeek = mCmbDayOfWeek->currentIndex();
    s.dayOfMonth = mSpnDayOfMonth->value();
    s.hour = mSpnHour->value();
    s.minute = mSpnMinute->value();

    if (mChkPackageCache->isChecked())  s.categories << CleanerService::PACKAGE_CACHE;
    if (mChkCrashReports->isChecked())  s.categories << CleanerService::CRASH_REPORTS;
    if (mChkAppLogs->isChecked())       s.categories << CleanerService::APPLICATION_LOGS;
    if (mChkAppCaches->isChecked())     s.categories << CleanerService::APPLICATION_CACHES;
    if (mChkTrash->isChecked())         s.categories << CleanerService::TRASH;
    if (mChkDevToolCaches->isChecked()) s.categories << CleanerService::DEV_TOOL_CACHES;
    if (mChkBrowserPrivacy->isChecked()) s.categories << CleanerService::BROWSER_PRIVACY;

    s.minFileAgeSecs = mChkSkipRecent->isChecked() ? mSpnMinFileAge->value() * 3600 : 0;

    return s;
}

void ScheduleEditorDialog::onSave()
{
    if (!validate())
        return;

    ScheduleManager::CleaningSchedule s = getSchedule();

    if (mEditMode) {
        emit scheduleUpdated(s);
    } else {
        emit scheduleCreated(s);
    }

    accept();
}
