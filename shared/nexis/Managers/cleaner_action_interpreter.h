// SSO-23859: executes a parsed CleanerML Cleaner's actions
// (delete/glob/walk/regex/truncate/sqlite.vacuum) as a TrustSafetyActionProvider,
// so callers get dry-run preview, live-confirm, and cancel for free from
// TrustSafetyPreviewDialog/TrustSafetyRunner (SSO-15380) without a new dialog.

#ifndef CLEANER_ACTION_INTERPRETER_H
#define CLEANER_ACTION_INTERPRETER_H

#include <Common/trust_safety_types.h>
#include <Managers/cleaner_service.h>
#include <Tools/cleanerml_model.h>

#include <QSet>
#include <QString>
#include <QStringList>

class CleanerActionInterpreter : public TrustSafetyActionProvider
{
public:
    // Paths are resolved from $$home$$/$$cache$$, the cleaner's own <var>
    // definitions (pass a cleaner already reduced by
    // CleanerML::forPlatform()), a leading "~", and $HOME / $XDG_CONFIG_HOME /
    // $XDG_CACHE_HOME / $XDG_DATA_HOME. A path that still holds an unknown
    // token is dropped at scan() time rather than guessed at. Everything is
    // confined to the sandbox roots.
    struct SandboxRoots {
        QString home;
        QString cache;
    };

    static SandboxRoots defaultSandboxRoots();

    CleanerActionInterpreter(CleanerML::Cleaner cleaner,
                              QSet<QString> selectedOptionIds,
                              SandboxRoots sandboxRoots);

    // Paths the user has excluded from cleaning are never listed or touched.
    void setExclusions(const QList<CleanerService::ExclusionEntry> &exclusions);

    // Every concrete path `rawPath` can stand for (several when a variable
    // has more than one value or globs). Empty when it cannot be resolved.
    QStringList expandPath(const QString &rawPath) const;

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override;

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override;

private:
    using ItemSink = std::function<void(const TrustSafetyActionItem &)>;

    void scanLiteralPath(const CleanerML::Option &option, const CleanerML::Action &action,
                         bool isTruncate, const ItemSink &itemFound);
    void scanPattern(const CleanerML::Option &option, const CleanerML::Action &action,
                     const ItemSink &itemFound);
    void scanVacuum(const CleanerML::Option &option, const CleanerML::Action &action,
                    const ItemSink &itemFound);
    // Fills the category, risk tier and any warning text, then emits.
    void emitItem(TrustSafetyActionItem item, const CleanerML::Option &option, const ItemSink &itemFound) const;

    QStringList expandTokens(const QString &rawPath, int depth) const;
    QStringList expandVar(const QString &name, int depth) const;
    // "~", $HOME and the XDG base directories; empty if another $NAME remains.
    QString expandEnvironment(const QString &path) const;

    // Root (home or cache) that confines candidatePath, or an empty string
    // if candidatePath escapes both.
    QString confiningRoot(const QString &candidatePath) const;
    // Confined, not a sandbox root itself, not excluded by the user. Cheap
    // enough to run per file, at scan time and again before touching disk.
    bool isAllowedTarget(const QString &path) const;
    // The shared lifecycle deny-list (system locations, root-owned or
    // package-owned paths, credential stores). It can spawn dpkg/rpm, so it
    // is asked once per literal target or searched directory at scan time,
    // not once per matched file.
    bool passesDenyList(const QString &path) const;

    CleanerML::Cleaner mCleaner;
    QSet<QString> mSelectedOptionIds;
    SandboxRoots mSandboxRoots;
    QList<CleanerService::ExclusionEntry> mExclusions;
};

#endif // CLEANER_ACTION_INTERPRETER_H
