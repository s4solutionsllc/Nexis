#include "shared_data_guard.h"

#include <QChar>
#include <QCoreApplication>

namespace SharedDataGuard {

namespace {

const QStringList &channelTokens()
{
    static const QStringList tokens = {
        QStringLiteral("beta"),     QStringLiteral("alpha"),
        QStringLiteral("nightly"),  QStringLiteral("canary"),
        QStringLiteral("dev"),      QStringLiteral("devel"),
        QStringLiteral("insiders"), QStringLiteral("insider"),
        QStringLiteral("preview"),  QStringLiteral("esr"),
        QStringLiteral("stable"),   QStringLiteral("unstable"),
        QStringLiteral("rc"),       QStringLiteral("snapshot"),
        QStringLiteral("bin"),      QStringLiteral("git"),
    };
    return tokens;
}

bool isSeparator(QChar c)
{
    return !c.isLetterOrNumber();
}

// `leaf` is exactly `id`, or `id` followed by a non-alphanumeric separator
// (".plist", "-cache", ...). Mirrors how the scanners decide a leaf matches.
bool belongsTo(const QString &leaf, const QString &id)
{
    if (id.isEmpty())
        return false;
    if (leaf.compare(id, Qt::CaseInsensitive) == 0)
        return true;
    return leaf.length() > id.length()
        && leaf.startsWith(id, Qt::CaseInsensitive)
        && isSeparator(leaf.at(id.length()));
}

} // namespace

QString familyBase(const QString &id)
{
    for (int i = id.length() - 1; i > 0; --i) {
        if (!isSeparator(id.at(i)))
            continue;
        const QString tail = id.mid(i + 1);
        if (channelTokens().contains(tail, Qt::CaseInsensitive))
            return id.left(i);
        break;
    }
    return id;
}

bool areVariants(const QString &a, const QString &b)
{
    return !a.isEmpty() && !b.isEmpty()
        && a.compare(b, Qt::CaseInsensitive) != 0
        && familyBase(a).compare(familyBase(b), Qt::CaseInsensitive) == 0;
}

Claim classify(const QString &leaf, const QString &matchedId, const QStringList &installedIds)
{
    if (matchedId.isEmpty())
        return {};

    // Rule 1: another installed app owns this leaf at least as specifically.
    const int matchedLen = belongsTo(leaf, matchedId) ? matchedId.length() : 0;
    const QString *owner = nullptr;
    for (const QString &id : installedIds) {
        if (belongsTo(leaf, id) && id.length() >= matchedLen
            && (!owner || id.length() > owner->length()))
            owner = &id;
    }
    if (owner) {
        return { *owner,
                 QCoreApplication::translate("SharedDataGuard",
                     "Also used by %1, which is still installed").arg(*owner) };
    }

    // Rule 2: unsuffixed id of a family whose channel variant is still installed.
    if (matchedId.compare(familyBase(matchedId), Qt::CaseInsensitive) == 0) {
        for (const QString &id : installedIds) {
            if (areVariants(matchedId, id)) {
                return { id,
                         QCoreApplication::translate("SharedDataGuard",
                             "Profile data is shared with %1, a variant that is still installed").arg(id) };
            }
        }
    }
    return {};
}

} // namespace SharedDataGuard
