#include "homebrew_tool_macos.h"
#include "Utils/command_util.h"
#include "Utils/brew_util.h"
#include <QDebug>
#include <QJsonDocument>

bool HomebrewToolMacOS::isAvailable()
{
    return !findBrew().isEmpty();
}

QList<RepositoryPtr> HomebrewToolMacOS::listRepositories()
{
    QList<RepositoryPtr> result;

    ExecResult execResult = CommandUtil::execWithStatus(findBrew(),
                                           {"info", "--json=v2", "--installed"},
                                           120000);
    if (!execResult.ok()) {
        qCritical() << "Failed to list Homebrew packages:" << execResult.error;
        return result;
    }

    QJsonDocument doc = QJsonDocument::fromJson(execResult.output.trimmed().toUtf8());

    if (doc.isNull()) {
        qCritical() << "Failed to parse brew info JSON";
        return result;
    }

    for (const BrewEntry &e : parseBrewJson(doc)) {
        RepositoryPtr repo(new Repository);
        repo->kind = Repository::Kind::HomebrewPackage;
        repo->id = e.identifier;
        repo->displayName = e.displayName.isEmpty() ? e.identifier : e.displayName;
        repo->description = e.description;
        repo->isActive = true;
        result.append(repo);
    }

    return result;
}

void HomebrewToolMacOS::removeRepository(const RepositoryPtr &repo)
{
    if (repo.isNull() || repo->id.isEmpty())
        return;

    // Homebrew rejects --cask on formulae and vice-versa; without per-entry
    // backend type we let `brew uninstall` resolve. (Cask vs formula is
    // surfaced separately via PackageTool when the UI needs that detail.)
    ExecResult result = CommandUtil::execWithStatus(findBrew(), {"uninstall", repo->id});
    if (!result.ok())
        qCritical() << "Failed to uninstall Homebrew package:" << result.error;
}

void HomebrewToolMacOS::addRepository(const QString &spec, bool isSource)
{
    Q_UNUSED(isSource);
    if (spec.isEmpty())
        return;

    ExecResult result = CommandUtil::execWithStatus(findBrew(), {"install", spec});
    if (!result.ok())
        qCritical() << "Failed to install Homebrew package:" << result.error;
}
