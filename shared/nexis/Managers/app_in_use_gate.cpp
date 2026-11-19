#include "app_in_use_gate.h"

#include <Utils/command_util.h>

#include <QDir>
#include <QFileInfo>

AppInUseGate::AppInUseGate(const QString &homeRoot)
    : mRules(builtinRules())
{
    const QString raw = QDir::cleanPath(homeRoot.isEmpty() ? QDir::homePath() : homeRoot);
    const QString canonical = QFileInfo(raw).canonicalFilePath();
    mHomeRoot = canonical.isEmpty() ? raw : canonical;
}

QList<AppInUseGate::Rule> AppInUseGate::builtinRules()
{
    // Roots from both platforms live in one table: a root that does not exist
    // on this OS simply never matches. Process names are what pgrep -x sees
    // (the executable name, not the bundle name, on Linux).
    return {
        {QStringLiteral("Google Chrome"),
         {QStringLiteral("chrome"), QStringLiteral("Google Chrome")},
         {QStringLiteral(".cache/google-chrome"), QStringLiteral(".config/google-chrome"),
          QStringLiteral("Library/Caches/Google/Chrome"),
          QStringLiteral("Library/Application Support/Google/Chrome")}},
        {QStringLiteral("Chromium"),
         {QStringLiteral("chromium"), QStringLiteral("chromium-browser")},
         {QStringLiteral(".cache/chromium"), QStringLiteral(".config/chromium")}},
        {QStringLiteral("Microsoft Edge"),
         {QStringLiteral("msedge"), QStringLiteral("Microsoft Edge")},
         {QStringLiteral(".cache/microsoft-edge"), QStringLiteral(".config/microsoft-edge"),
          QStringLiteral("Library/Caches/Microsoft Edge"),
          QStringLiteral("Library/Application Support/Microsoft Edge")}},
        {QStringLiteral("Brave"),
         {QStringLiteral("brave"), QStringLiteral("Brave Browser")},
         {QStringLiteral(".cache/BraveSoftware"), QStringLiteral(".config/BraveSoftware"),
          QStringLiteral("Library/Caches/BraveSoftware"),
          QStringLiteral("Library/Application Support/BraveSoftware")}},
        {QStringLiteral("Opera"),
         {QStringLiteral("opera")},
         {QStringLiteral(".cache/opera"), QStringLiteral(".config/opera")}},
        {QStringLiteral("Mozilla Firefox"),
         {QStringLiteral("firefox"), QStringLiteral("firefox-bin")},
         {QStringLiteral(".cache/mozilla/firefox"), QStringLiteral(".mozilla/firefox"),
          QStringLiteral("Library/Caches/Firefox"),
          QStringLiteral("Library/Caches/Mozilla/Firefox"),
          QStringLiteral("Library/Application Support/Firefox")}},
        {QStringLiteral("Safari"),
         {QStringLiteral("Safari")},
         {QStringLiteral("Library/Caches/com.apple.Safari"),
          QStringLiteral("Library/Caches/com.apple.Safari.SafeBrowsing"),
          QStringLiteral("Library/Caches/com.apple.WebKit.PluginProcess"),
          QStringLiteral("Library/Safari")}},
        {QStringLiteral("Slack"),
         {QStringLiteral("slack"), QStringLiteral("Slack")},
         {QStringLiteral(".config/Slack"), QStringLiteral("Library/Application Support/Slack")}},
        {QStringLiteral("Discord"),
         {QStringLiteral("discord"), QStringLiteral("Discord")},
         {QStringLiteral(".config/discord"), QStringLiteral("Library/Application Support/discord")}},
        {QStringLiteral("Visual Studio Code"),
         // macOS runs VS Code's main process as the generic "Electron"
         // binary, so any open Electron app also holds the Code profile; that
         // can only cause an extra skip, never a deletion.
         {QStringLiteral("code"), QStringLiteral("Code"), QStringLiteral("Electron")},
         {QStringLiteral(".config/Code"), QStringLiteral("Library/Application Support/Code")}},
        {QStringLiteral("Spotify"),
         {QStringLiteral("spotify"), QStringLiteral("Spotify")},
         {QStringLiteral(".cache/spotify"), QStringLiteral(".config/spotify"),
          QStringLiteral("Library/Caches/com.spotify.client"),
          QStringLiteral("Library/Application Support/Spotify")}},
        {QStringLiteral("Zoom"),
         {QStringLiteral("zoom"), QStringLiteral("zoom.us")},
         {QStringLiteral(".zoom"), QStringLiteral(".cache/zoom"),
          QStringLiteral("Library/Application Support/zoom.us"),
          QStringLiteral("Library/Logs/zoom.us")}},
        {QStringLiteral("Microsoft Teams"),
         {QStringLiteral("teams"), QStringLiteral("Microsoft Teams")},
         {QStringLiteral("Library/Application Support/Microsoft/Teams")}},
        {QStringLiteral("LibreOffice"),
         {QStringLiteral("soffice.bin")},
         {QStringLiteral(".cache/libreoffice"), QStringLiteral(".config/libreoffice")}},
        {QStringLiteral("Docker Desktop"),
         {QStringLiteral("Docker Desktop")},
         {QStringLiteral("Library/Containers/com.docker.docker"),
          QStringLiteral("Library/Logs/Docker Desktop")}},
        {QStringLiteral("Dropbox"),
         {QStringLiteral("Dropbox"), QStringLiteral("dropbox")},
         {QStringLiteral("Dropbox/.dropbox.cache")}},
        {QStringLiteral("Xcode"),
         {QStringLiteral("Xcode")},
         {QStringLiteral("Library/Developer/Xcode")}},
        {QStringLiteral("Mail"),
         {QStringLiteral("Mail")},
         {QStringLiteral("Library/Containers/com.apple.mail")}},
    };
}

bool AppInUseGate::isProcessRunning(const QString &processName) const
{
    if (!CommandUtil::isExecutable(QStringLiteral("pgrep")))
        return false;
    return CommandUtil::execWithStatus(QStringLiteral("pgrep"),
                                       {QStringLiteral("-x"), processName}).ok();
}

bool AppInUseGate::isAnyRunning(const QStringList &processes)
{
    for (const QString &name : processes) {
        auto it = mRunningCache.find(name);
        if (it == mRunningCache.end())
            it = mRunningCache.insert(name, isProcessRunning(name));
        if (it.value())
            return true;
    }
    return false;
}

QString AppInUseGate::runningOwnerOf(const QString &path)
{
    // Judge by where the path really lives, so a symlink into an app's
    // profile cannot dodge the gate (and one pointing out of it is not
    // blamed on the app). A dangling leaf falls back to its parent's
    // canonical form, as the deny-list does.
    QFileInfo info(path);
    QString resolved = info.canonicalFilePath();
    if (resolved.isEmpty()) {
        const QString parent = QFileInfo(info.absolutePath()).canonicalFilePath();
        resolved = parent.isEmpty() ? QDir::cleanPath(info.absoluteFilePath())
                                    : QDir::cleanPath(parent + QLatin1Char('/') + info.fileName());
    }

    for (const Rule &rule : std::as_const(mRules)) {
        for (const QString &rel : rule.homeRelativeRoots) {
            const QString root = mHomeRoot + QLatin1Char('/') + rel;
            if (resolved == root || resolved.startsWith(root + QLatin1Char('/'))) {
                if (isAnyRunning(rule.processes))
                    return rule.app;
                break;
            }
        }
    }
    return QString();
}
