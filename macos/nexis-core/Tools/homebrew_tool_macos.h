#ifndef HOMEBREW_TOOL_MACOS_H
#define HOMEBREW_TOOL_MACOS_H

#include <Tools/repository_tool.h>

// macOS Homebrew "software sources" backend. Implements only the platform-
// neutral RepositoryTool surface — no APT-specific changeSource/changeStatus
// shoehorning. Installed packages (formulae + casks) are returned as generic
// Repository entries with kind = HomebrewPackage.
class HomebrewToolMacOS : public RepositoryTool
{
public:
    bool isAvailable() override;
    QList<RepositoryPtr> listRepositories() override;
    void removeRepository(const RepositoryPtr &repo) override;
    void addRepository(const QString &spec, bool isSource) override;

    // Homebrew packages can't be toggled or edited in place — uninstall and
    // reinstall are the only meaningful operations. Adding via `brew install`
    // is supported.
    RepositoryCapabilities capabilities() const override { return {false, false, true}; }
};

#endif // HOMEBREW_TOOL_MACOS_H
