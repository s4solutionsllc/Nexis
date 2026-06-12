#ifndef APT_SOURCE_TOOL_LINUX_H
#define APT_SOURCE_TOOL_LINUX_H

#include <Tools/apt_source_tool.h>

class AptSourceToolLinux : public AptSourceTool
{
public:
    // RepositoryTool overrides — platform-neutral surface used by ToolManager
    // and any page that doesn't need APT-specific details.
    bool isAvailable() override;
    QList<RepositoryPtr> listRepositories() override;
    void removeRepository(const RepositoryPtr &repo) override;
    void addRepository(const QString &spec, bool isSource) override;
    RepositoryCapabilities capabilities() const override { return {true, true, true}; }

    // AptSourceTool overrides — APT-specific operations used by the Linux page.
    QList<APTSourcePtr> getSourceList() override;
    void removeAPTSource(const APTSourcePtr aptSource) override;
    void changeStatus(const APTSourcePtr aptSource, const bool status) override;
    void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource) override;

    // SSO-3728 / FW-01: writes a brand-new repository as a deb822 .sources file
    // under /etc/apt/sources.list.d/<fileStem>.sources with an explicit
    // Signed-By keyring path. No apt-key invocation. Used by addRepository()
    // on systems where deb822 is the norm (APT 3.1+ / Ubuntu 26.04+) and
    // exposed for callers that want to bypass add-apt-repository entirely.
    void addRepositoryDeb822(const QString &fileStem,
                             const APTSourcePtr &source);

    // True iff this system writes new repos as deb822 by default. Currently:
    // /etc/apt/sources.list.d/ubuntu.sources present (Ubuntu 26.04+, Debian
    // trixie+) — i.e. the stock distro source file is already deb822.
    bool prefersDeb822() const;
};

#endif // APT_SOURCE_TOOL_LINUX_H
