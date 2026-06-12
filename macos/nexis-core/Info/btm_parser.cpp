#include "btm_parser.h"

#include <QHash>
#include <QRegularExpression>

namespace {

BtmRecordType typeFromToken(const QString &token)
{
    static const QHash<QString, BtmRecordType> kMap = {
        {QStringLiteral("legacyLoginItem"), BtmRecordType::LegacyLoginItem},
        {QStringLiteral("loginItem"),       BtmRecordType::LoginItem},
        {QStringLiteral("launchdAgent"),    BtmRecordType::LaunchdAgent},
        {QStringLiteral("launchdDaemon"),   BtmRecordType::LaunchdDaemon},
        {QStringLiteral("helperLauncher"),  BtmRecordType::HelperLauncher},
        {QStringLiteral("appExtension"),    BtmRecordType::AppExtension},
        {QStringLiteral("mdmManaged"),      BtmRecordType::MdmManaged},
        {QStringLiteral("daemon"),          BtmRecordType::Daemon},
    };
    return kMap.value(token, BtmRecordType::Unknown);
}

// Parse the `( "a", "b" )` NSArray-style array literal that dumpbtm uses.
// Tolerates whitespace, single-line and multi-line forms. The parser is
// substring-based — full pretty-print arrays span multiple lines in dumpbtm,
// but the immediate per-record field is always single-line through Tahoe.
QStringList parseNsArray(const QString &value)
{
    QStringList out;
    static const QRegularExpression kQuoted(QStringLiteral("\"([^\"]*)\""));
    auto it = kQuoted.globalMatch(value);
    while (it.hasNext())
        out << it.next().captured(1);
    return out;
}

// `Disposition: [enabled, allowed, visible, notified] (11)`
void parseDisposition(const QString &value, QStringList &flags, int &bits)
{
    static const QRegularExpression kBracketed(QStringLiteral("\\[([^\\]]*)\\]"));
    static const QRegularExpression kParenInt(QStringLiteral("\\((\\d+)\\)"));

    QRegularExpressionMatch bm = kBracketed.match(value);
    if (bm.hasMatch()) {
        const QString inside = bm.captured(1);
        const QStringList parts = inside.split(',', Qt::SkipEmptyParts);
        for (const QString &p : parts) {
            QString t = p.trimmed();
            if (!t.isEmpty())
                flags << t;
        }
    }

    QRegularExpressionMatch pm = kParenInt.match(value);
    if (pm.hasMatch())
        bits = pm.captured(1).toInt();
}

bool dispositionImpliesEnabled(const QStringList &flags)
{
    // Apple writes either "enabled"/"disabled" and either "allowed"/
    // "disallowed". A record needs both enabled *and* allowed to actually run.
    bool enabled = true;
    bool allowed = true;
    for (const QString &f : flags) {
        if (f == QStringLiteral("disabled"))   enabled = false;
        if (f == QStringLiteral("disallowed")) allowed = false;
    }
    return enabled && allowed;
}

// Strip the `file://` prefix from a URL field, leaving a usable filesystem
// path. Returns the original string unchanged if it isn't a file URL.
QString fileUrlToPath(const QString &urlString)
{
    static const QString kFileScheme = QStringLiteral("file://");
    if (urlString.startsWith(kFileScheme)) {
        QString p = urlString.mid(kFileScheme.size());
        // Trim trailing slashes that file URLs for directories often carry.
        while (p.endsWith('/') && p.size() > 1)
            p.chop(1);
        return p;
    }
    return urlString;
}

} // namespace

bool BtmParser::isApplePrivatePath(const QString &path)
{
    if (path.isEmpty())
        return false;
    return path.startsWith(QStringLiteral("/System/Library/LaunchDaemons/"))
        || path.startsWith(QStringLiteral("/System/Library/LaunchAgents/"))
        || path.startsWith(QStringLiteral("/System/Library/LaunchAngels/"))
        || path.startsWith(QStringLiteral("/usr/libexec/"));
}

QList<BtmRecord> BtmParser::parse(const QByteArray &raw)
{
    return parse(QString::fromUtf8(raw));
}

