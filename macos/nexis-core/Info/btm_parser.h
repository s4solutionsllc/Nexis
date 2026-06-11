// SSO-3738 / FW-10: parse `sfltool dumpbtm` into a structured BtmRecord list
// and derive orphan / duplicate / Apple-managed flags.
//
// The parser is pure (no Qt singletons, no I/O) so the test target can compile
// it directly per FR-127 and run on any host — `sfltool` itself is macOS-only
// but its output is plain text.

#ifndef BTM_PARSER_H
#define BTM_PARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

enum class BtmRecordType {
    Unknown,
    LegacyLoginItem,
    LoginItem,
    LaunchdAgent,
    LaunchdDaemon,
    HelperLauncher,
    AppExtension,
    MdmManaged,
    Daemon,
};

struct BtmRecord {
    QString userUuid;            // user-section UUID
    int recordNumber = 0;        // 1-based `Record item #N`
    QString uuid;                // per-record UUID
    QString name;
    QString developerName;
    QString teamIdentifier;
    QString typeRaw;             // exact token from dumpbtm
    BtmRecordType type = BtmRecordType::Unknown;
    QStringList dispositionFlags;
    int dispositionBits = -1;    // raw int when present, -1 otherwise
    QString identifier;
    QString urlString;           // file:// URL as printed
    QString executablePath;
    QString plistPath;
    QString container;
    QString parentIdentifier;
    QStringList embeddedIdentifiers;
    QStringList associatedBundleIdentifiers;
    int generation = -1;

    // Derived
    bool enabled = true;
    bool appleManaged = false;
    bool orphan = false;
    bool duplicateIdentifier = false;
    bool duplicateExecutable = false;
};

class BtmParser
{
public:
    using FileExistsFn = std::function<bool(const QString &)>;

    // Parse raw `sfltool dumpbtm` stdout into BtmRecords. Section headers
    // (`==== Records for user <UUID> ====`) bind subsequent records to that
    // user. Records start with `Record item #N` and run until the next
    // `Record item` or section header. Fields are `\t<Name>: <value>`.
    static QList<BtmRecord> parse(const QByteArray &raw);
    static QList<BtmRecord> parse(const QString &raw);

    // Mark `duplicateIdentifier` / `duplicateExecutable` on records whose
    // identifier or executable path appears more than once within the same
    // user section.
    static void flagDuplicates(QList<BtmRecord> &records);

    // Mark `orphan` on records whose executable AND plist paths (those that
    // are set) all return false from `exists`. Apple-managed records are
    // skipped. The predicate is injected so tests can be host-agnostic.
    static void flagOrphans(QList<BtmRecord> &records,
                            const FileExistsFn &exists);

    // True if `path` lives under an Apple-private launchd directory and
    // therefore should be treated as system-managed (no enable/disable,
    // no orphan flagging even if SIP hides it from us).
    static bool isApplePrivatePath(const QString &path);
};

#endif // BTM_PARSER_H
