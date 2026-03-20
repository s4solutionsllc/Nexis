#include "repo_knowledge_base.h"

#include <QUrl>
#include <QRegularExpression>

struct RepoPattern {
    const char *pattern;
    const char *name;
    const char *description;
};

static const RepoPattern s_knownRepos[] = {
    // Ubuntu — specific mirrors before generic
    { "security.ubuntu.com",        QT_TR_NOOP("Ubuntu Security"),      QT_TR_NOOP("Official Ubuntu security updates repository") },
    { "ports.ubuntu.com",           QT_TR_NOOP("Ubuntu Ports"),         QT_TR_NOOP("Ubuntu repository for ARM, RISC-V and other non-x86 architectures") },
    { "archive.ubuntu.com",         QT_TR_NOOP("Ubuntu Main"),          QT_TR_NOOP("Official Ubuntu main archive repository") },
    { "old-releases.ubuntu.com",    QT_TR_NOOP("Ubuntu Old Releases"),  QT_TR_NOOP("Archive of EOL Ubuntu release packages") },

    // Debian
    { "deb.debian.org/debian-security", QT_TR_NOOP("Debian Security"),  QT_TR_NOOP("Official Debian security updates repository") },
    { "deb.debian.org",             QT_TR_NOOP("Debian Official"),       QT_TR_NOOP("Official Debian package repository") },
    { "ftp.debian.org",             QT_TR_NOOP("Debian Official"),       QT_TR_NOOP("Official Debian package repository") },

    // Launchpad PPAs — specific PPAs before generic fallback
    { "launchpadcontent.net/deadsnakes", QT_TR_NOOP("Deadsnakes PPA"),  QT_TR_NOOP("PPA providing newer Python versions for Ubuntu") },
    { "launchpad.net/~deadsnakes",       QT_TR_NOOP("Deadsnakes PPA"),  QT_TR_NOOP("PPA providing newer Python versions for Ubuntu") },
    { "launchpadcontent.net/git-core",   QT_TR_NOOP("Git Core PPA"),    QT_TR_NOOP("PPA providing the latest stable Git releases") },
    { "launchpad.net/~git-core",         QT_TR_NOOP("Git Core PPA"),    QT_TR_NOOP("PPA providing the latest stable Git releases") },
    { "launchpadcontent.net",       QT_TR_NOOP("Launchpad PPA"),         QT_TR_NOOP("Third-party Personal Package Archive hosted on Launchpad") },
    { "launchpad.net",              QT_TR_NOOP("Launchpad PPA"),         QT_TR_NOOP("Third-party Personal Package Archive hosted on Launchpad") },

    // Microsoft — specific repos before generic microsoft.com
    { "packages.microsoft.com/repos/vscode",  QT_TR_NOOP("VS Code"),         QT_TR_NOOP("Microsoft Visual Studio Code editor repository") },
    { "packages.microsoft.com/repos/edge",    QT_TR_NOOP("Microsoft Edge"),  QT_TR_NOOP("Microsoft Edge browser repository") },
    { "packages.microsoft.com/repos/teams",   QT_TR_NOOP("Microsoft Teams"), QT_TR_NOOP("Microsoft Teams collaboration app repository") },
    { "packages.microsoft.com",               QT_TR_NOOP("Microsoft Packages"), QT_TR_NOOP("Official Microsoft Linux software repository") },

    // Google
    { "dl.google.com/linux/chrome",      QT_TR_NOOP("Google Chrome"),    QT_TR_NOOP("Official Google Chrome browser repository") },
    { "dl.google.com/linux/earth",       QT_TR_NOOP("Google Earth"),     QT_TR_NOOP("Official Google Earth repository") },
    { "packages.cloud.google.com",       QT_TR_NOOP("Google Cloud"),     QT_TR_NOOP("Google Cloud SDK and Kubernetes tools repository") },

    // NodeSource
    { "deb.nodesource.com",         QT_TR_NOOP("NodeSource"),            QT_TR_NOOP("NodeSource Node.js binary distributions repository") },

    // Docker
    { "download.docker.com",        QT_TR_NOOP("Docker CE"),             QT_TR_NOOP("Official Docker Community Edition repository") },

    // PostgreSQL
    { "apt.postgresql.org",         QT_TR_NOOP("PostgreSQL"),            QT_TR_NOOP("Official PostgreSQL Global Development Group repository") },

    // MySQL
    { "repo.mysql.com",             QT_TR_NOOP("MySQL"),                 QT_TR_NOOP("Official Oracle MySQL repository") },

    // GitHub CLI
    { "cli.github.com/packages",    QT_TR_NOOP("GitHub CLI"),            QT_TR_NOOP("Official GitHub CLI tool repository") },

    // Grafana
    { "apt.grafana.com",            QT_TR_NOOP("Grafana"),               QT_TR_NOOP("Official Grafana Labs monitoring platform repository") },

    // HashiCorp
    { "apt.releases.hashicorp.com", QT_TR_NOOP("HashiCorp"),             QT_TR_NOOP("Official HashiCorp tools repository (Terraform, Vault, Consul)") },

    // Mozilla
    { "packages.mozilla.org",       QT_TR_NOOP("Mozilla APT"),           QT_TR_NOOP("Official Mozilla APT repository for Firefox and Thunderbird") },

    // Brave Browser
    { "brave-browser-apt-release.s3.brave.com", QT_TR_NOOP("Brave Browser"), QT_TR_NOOP("Official Brave browser repository") },
    { "apt.brave.com",              QT_TR_NOOP("Brave Browser"),         QT_TR_NOOP("Official Brave browser repository") },

    // Dropbox
    { "linux.dropbox.com",          QT_TR_NOOP("Dropbox"),               QT_TR_NOOP("Official Dropbox Linux client repository") },

    // Steam / Valve
    { "repo.steampowered.com",      QT_TR_NOOP("Steam"),                 QT_TR_NOOP("Official Valve Steam client repository") },
    { "steamdeb.akamaized.net",     QT_TR_NOOP("Steam"),                 QT_TR_NOOP("Official Valve Steam client repository") },

    // Spotify
    { "repository.spotify.com",     QT_TR_NOOP("Spotify"),               QT_TR_NOOP("Official Spotify music streaming client repository") },

    // Sublime Text
    { "download.sublimetext.com",   QT_TR_NOOP("Sublime Text"),          QT_TR_NOOP("Official Sublime Text editor repository") },

    // VSCodium
    { "paulcarroty.gitlab.io/vscodium-deb-rpm-repo", QT_TR_NOOP("VSCodium"), QT_TR_NOOP("Community-maintained binary releases of VS Code without telemetry") },
    { "download.vscodium.com",      QT_TR_NOOP("VSCodium"),              QT_TR_NOOP("Community-maintained binary releases of VS Code without telemetry") },

    // openSUSE Build Service
    { "download.opensuse.org",      QT_TR_NOOP("openSUSE Build Service"), QT_TR_NOOP("openSUSE Build Service (OBS) package repository") },
};

static constexpr int s_knownReposCount = static_cast<int>(sizeof(s_knownRepos) / sizeof(s_knownRepos[0]));

RepoKnownInfo RepoKnowledgeBase::lookup(const QString &uri)
{
    const QString lower = uri.toLower();
    for (int i = 0; i < s_knownReposCount; ++i) {
        if (lower.contains(QString::fromLatin1(s_knownRepos[i].pattern).toLower())) {
            RepoKnownInfo info;
            info.name        = QObject::tr(s_knownRepos[i].name);
            info.description = QObject::tr(s_knownRepos[i].description);
            info.url         = uri;
            return info;
        }
    }
    return {};
}

QString RepoKnowledgeBase::domainFromUri(const QString &uri)
{
    QUrl url(uri);
    if (url.isValid() && !url.host().isEmpty())
        return url.host();

    static const QRegularExpression re(
        QStringLiteral(R"(https?://([^/\s]+))"),
        QRegularExpression::CaseInsensitiveOption
    );
    const QRegularExpressionMatch m = re.match(uri);
    if (m.hasMatch())
        return m.captured(1);

    return {};
}
