#ifndef REPO_HEALTH_TYPES_H
#define REPO_HEALTH_TYPES_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QMap>
#include <QVariantMap>

struct RepoRepairAction {
    enum Type {
        RunCommand,
        ConvertToDeb822,
        RemoveDuplicate,
        DiagnoseConnection,
        DisableSource,
        EnableSource,
        RemoveSource,
        AskClaude
    };
    Type type;
    QString label;
    QString command;        // RunCommand only
    QVariantMap context;
};

struct RepoHealthIssue {
    enum Severity { Info, Warning, Error };
    Severity severity = Info;
    QString code;
    QString summary;
    QString detail;
    QList<RepoRepairAction> actions;
};

struct RepoHealthResult {
    enum Status { Unknown, Healthy, Warning, Error };
    Status status = Unknown;
    QString name;
    QString description;
    QList<RepoHealthIssue> issues;
    QDateTime lastChecked;
    QString releaseOrigin;
};

struct RepoKnownInfo {
    QString name;
    QString description;
    QString url;
    QString keyUrl;
};

using RepoHealthCache = QMap<QString, RepoHealthResult>;

#endif // REPO_HEALTH_TYPES_H
