#include "cleanerml_platform.h"

#include <QRegularExpression>

namespace CleanerML {

QString currentOs()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

bool osMatches(const QStringList &os, const QString &current)
{
    if (os.isEmpty())
        return true;
    for (const QString &raw : os) {
        const QString name = raw.trimmed().toLower();
        if (name == current)
            return true;
        if (name == QLatin1String("darwin") && current == QLatin1String("macos"))
            return true;
        if (name == QLatin1String("unix")
            && (current == QLatin1String("linux") || current == QLatin1String("macos")))
            return true;
    }
    return false;
}

bool pathIsForeign(const QString &path, const QHash<QString, QList<VarValue>> &vars)
{
    if (path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char('%')))
        return true;

    static const QRegularExpression tokenRe(QStringLiteral("\\$\\$([A-Za-z0-9_-]+)\\$\\$"));
    QRegularExpressionMatchIterator it = tokenRe.globalMatch(path);
    while (it.hasNext()) {
        const QString name = it.next().captured(1).toLower();
        if (name == QLatin1String("home") || name == QLatin1String("cache"))
            continue;
        if (vars.value(name).isEmpty())
            return true;
    }
    return false;
}

Cleaner forPlatform(const Cleaner &cleaner, const QString &os)
{
    Cleaner result = cleaner;
    result.options.clear();
    result.vars.clear();
    result.running.clear();

    if (!osMatches(cleaner.os, os))
        return result;

    for (auto it = cleaner.vars.constBegin(); it != cleaner.vars.constEnd(); ++it) {
        QList<VarValue> values;
        for (const VarValue &value : it.value()) {
            if (osMatches(value.os, os) && !pathIsForeign(value.value, {}))
                values.append(value);
        }
        if (!values.isEmpty())
            result.vars.insert(it.key(), values);
    }

    for (const RunningCheck &check : cleaner.running) {
        if (osMatches(check.os, os))
            result.running.append(check);
    }

    for (const Option &option : cleaner.options) {
        if (!osMatches(option.os, os))
            continue;

        Option kept = option;
        kept.actions.clear();
        bool runnable = true;
        for (const Action &action : option.actions) {
            if (!osMatches(action.os, os))
                continue;
            if (action.type == ActionType::Unsupported || action.search == QLatin1String("deep")) {
                runnable = false;
                break;
            }
            if (pathIsForeign(action.path, result.vars))
                continue;
            kept.actions.append(action);
        }
        if (runnable && !kept.actions.isEmpty())
            result.options.append(kept);
    }
    return result;
}

} // namespace CleanerML
