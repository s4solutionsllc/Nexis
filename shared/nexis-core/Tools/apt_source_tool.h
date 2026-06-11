#ifndef AptSourceTool_H
#define AptSourceTool_H

#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include "repository_tool.h"
#include <QSharedPointer>

class APTSource {
public:
    enum Format { Legacy, Deb822 };

    QString filePath;
    bool isSource = false;
    QString options;
    QString uri;
    QString suites;
    QString components;
    QString source;
    bool isActive = false;
    Format format = Legacy;
    QString signedByPath;
};

typedef QSharedPointer<APTSource> APTSourcePtr;

// APT-specific extension of the platform-neutral RepositoryTool. Only the
// Linux implementation provides this; the macOS Homebrew implementation
// satisfies RepositoryTool directly without inheriting these APT-specific
// operations (no more changeSource/changeStatus shoehorning).
class AptSourceTool : public RepositoryTool
{
public:
    ~AptSourceTool() override = default;

    virtual QList<APTSourcePtr> getSourceList() = 0;
    virtual void removeAPTSource(const APTSourcePtr aptSource) = 0;
    virtual void changeStatus(const APTSourcePtr aptSource, const bool status) = 0;
    virtual void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource) = 0;

    // Static parsing methods for testability (FR-76).
    // Parses a single legacy .list format line into an APTSource.
    // Returns nullptr if the line doesn't match the expected format.
    static APTSourcePtr parseSourceListLine(const QString &line,
                                            const QString &binaryType,
                                            const QString &sourceType);

    // Parses a deb822 format stanza (key-value block) into APTSource entries.
    // Multi-suite stanzas are expanded into one entry per suite.
    // Returns empty list if stanza doesn't contain the required Types field.
    static QList<APTSourcePtr> parseDeb822Stanza(const QString &stanzaText,
                                                  const QString &binaryType,
                                                  const QString &sourceType);
};

#endif // AptSourceTool_H
