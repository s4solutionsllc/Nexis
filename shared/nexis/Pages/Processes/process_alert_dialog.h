#ifndef PROCESS_ALERT_DIALOG_H
#define PROCESS_ALERT_DIALOG_H

#include <QDialog>
#include <QString>

#include "Managers/process_prefs_manager.h"

class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

// FR-116: set or clear a per-process CPU/memory threshold. The Processes
// page evaluates thresholds per tick and fires a tray notification when
// the aggregated RSS or CPU% across PIDs with the matching name exceeds
// the configured value. 0 means "no threshold on this metric".
//
// Dialog prefills from an existing threshold if one exists for this name.
class ProcessAlertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProcessAlertDialog(const QString &processName, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onDelete();

private:
    void buildUI();
    void populateFromPrefs();

    QString mProcessName;
    ProcessPrefsManager::Threshold mExisting;

    QSpinBox   *mSpnCpu    = nullptr;
    QSpinBox   *mSpnMem    = nullptr;
    QComboBox  *mCmbUnit   = nullptr;
    QLabel     *mLblName   = nullptr;
    QPushButton *mBtnSave  = nullptr;
    QPushButton *mBtnDelete = nullptr;
    QPushButton *mBtnCancel = nullptr;
};

#endif // PROCESS_ALERT_DIALOG_H
