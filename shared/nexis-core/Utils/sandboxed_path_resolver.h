#ifndef SANDBOXED_PATH_RESOLVER_H
#define SANDBOXED_PATH_RESOLVER_H

#include <QList>
#include <QString>

#include "nexis-core_global.h"

// SSO-23859: confines glob/walk/regex path matching to a caller-supplied
// base directory (the user's home or cache dir). Confinement is checked
// against the canonical (symlink- and ".."-resolved) path of every
// candidate, not the literal pattern text, so neither a traversal segment
// nor a symlink hop inside the sandbox can make a match resolve outside
// baseDir. Deep Cleaning Engine action executors (SSO-23856 model) sit on
// top of this rather than walking the filesystem themselves.
class NEXISCORESHARED_EXPORT SandboxedPathResolver
{
public:
    enum class MatchKind {
        Glob,   // single-directory shell-style match under baseDir/subPath
        Walk,   // every file, recursively, under baseDir/subPath
        Regex,  // recursive walk under baseDir/subPath, kept iff the path
                // relative to baseDir matches `pattern` as a regex
    };

    struct MatchedFile {
        QString absolutePath;   // canonical; always confined to baseDir
        qint64 sizeBytes = -1;  // -1 if it could not be stat'd (e.g. TOCTOU race)
    };

    // `pattern` is interpreted per `kind` (ignored for Walk). Returns only
    // files whose canonical path is baseDir itself or a descendant of it —
    // anything else (a ".." traversal, an absolute-path pattern, a symlink
    // that resolves outside baseDir) is silently dropped, never surfaced.
    // Symlinked subdirectories are not descended into during Walk/Regex.
    static QList<MatchedFile> resolve(const QString &baseDir,
                                       const QString &subPath,
                                       const QString &pattern,
                                       MatchKind kind);

    // True iff candidatePath's canonical form is baseDir itself or a
    // descendant of it. Exposed standalone so one-off paths (and tests) can
    // be guarded without going through resolve().
    static bool isPathConfinedTo(const QString &candidatePath, const QString &baseDir);

private:
    SandboxedPathResolver();
};

#endif // SANDBOXED_PATH_RESOLVER_H
