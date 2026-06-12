#ifndef MACOS_MAINTENANCE_PANEL_H
#define MACOS_MAINTENANCE_PANEL_H

#include <QDialog>
#include <QFuture>
#include <QString>
#include <QStringList>
#include <QList>

class QLabel;
class QPushButton;

// Describes one macOS maintenance task. The cmd/args fields are the primary
// test seam — use MacOSMaintenancePanel::defaultTasks() to verify argv
// construction without executing any real commands.
struct MacOSMaintenanceTask {
    QString id;
    QString title;
    QString description;
    QString confirmText;
    QString cmd;
    QStringList args;
    int timeoutMs = 30000;
    bool needsSudo = false;
    // Optional second command executed only after cmd exits 0
    QString cmd2;
    QStringList args2;
    int timeoutMs2 = 10000;
};

// Modal dialog that exposes OnyX-style macOS maintenance tasks:
//   Spotlight reindex, disk verify, Launch Services rebuild, DNS flush,
//   and several well-known hidden Finder/Dock defaults toggles.
//
// Each task runs asynchronously (QtConcurrent). The destructor blocks until
// all workers finish, following the same WI-01 UAF-safe worker-lifetime
// contract used by MaintenanceWizardDialog.
class MacOSMaintenancePanel : public QDialog
{
    Q_OBJECT

public:
    explicit MacOSMaintenancePanel(QWidget *parent = nullptr);
    ~MacOSMaintenancePanel() override;

    // Returns the canonical task list. Pure function; exposed so unit tests
    // can verify command / argument construction without running real commands.
    static QList<MacOSMaintenanceTask> defaultTasks();

private:
    void buildUI();
    void runTask(int index);
    void onTaskFinished(int index, bool success,
                        const QString &output, const QString &error);

    struct TaskRow {
        QPushButton *btn          = nullptr;
        QLabel      *statusLabel  = nullptr;
        QFuture<void> future;
    };

    QList<TaskRow> mRows;
};

#endif // MACOS_MAINTENANCE_PANEL_H
