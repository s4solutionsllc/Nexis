#ifndef APP_IN_USE_GATE_H
#define APP_IN_USE_GATE_H

#include <QHash>
#include <QString>
#include <QStringList>

// SSO-27095: decides whether a path the cleaner is about to delete belongs to
// an application that is open right now. Browser, Slack, Discord, VS Code and
// similar profiles are live databases/caches the owning process holds open;
// deleting under it corrupts the app's state or makes it re-download on next
// use. CleanerService skips such paths and reports them instead.
//
// Ownership is by location, not by which cleaner category found the path, so
// the same rule protects the legacy categories (browser privacy, application
// caches) and the data-driven cleaning profiles alike.
class AppInUseGate
{
public:
    struct Rule {
        QString app;                 // display label, used in the skip report
        QStringList processes;       // exact process names (pgrep -x)
        QStringList homeRelativeRoots; // directories under $HOME the app owns
    };

    // `homeRoot`: the effective home directory the rules are resolved
    // against. Defaults to QDir::homePath() — the same root the scanners use.
    explicit AppInUseGate(const QString &homeRoot = QString());
    virtual ~AppInUseGate() = default;

    // Built-in rules for the apps whose bundled profiles and legacy cleaner
    // categories touch live profile data.
    static QList<Rule> builtinRules();

    // The label of the running app that owns `path`, or an empty string when
    // no rule matches or the owning app is not running. Process lookups are
    // memoised for the lifetime of the gate, so create one gate per clean run.
    QString runningOwnerOf(const QString &path);

protected:
    // Test seam; production shells out to `pgrep -x`.
    virtual bool isProcessRunning(const QString &processName) const;

private:
    bool isAnyRunning(const QStringList &processes);

    QString mHomeRoot;
    QList<Rule> mRules;
    QHash<QString, bool> mRunningCache;
};

#endif // APP_IN_USE_GATE_H
