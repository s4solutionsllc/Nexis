#ifndef REPO_HEALTH_TYPES_H
#define REPO_HEALTH_TYPES_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QMap>

struct RepoHealthIssue {
    enum Severity { Info, Warning, Error };
    Severity severity = Info;
    QString code;
    QString summary;
    QString detail;
    QString repairCmd;
    QString repairLabel;
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
};

using RepoHealthCache = QMap<QString, RepoHealthResult>;

#endif // REPO_HEALTH_TYPES_H
