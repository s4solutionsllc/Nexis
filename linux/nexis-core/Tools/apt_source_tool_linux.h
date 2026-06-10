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
};

#endif // APT_SOURCE_TOOL_LINUX_H