QList<BtmRecord> BtmParser::parse(const QString &raw)
{
    QList<BtmRecord> records;
    if (raw.isEmpty())
        return records;

    static const QRegularExpression kSectionHeader(
        QStringLiteral("^=+\\s*Records for user\\s+([A-Fa-f0-9-]+)\\s*=+\\s*$"));
    static const QRegularExpression kRecordHeader(
        QStringLiteral("^Record item\\s*#?(\\d+)\\s*:?\\s*$"));

    const QStringList lines = raw.split('\n');

    QString currentUser;
    BtmRecord cur;
    bool inRecord = false;

    auto commit = [&]() {
        if (!inRecord)
            return;
        cur.type = typeFromToken(cur.typeRaw);
        cur.enabled = dispositionImpliesEnabled(cur.dispositionFlags);
        cur.appleManaged = isApplePrivatePath(cur.plistPath)
                           || isApplePrivatePath(cur.executablePath);
        records.append(cur);
        cur = BtmRecord{};
        inRecord = false;
    };

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();

        QRegularExpressionMatch sm = kSectionHeader.match(trimmed);
        if (sm.hasMatch()) {
            commit();
            currentUser = sm.captured(1);
            continue;
        }

        QRegularExpressionMatch rm = kRecordHeader.match(trimmed);
        if (rm.hasMatch()) {
            commit();
            cur = BtmRecord{};
            cur.userUuid = currentUser;
            cur.recordNumber = rm.captured(1).toInt();
            inRecord = true;
            continue;
        }

        if (!inRecord)
            continue;

        // Field lines: `\t<Name>: <value>`. We allow leading whitespace
        // generally so a fixture with spaces instead of tabs still parses,
        // but only the first ": " counts as the separator (URLs contain
        // colons).
        const int sep = line.indexOf(QStringLiteral(": "));
        if (sep < 0)
            continue;
        const QString key = line.left(sep).trimmed();
        const QString value = line.mid(sep + 2).trimmed();
        if (key.isEmpty())
            continue;

        if (key == QStringLiteral("UUID"))                    cur.uuid = value;
        else if (key == QStringLiteral("Name"))               cur.name = value;
        else if (key == QStringLiteral("Developer Name"))     cur.developerName = value;
        else if (key == QStringLiteral("Team Identifier"))    cur.teamIdentifier = value;
        else if (key == QStringLiteral("Type"))               cur.typeRaw = value;
        else if (key == QStringLiteral("Disposition"))
            parseDisposition(value, cur.dispositionFlags, cur.dispositionBits);
        else if (key == QStringLiteral("Identifier"))         cur.identifier = value;
        else if (key == QStringLiteral("URL"))                cur.urlString = value;
        else if (key == QStringLiteral("Executable Path"))    cur.executablePath = value;
        else if (key == QStringLiteral("Plist Path"))         cur.plistPath = value;
        else if (key == QStringLiteral("Container"))          cur.container = value;
        else if (key == QStringLiteral("Parent Identifier"))  cur.parentIdentifier = value;
        else if (key == QStringLiteral("Embedded Item Identifiers"))
            cur.embeddedIdentifiers = parseNsArray(value);
        else if (key == QStringLiteral("Associated Bundle Identifiers"))
            cur.associatedBundleIdentifiers = parseNsArray(value);
        else if (key == QStringLiteral("Generation"))
            cur.generation = value.toInt();
    }

    commit();
    return records;
}

void BtmParser::flagDuplicates(QList<BtmRecord> &records)
{
    // Count per (user, identifier) and (user, executable). A record is a
    // duplicate when its key appears more than once.
    QHash<QString, int> idCounts;
    QHash<QString, int> execCounts;

    auto idKey = [](const BtmRecord &r) {
        return r.userUuid + QStringLiteral("\x1f") + r.identifier;
    };
    auto execKey = [](const BtmRecord &r) {
        return r.userUuid + QStringLiteral("\x1f") + r.executablePath;
    };

    for (const BtmRecord &r : records) {
        if (!r.identifier.isEmpty())
            idCounts[idKey(r)]++;
        if (!r.executablePath.isEmpty())
            execCounts[execKey(r)]++;
    }

    for (BtmRecord &r : records) {
        if (!r.identifier.isEmpty() && idCounts.value(idKey(r)) > 1)
            r.duplicateIdentifier = true;
        if (!r.executablePath.isEmpty() && execCounts.value(execKey(r)) > 1)
            r.duplicateExecutable = true;
    }
}

void BtmParser::flagOrphans(QList<BtmRecord> &records,
                            const FileExistsFn &exists)
{
    if (!exists)
        return;

    for (BtmRecord &r : records) {
        if (r.appleManaged)
            continue;

        // Build the list of on-disk paths the record claims. If everything
        // we can check is missing, call it orphan.
        QStringList probes;
        if (!r.executablePath.isEmpty())
            probes << r.executablePath;
        if (!r.plistPath.isEmpty())
            probes << r.plistPath;
        if (probes.isEmpty() && !r.urlString.isEmpty()) {
            const QString p = fileUrlToPath(r.urlString);
            if (!p.isEmpty() && p.startsWith('/'))
                probes << p;
        }
        if (probes.isEmpty())
            continue;

        bool anyExists = false;
        for (const QString &p : probes) {
            if (exists(p)) {
                anyExists = true;
                break;
            }
        }
        if (!anyExists)
            r.orphan = true;
    }
}
