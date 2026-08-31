#include "sandboxed_path_resolver.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// QFileInfo::canonicalFilePath() resolves symlinks and ".." but returns an
// empty string for a path that doesn't exist on disk (e.g. a traversal
// pattern probed before we know whether the target is real). Fall back to a
// lexically-cleaned absolute path so confinement checks still have
// something to compare against.
QString canonicalOrCleaned(const QString &path)
{
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;
    return QDir::cleanPath(info.absoluteFilePath());
}

} // namespace

SandboxedPathResolver::SandboxedPathResolver()
{
}

bool SandboxedPathResolver::isPathConfinedTo(const QString &candidatePath, const QString &baseDir)
{
    const QString base = canonicalOrCleaned(baseDir);
    const QString candidate = canonicalOrCleaned(candidatePath);

    if (base.isEmpty() || candidate.isEmpty())
        return false;

    if (candidate == base)
        return true;

    return candidate.startsWith(base + QLatin1Char('/'));
}

QList<SandboxedPathResolver::MatchedFile> SandboxedPathResolver::resolve(
    const QString &baseDir, const QString &subPath, const QString &pattern, MatchKind kind)
{
    QList<MatchedFile> matches;

    const QString base = canonicalOrCleaned(baseDir);
    if (base.isEmpty())
        return matches;

    const QString startDir = QDir::cleanPath(
        QDir(baseDir).absoluteFilePath(subPath.isEmpty() ? QStringLiteral(".") : subPath));

    if (!isPathConfinedTo(startDir, baseDir))
        return matches;

    if (!QFileInfo::exists(startDir) || !QFileInfo(startDir).isDir())
        return matches;

    auto tryAdd = [&](const QString &absPath) {
        if (!isPathConfinedTo(absPath, baseDir))
            return;

        QFileInfo info(absPath);
        if (!info.isFile())
            return;

        MatchedFile match;
        match.absolutePath = canonicalOrCleaned(absPath);
        match.sizeBytes = info.exists() ? info.size() : -1;
        matches.append(match);
    };

    switch (kind) {
    case MatchKind::Glob: {
        QDir dir(startDir);
        const QStringList nameFilters = {pattern.isEmpty() ? QStringLiteral("*") : pattern};
        const QStringList entries =
            dir.entryList(nameFilters, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QString &name : entries)
            tryAdd(dir.absoluteFilePath(name));
        break;
    }
    case MatchKind::Walk: {
        // No FollowSymlinks: a symlinked subdirectory inside the sandbox is
        // not descended into, closing off that escape vector at the walk
        // level (a symlinked *file* is still caught by tryAdd's canonical
        // confinement check below).
        QDirIterator it(startDir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                         QDirIterator::Subdirectories);
        while (it.hasNext())
            tryAdd(it.next());
        break;
    }
    case MatchKind::Regex: {
        const QRegularExpression re(pattern);
        if (!re.isValid())
            break;

        QDirIterator it(startDir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                         QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString absPath = it.next();
            const QString relPath = QDir(base).relativeFilePath(canonicalOrCleaned(absPath));
            if (re.match(relPath).hasMatch())
                tryAdd(absPath);
        }
        break;
    }
    }

    return matches;
}
