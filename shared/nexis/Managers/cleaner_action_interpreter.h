// SSO-23859: executes a parsed CleanerML Cleaner's actions
// (delete/glob/walk/regex/truncate/sqlite.vacuum) as a TrustSafetyActionProvider,
// so callers get dry-run preview, live-confirm, and cancel for free from
// TrustSafetyPreviewDialog/TrustSafetyRunner (SSO-15380) without a new dialog.

#ifndef CLEANER_ACTION_INTERPRETER_H
#define CLEANER_ACTION_INTERPRETER_H

#include <Common/trust_safety_types.h>
#include <Tools/cleanerml_model.h>

#include <QSet>
#include <QString>

class CleanerActionInterpreter : public TrustSafetyActionProvider
{
public:
    // Only $$home$$/$$cache$$ (case-insensitive) are resolved here — these are
    // the two CleanerML variables generic to every app. App-specific tokens
    // (e.g. $$profile$$, $$base$$) require per-app profile discovery and are
    // out of scope for this generic interpreter (see SSO-23860); any action
    // whose path still contains such a token after substitution is dropped
    // at scan() time rather than guessed at.
    struct SandboxRoots {
        QString home;
        QString cache;
    };

    static SandboxRoots defaultSandboxRoots();

    CleanerActionInterpreter(CleanerML::Cleaner cleaner,
                              QSet<QString> selectedOptionIds,
                              SandboxRoots sandboxRoots);

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override;

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override;

private:
    void scanLiteralPath(const CleanerML::Action &action, bool isTruncate,
                          const std::function<void(const TrustSafetyActionItem &)> &itemFound);
    void scanPattern(const CleanerML::Action &action,
                      const std::function<void(const TrustSafetyActionItem &)> &itemFound);
    void scanVacuum(const CleanerML::Action &action,
                     const std::function<void(const TrustSafetyActionItem &)> &itemFound);

    // Expands $$home$$/$$cache$$ in rawPath; returns an empty string if an
    // unresolvable variable remains.
    QString expandVariables(const QString &rawPath) const;
    // Root (home or cache) that confines candidatePath, or an empty string
    // if candidatePath escapes both.
    QString confiningRoot(const QString &candidatePath) const;

    CleanerML::Cleaner mCleaner;
    QSet<QString> mSelectedOptionIds;
    SandboxRoots mSandboxRoots;
};

#endif // CLEANER_ACTION_INTERPRETER_H
