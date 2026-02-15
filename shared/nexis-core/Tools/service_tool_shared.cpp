// Shared Service methods — identical across platforms.
// Platform-specific ServiceTool methods live in
// linux/nexis-core/Tools/service_tool.cpp and macos/nexis-core/Tools/service_tool.cpp.

#include "service_tool.h"

Service::Service(const QString &name, const QString description, const bool status, const bool active) :
    name(name),
    description(description),
    status(status),
    active(active)
{ }
