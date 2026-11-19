#ifndef PERSISTENCE_WATCH_H
#define PERSISTENCE_WATCH_H

#include "nexis-core_global.h"

#include <Info/startup_info.h>

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// SSO-27149: report-only persistence watch. Keeps a baseline of every
// autostart / LaunchAgent / LaunchDaemon / cron entry the user has seen and
// reports what appeared (or changed what it runs) since. Nothing here blocks
// or removes anything; the user acknowledges to move the baseline forward.

struct PersistenceEntry {
    QString key;          // stable identity, see PersistenceWatch::keyFor()
    QString label;        // human-readable, only used for display
    QString fingerprint;  // hash of what the entry runs; empty for cron lines
};

struct PersistenceDiff {
    QStringList added;    // keys not in the baseline
    QStringList changed;  // keys whose fingerprint differs from the baseline
    QStringList removed;  // baseline keys no longer present

    bool hasFlagged() const { return !added.isEmpty() || !changed.isEmpty(); }
    QSet<QString> flagged() const;
};

struct CronEntry {
    QString schedule;  // "*/5 * * * *" or "@reboot"
    QString command;
    QString raw;       // trimmed original line, used as the identity
};

class NEXISCORESHARED_EXPORT PersistenceWatch
{
public:
    struct Evaluation {
        PersistenceDiff diff;
        bool seeded = false;  // no usable baseline existed, current state adopted
    };

    explicit PersistenceWatch(const QString &baselinePath);

    // Compares `current` with the stored baseline. With no readable baseline
    // (first run, or unreadable file) the current state is adopted silently:
    // flagging every pre-existing item as "new" would be noise, not signal.
    // Entries that vanished are dropped from the baseline so that an entry
    // that is removed and later comes back is flagged again.
    Evaluation evaluate(const QList<PersistenceEntry> &current);

    // Moves the baseline forward to `current`. A non-empty `keys` restricts
    // the update to those keys (everything else in the baseline is kept).
    bool acknowledge(const QList<PersistenceEntry> &current,
                     const QSet<QString> &keys = QSet<QString>());

    static PersistenceDiff diff(const QList<PersistenceEntry> &baseline,
                                const QList<PersistenceEntry> &current);

    static QByteArray serialize(const QList<PersistenceEntry> &entries);
    static bool deserialize(const QByteArray &data, QList<PersistenceEntry> *out);

    // Cron table text (`crontab -l`) -> jobs. Comments, blanks and VAR=value
    // lines are skipped.
    static QList<CronEntry> parseCrontab(const QString &text);

    static QString keyFor(const StartupAppData &item);
    static PersistenceEntry entryFor(const StartupAppData &item);
    static StartupAppData itemForCron(const CronEntry &cron);

    // Hash of the file with the "is it enabled" lines removed, so that
    // toggling an item on or off in Nexis (or the desktop's own settings)
    // is not reported as a change to what it runs.
    static QString fingerprintContent(const QByteArray &content);
    static QString fingerprintFile(const QString &path);

private:
    bool load(QList<PersistenceEntry> *out) const;
    bool save(const QList<PersistenceEntry> &entries) const;

    QString mPath;
};

#endif // PERSISTENCE_WATCH_H
