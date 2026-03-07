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
    virtual ~AptSourceTool() = default;

    virtual bool checkSourceRepository() = 0;
    virtual QList<APTSourcePtr> getSourceList() = 0;
    virtual void removeAPTSource(const APTSourcePtr aptSource) = 0;
    virtual void changeStatus(const APTSourcePtr aptSource, const bool status) = 0;
    virtual void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource) = 0;
    virtual void addRepository(const QString &repository, const bool isSource) = 0;

    // Static parsing methods for testability (FR-76).
    // Parses a single legacy .list format line into an APTSource.
    // Returns nullptr if the line doesn't match the expected format.
    static APTSourcePtr parseSourceListLine(const QString &line,
                                            const QString &binaryType,
                                            const QString &sourceType);

    // Parses a deb822 format stanza (key-value block) into an APTSource.
    // Returns nullptr if stanza doesn't contain the required Types field.
    static APTSourcePtr parseDeb822Stanza(const QString &stanzaText,
                                          const QString &binaryType,
                                          const QString &sourceType);
};

#endif // AptSourceTool_H
