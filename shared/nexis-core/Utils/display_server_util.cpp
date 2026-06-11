#include "display_server_util.h"

#include <QByteArray>
#include <QString>

DisplayServerUtil::Kind DisplayServerUtil::classify(const QString &platformName,
                                                    const QString &xdgSessionType,
                                                    const QString &waylandDisplay,
                                                    const QString &display)
{
    // Qt's platform plugin is the strongest signal: it's what the app is
    // *actually* talking to right now, regardless of what the user's session
    // type says.
    if (!platformName.isEmpty()) {
        const QString p = platformName.toLower();
        if (p.startsWith(QStringLiteral("wayland"))) {
            return Kind::Wayland;
        }
        if (p == QStringLiteral("xcb")) {
            return Kind::X11;
        }
        if (p == QStringLiteral("offscreen")) {
            return Kind::Offscreen;
        }
        return Kind::Other;
    }

    // No QGuiApplication yet (pre-construction CLI checks). Fall back to env.
    if (!waylandDisplay.isEmpty()) {
        return Kind::Wayland;
    }
    if (xdgSessionType.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0) {
        return Kind::Wayland;
    }
    if (xdgSessionType.compare(QStringLiteral("x11"), Qt::CaseInsensitive) == 0) {
        return Kind::X11;
    }
    if (!display.isEmpty()) {
        return Kind::X11;
    }
    return Kind::Other;
}

DisplayServerUtil::Kind DisplayServerUtil::detect()
{
    const QString xdg = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
    const QString wl  = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    const QString dp  = QString::fromLocal8Bit(qgetenv("DISPLAY"));
    return classify(QString(), xdg, wl, dp);
}

bool DisplayServerUtil::isXWayland(const QString &platformName)
{
    const QString xdg = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
    const QString wl  = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    const QString dp  = QString::fromLocal8Bit(qgetenv("DISPLAY"));

    // The "we're an X client" half: prefer Qt's platform name when the
    // caller has it (authoritative — `xcb` ⇒ X11 surface). With an empty
    // platformName, fall back to "DISPLAY set, no Wayland signals" which is
    // a best-effort guess for pre-QApplication callers.
    const bool isXClient = !platformName.isEmpty()
        ? (platformName.compare(QStringLiteral("xcb"), Qt::CaseInsensitive) == 0)
        : (!dp.isEmpty()
            && wl.isEmpty()
            && xdg.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) != 0);

    if (!isXClient) {
        return false;
    }

    // The "session is Wayland" half: XDG_SESSION_TYPE=wayland or a live
    // WAYLAND_DISPLAY socket means the X server we're talking to is
    // XWayland, not a real X.Org. (When the caller passed platformName=xcb
    // explicitly, WAYLAND_DISPLAY alongside is the canonical signal — that's
    // exactly the GNOME 50 case.)
    if (xdg.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!wl.isEmpty()) {
        return true;
    }
    return false;
}

QString DisplayServerUtil::name(Kind kind)
{
    switch (kind) {
    case Kind::Wayland:   return QStringLiteral("wayland");
    case Kind::X11:       return QStringLiteral("x11");
    case Kind::Offscreen: return QStringLiteral("offscreen");
    case Kind::Other:     return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

QString DisplayServerUtil::describeCurrent(const QString &platformName)
{
    const QString xdg = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
    const QString wl  = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    const QString dp  = QString::fromLocal8Bit(qgetenv("DISPLAY"));
    const Kind k = classify(platformName, xdg, wl, dp);
    if (k == Kind::X11 && isXWayland(platformName)) {
        return QStringLiteral("xwayland");
    }
    return name(k);
}
