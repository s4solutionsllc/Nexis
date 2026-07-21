#include "leftover_scanner_linux.h"

#include "Tools/lifecycle_deny_list.h"
#include "Utils/file_util.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace LeftoverScannerLinux {

namespace {

struct SearchRoot {
    QString path;
    QString category;
};

QList<SearchRoot> searchRoots()
{
    const QString home = QDir::homePath();

    // XDG user directories — prefer env-var overrides then fallback defaults.
    const QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME", home + "/.config");
    const QString cacheHome  = qEnvironmentVariable("XDG_CACHE_HOME",  home + "/.cache");
    const QString dataHome   = qEnvironmentVariable("XDG_DATA_HOME",   home + "/.local/share");

    return {
        { configHome,                          QStringLiteral("Config") },
        { cacheHome,                           QStringLiteral("Cache") },
        { dataHome,                            QStringLiteral("Data") },
        { configHome + "/autostart",           QStringLiteral("Autostart") },
    };
}

// Returns true if `leaf` matches any entry in `names` via an exact or
// dot-delimited prefix match. Also matches reverse-DNS ids by checking
// whether any name is a suffix segment of the leaf (e.g. "firefox" matches
// "org.mozilla.firefox").
//
// The prefix check only accepts '.' as the delimiter (e.g. "firefox.desktop",
// "org.mozilla.firefox.conf") — NOT '-', '_', or ' '. Those characters are
// how genuinely distinct sibling packages are named on Linux (firefox vs
// firefox-esr, code vs code-insiders, gnome-terminal vs
// gnome-terminal-server), so treating them as separators would delete an
// unrelated package's data. See tests/core/test_app_leftovers_linux.cpp
// true-negative fixtures.
bool leafMatches(const QString &leaf, const QStringList &names, QString *matched)
{
    for (const QString &name : names) {
        if (name.isEmpty())
            continue;

        // Exact match.
        if (leaf.compare(name, Qt::CaseInsensitive) == 0) {
            if (matched) *matched = name;
            return true;
        }

        // Prefix followed by a literal '.' (e.g. "firefox.desktop",
        // "org.mozilla.firefox.conf").
        if (leaf.startsWith(name, Qt::CaseInsensitive)) {
            const int len = name.length();
            if (len < leaf.length() && leaf.at(len) == QLatin1Char('.')) {
                if (matched) *matched = name;
                return true;
            }
        }

        // Reverse-DNS: the leaf itself is a reverse-DNS id containing `name`
        // as the last dot-separated component (e.g. leaf="org.mozilla.firefox"
        // name="firefox").
        if (leaf.endsWith(QLatin1Char('.') + name, Qt::CaseInsensitive)) {
            if (matched) *matched = name;
            return true;
        }

        // The leaf ends with the reverse-DNS app id supplied as a name
        // (e.g. name="org.mozilla.firefox", leaf="org.mozilla.firefox.conf").
        if (leaf.startsWith(name + QLatin1Char('.'), Qt::CaseInsensitive)) {
            if (matched) *matched = name;
            return true;
        }
    }
    return false;
}

} // namespace

QList<LeftoverCandidate> scanLeftovers(const QStringList &packageNames)
{
    QList<LeftoverCandidate> out;
    if (packageNames.isEmpty())
        return out;

    for (const SearchRoot &root : searchRoots()) {
        QDir dir(root.path);
        if (!dir.exists())
            continue;

        const QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);

        for (const QFileInfo &fi : entries) {
            QString matched;
            if (!leafMatches(fi.fileName(), packageNames, &matched))
                continue;

            // CISO §2: canonicalize before deny-list check. LifecycleDenyList is
            // the single centralized deny-list (SSO-15386/SSO-15373) shared with
            // the orphan scanner — do not reintroduce a parallel copy here.
            const QString canonical = fi.canonicalFilePath();
            if (canonical.isEmpty())
                continue; // dangling symlink — skip
            if (!LifecycleDenyList::isSafe(canonical))
                continue;

            LeftoverCandidate c;
            c.path          = fi.absoluteFilePath();
            c.canonicalPath = canonical;
            c.sizeBytes     = FileUtil::getFileSize(canonical);
            c.category      = root.category;
            c.matchedName   = matched;
            out.append(c);
        }
    }

    std::sort(out.begin(), out.end(),
        [](const LeftoverCandidate &a, const LeftoverCandidate &b) {
            return a.sizeBytes > b.sizeBytes;
        });

    return out;
}

} // namespace LeftoverScannerLinux
