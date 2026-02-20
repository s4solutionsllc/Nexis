#ifndef APT_SOURCE_TOOL_LINUX_H
#define APT_SOURCE_TOOL_LINUX_H

#include <Tools/apt_source_tool.h>

class AptSourceToolLinux : public AptSourceTool
{
public:
    bool checkSourceRepository() override;
    QList<APTSourcePtr> getSourceList() override;
    void removeAPTSource(const APTSourcePtr aptSource) override;
    void changeStatus(const APTSourcePtr aptSource, const bool status) override;
    void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource) override;
    void addRepository(const QString &repository, const bool isSource) override;
};

#endif // APT_SOURCE_TOOL_LINUX_H
