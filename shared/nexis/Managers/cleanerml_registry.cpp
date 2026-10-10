#include "cleanerml_registry.h"

#include <Tools/cleanerml_platform.h>
#include <Utils/command_util.h>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

// Q_INIT_RESOURCE must be called from outside any namespace.
static void initCleanerMLResource()
{
    Q_INIT_RESOURCE(cleanerml);
}

namespace CleanerMLRegistry {

QString bundledDir()
{
    initCleanerMLResource();
    return QStringLiteral(":/cleaners.d");
}

QString userDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cleaners.d");
}

QList<CleanerML::Cleaner> load(const QStringList &dirs, const QString &os, QList<CleanerML::ParseError> *errors)
{
    QHash<QString, CleanerML::Cleaner> byId;
    for (const QString &dir : dirs) {
        const CleanerML::ParseResult parsed = CleanerML::parseDirectory(dir, CleanerML::ParseMode::Lenient);
        if (errors)
            errors->append(parsed.errors);
        for (const CleanerML::Cleaner &cleaner : parsed.cleaners)
            byId.insert(cleaner.id, cleaner);
    }

    QList<CleanerML::Cleaner> cleaners;
    for (const CleanerML::Cleaner &cleaner : std::as_const(byId)) {
        const CleanerML::Cleaner usable = CleanerML::forPlatform(cleaner, os);
        if (!usable.options.isEmpty())
            cleaners.append(usable);
    }
    std::sort(cleaners.begin(), cleaners.end(), [](const CleanerML::Cleaner &a, const CleanerML::Cleaner &b) {
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
    return cleaners;
}

QList<CleanerML::Cleaner> loadDefault(QList<CleanerML::ParseError> *errors)
{
    return load({bundledDir(), userDir()}, CleanerML::currentOs(), errors);
}

bool hasData(const CleanerML::Cleaner &cleaner, const CleanerActionInterpreter::SandboxRoots &roots)
{
    const CleanerActionInterpreter interpreter(cleaner, {}, roots);
    for (const CleanerML::Option &option : cleaner.options) {
        for (const CleanerML::Action &action : option.actions) {
            const bool pattern = action.type == CleanerML::ActionType::Glob
                || (action.type == CleanerML::ActionType::SqliteVacuum && action.search == QLatin1String("glob"));
            for (const QString &path : interpreter.expandPath(action.path)) {
                // A glob's last component is a pattern; its directory is
                // what has to exist.
                if (QFileInfo::exists(pattern ? QFileInfo(path).path() : path))
                    return true;
            }
        }
    }
    return false;
}

bool isRunning(const CleanerML::Cleaner &cleaner)
{
    if (!CommandUtil::isExecutable(QStringLiteral("pgrep")))
        return false;
    for (const CleanerML::RunningCheck &check : cleaner.running) {
        if (check.type != QLatin1String("exe"))
            continue;
        if (CommandUtil::execWithStatus(QStringLiteral("pgrep"), {QStringLiteral("-x"), check.value}).ok())
            return true;
    }
    return false;
}

void BatchProvider::add(const CleanerML::Cleaner &cleaner, const QSet<QString> &optionIds,
                        const CleanerActionInterpreter::SandboxRoots &roots,
                        const QList<CleanerService::ExclusionEntry> &exclusions)
{
    if (optionIds.isEmpty() || mByCleanerId.contains(cleaner.id))
        return;
    auto interpreter = std::make_unique<CleanerActionInterpreter>(cleaner, optionIds, roots);
    interpreter->setExclusions(exclusions);
    mByCleanerId.insert(cleaner.id, interpreter.get());
    mInterpreters.push_back(std::move(interpreter));
}

void BatchProvider::scan(QAtomicInt *cancelled,
                         const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    for (const auto &interpreter : mInterpreters) {
        if (cancelled && cancelled->loadRelaxed())
            return;
        interpreter->scan(cancelled, itemFound);
    }
}

TrustSafetyActionResult BatchProvider::performItem(const TrustSafetyActionItem &item, bool dryRun)
{
    CleanerActionInterpreter *interpreter = mByCleanerId.value(item.categoryId);
    if (!interpreter) {
        TrustSafetyActionResult result;
        result.itemId = item.id;
        result.error = QObject::tr("No cleaner is loaded for this item.");
        return result;
    }
    return interpreter->performItem(item, dryRun);
}

}
