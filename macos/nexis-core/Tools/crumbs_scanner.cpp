#include "crumbs_scanner.h"

#include "Utils/file_util.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace CrumbsScanner {

namespace {

QStringList searchRoots()
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return {
        home + "/Library/Preferences",
        home + "/Library/Application Support",
        home + "/Library/Caches",
        home + "/Library/Saved Application State",
        home + "/Library/Containers",
        home + "/Library/Logs",
    };
}

bool leafMatchesAnyId(const QString &leaf, const QStringList &bundleIds, QString *matched)
{
    for (const QString &id : bundleIds) {
        if (id.isEmpty())
            continue;
        if (leaf == id || leaf.startsWith(id + QLatin1String("."))
            || leaf.startsWith(id + QLatin1String("/"))
            || leaf.startsWith(id + QLatin1String("-")))
        {
            if (matched)
                *matched = id;
            return true;
        }
        // The Preferences dir typically contains "<bundleId>.plist" —
        // match as a filename-prefix too.
        if (leaf.startsWith(id)) {
            const int trailing = leaf.length() - id.length();
            const QChar next = leaf.at(id.length());
            if (trailing > 0 && !next.isLetterOrNumber()) {
                if (matched)
                    *matched = id;
                return true;
            }
        }
    }
    return false;
}

} // namespace

QList<CrumbCandidate> scanCrumbs(const QStringList &bundleIds)
{
    QList<CrumbCandidate> out;
    if (bundleIds.isEmpty())
        return out;

    for (const QString &rootPath : searchRoots()) {
        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QFileInfoList entries = root.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo &fi : entries) {
            QString matched;
            if (!leafMatchesAnyId(fi.fileName(), bundleIds, &matched))
                continue;

            CrumbCandidate c;
            c.path = fi.absoluteFilePath();
            c.sizeBytes = static_cast<qint64>(FileUtil::getFileSize(c.path));
            c.matchedBundleId = matched;
            out.append(c);
        }
    }

    std::sort(out.begin(), out.end(),
        [](const CrumbCandidate &a, const CrumbCandidate &b) {
            return a.sizeBytes > b.sizeBytes;
        });

    return out;
}

} // namespace CrumbsScanner
