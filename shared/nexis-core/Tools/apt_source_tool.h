#ifndef AptSourceTool_H
#define AptSourceTool_H

#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QSharedPointer>

class APTSource {
public:
    QString filePath;
    bool isSource;
    QString options;
    QString uri;
    QString suites;
    QString components;

    QString source;
    bool isActive;
};

typedef QSharedPointer<APTSource> APTSourcePtr;

class AptSourceTool
{
public:
    static bool checkSourceRepository();
    static QList<APTSourcePtr> getSourceList();
    static void removeAPTSource(const APTSourcePtr aptSource);
    static void changeStatus(const APTSourcePtr aptSource, const bool status);
    static void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource);
    static void addRepository(const QString &repository, const bool isSource);
};

#endif // AptSourceTool_H
