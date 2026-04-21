#ifndef PROCESS_PREFS_MANAGER_H
#define PROCESS_PREFS_MANAGER_H

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

// FR-116: persistence + signal hub for the Processes-page "pin" and
// threshold-alert features. Two pieces of state live here:
//
//   1. Pinned process names (by comm) — sticky at top of the table,
//      survive sort and filter.
//   2. Thresholds keyed by process name — fire a tray notification when
//      the aggregate CPU% or memory across PIDs with the same name
//      exceeds the user-configured value.
//
// Both persist via SettingManager in the same JSON-blob style used by
// ScheduleManager and CleanerExclusions. Emits changed() on mutation so
// ProcessesPage can re-render.
class ProcessPrefsManager : public QObject
{
    Q_OBJECT

public:
    struct Threshold {
        QString name;            // process comm
        int     cpuPercent = 0;  // 0 = no threshold on CPU
        qint64  memoryBytes = 0; // 0 = no threshold on memory
    };

    static ProcessPrefsManager *ins();

    // Pin state
    QStringList pinnedNames() const;
    bool isPinned(const QString &name) const;
    void setPinned(const QString &name, bool pinned);

    // Thresholds
    QList<Threshold> thresholds() const;
    Threshold threshold(const QString &name) const;   // default-constructed if absent
    bool hasThreshold(const QString &name) const;
    void setThreshold(const Threshold &t);
    void removeThreshold(const QString &name);

signals:
    void changed();

private:
    ProcessPrefsManager();
    static ProcessPrefsManager *instance;

    void load();
    void saveNames();
    void saveThresholds();

    QSet<QString>    mPinnedNames;
    QList<Threshold> mThresholds;
};

#endif // PROCESS_PREFS_MANAGER_H
