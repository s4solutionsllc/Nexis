#ifndef REPOSITORY_TOOL_H
#define REPOSITORY_TOOL_H

#include <QString>
#include <QList>
#include <QSharedPointer>

// Platform-neutral software-source abstraction shared between Linux APT and
// macOS Homebrew. The kind enum lets a consumer know what backend is in play
// without inspecting strings — values are stable strings used in cache keys
// and tests.
class Repository {
public:
    enum class Kind { AptSource, HomebrewPackage };

    Kind kind = Kind::AptSource;

    // Stable identifier used for cache keys and equality. For APT this is the
    // raw source line; for Homebrew it is the formula/cask token.
    QString id;

    // Short label suitable for showing in the list (e.g. the deb line stripped
    // of options, or "package-name" / "cask-token").
    QString displayName;

    // Longer description for the detail panel (deb suites/components blob, or
    // the Homebrew formula/cask desc field).
    QString description;

    // True iff the repository is currently enabled. Always true for installed
    // Homebrew packages; reflects the commented/Enabled flag on APT sources.
    bool isActive = true;
};

using RepositoryPtr = QSharedPointer<Repository>;

// Capabilities the page UI can query without down-casting. Defaults are set
// to the most permissive APT-style behaviour; the macOS HomebrewTool turns
// off the ones that don't apply.
struct RepositoryCapabilities {
    bool canToggle = true;     // enable/disable an existing entry
    bool canEdit = true;       // edit URI/suites/components in-place
    bool canAdd = true;        // add a new repository or install a package
};

// Platform-neutral interface every "software sources" backend implements.
// Linux APT extends this with APT-specific methods on AptSourceTool; the
// macOS implementation (HomebrewTool) implements only this base interface.
class RepositoryTool
{
public:
    virtual ~RepositoryTool() = default;

    // True if the backend is usable on this system (apt sources.list.d present,
    // or `brew` resolvable). Drives whether the nav button shows at all.
    virtual bool isAvailable() = 0;

    virtual QList<RepositoryPtr> listRepositories() = 0;
    virtual void removeRepository(const RepositoryPtr &repo) = 0;
    virtual void addRepository(const QString &spec, bool isSource) = 0;

    virtual RepositoryCapabilities capabilities() const { return {}; }
};

#endif // REPOSITORY_TOOL_H
