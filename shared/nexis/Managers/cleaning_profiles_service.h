#ifndef CLEANING_PROFILES_SERVICE_H
#define CLEANING_PROFILES_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QFileInfoList>
#include <QHash>
#include <functional>

// FW-12: data-driven application cleaning profiles. Profiles are JSON
// descriptors loaded from a bundled resource directory and a user-writable
// directory under the Nexis config dir. Each profile lists glob patterns,
// a safety class (`safe` or `aggressive`), and an optional age policy.
//
// The loader is intentionally side-effect-free: it discovers, validates and
// returns Profile records. Expansion to real on-disk paths is a separate
// step (`expandPaths`) so unit tests can substitute a fake $HOME via the
// resolver seam.
class CleaningProfilesService : public QObject
{
    Q_OBJECT

public:
    enum class Safety {
        Safe,
        Aggressive
    };

    struct Profile {
        QString id;                  // unique, slug-style
        QString name;                // human-readable display name
        QString app;                 // app/family label (e.g. "Visual Studio Code")
        QString description;
        QStringList platforms;       // {"linux","macos"}; empty = both
        Safety safety = Safety::Safe;
        QStringList paths;           // raw glob patterns w/ $HOME, ~ placeholders
        int minAgeDays = 0;          // 0 = any age
        QString source;              // origin (bundled resource path or user file path)
    };

    struct ValidationError {
        QString file;                // source path (empty if from in-memory blob)
        QString id;                  // profile id (empty if parse failed)
        QString message;
    };

    // Optional placeholder resolver. Maps placeholder names like "HOME",
    // "XDG_CACHE_HOME" to absolute paths. Defaults to the live environment.
    using PathResolver = std::function<QString(const QString &placeholder)>;

    static CleaningProfilesService *ins();

    // Load all profiles for the current platform. Combines bundled profiles
    // (from `:/cleaning_profiles/<platform>/*.json`) and user profiles
    // (from `~/.config/Nexis/cleaning_profiles/*.json`); user profiles with
    // the same id override bundled ones.
    QList<Profile> loadAll();

    // Same as `loadAll`, but explicit about platform and directories — used by
    // tests to point at a `QTemporaryDir` for both bundled and user roots.
    QList<Profile> loadFrom(const QStringList &bundledRoots,
                            const QStringList &userRoots,
                            const QString &platform);

    // Parse a single profile JSON blob. Returns true on success; populates
    // `out`. On failure, fills `err.message`.
    static bool parseProfile(const QByteArray &json,
                             Profile &out,
                             ValidationError &err);

    // Expand a profile's glob patterns into matching QFileInfo entries.
    // Honors Q_OS via the resolver. Symlinks are returned as-is; the cleaner
    // decides whether to follow them.
    QFileInfoList expandPaths(const Profile &profile,
                              const PathResolver &resolver = nullptr) const;

    // Convenience: scan every profile, return the concatenated QFileInfoList
    // gated by `allowAggressive` and the cleaner exclusions list.
    // The optional `perProfile` out-param exposes the per-profile breakdown
    // for UI consumers that want grouped results.
    QFileInfoList scan(const QList<Profile> &profiles,
                       bool allowAggressive,
                       const std::function<bool(const QString &)> &isExcluded,
                       QHash<QString, QFileInfoList> *perProfile = nullptr,
                       const PathResolver &resolver = nullptr) const;

    // Helpers exposed for tests/UI.
    static Safety safetyFromString(const QString &s, bool *ok = nullptr);
    static QString safetyToString(Safety s);
    static QString currentPlatform();    // "linux" or "macos"
    static QString defaultUserProfilesDir();
    static QStringList defaultBundledProfileRoots();   // typically just ":/cleaning_profiles/<platform>"

    // Returns the validation errors collected by the most recent loadAll/loadFrom
    // call. Useful for the UI to surface "skipped malformed profile foo.json".
    QList<ValidationError> lastErrors() const { return mLastErrors; }

protected:
    CleaningProfilesService();
    virtual ~CleaningProfilesService() = default;

private:
    static CleaningProfilesService *instance;
    mutable QList<ValidationError> mLastErrors;

    static QString defaultResolverImpl(const QString &placeholder);
    QString resolvePlaceholders(const QString &input,
                                const PathResolver &resolver) const;
    QFileInfoList globExpand(const QString &resolvedPattern,
                             int minAgeDays) const;
    QList<Profile> readProfilesFromDir(const QString &dir,
                                       const QString &platform);
};

#endif // CLEANING_PROFILES_SERVICE_H
