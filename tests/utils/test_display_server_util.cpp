#include <QTest>

#include "Utils/display_server_util.h"

// SSO-3729 / FW-02: pure mapping covered exhaustively so that callers — and
// any future XWayland-gated feature — can rely on a single source of truth
// for "what display server is this process actually using". The env-var path
// of detect()/isXWayland()/describeCurrent() is driven through qputenv/
// qunsetenv so the tests stay hermetic.

class TestDisplayServerUtil : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // ── classify (pure) ─────────────────────────────────────────────────────
    void classify_qtPlatformWayland_wins();
    void classify_qtPlatformWaylandEgl_wins();
    void classify_qtPlatformXcb_isX11();
    void classify_qtPlatformOffscreen_isOffscreen();
    void classify_qtPlatformCocoa_isOther();
    void classify_qtPlatformOverridesEnv();

    void classify_envWaylandDisplay_isWayland();
    void classify_envXdgWayland_isWayland();
    void classify_envXdgX11_isX11();
    void classify_envDisplayOnly_isX11();
    void classify_envXdgCaseInsensitive_isWayland();
    void classify_envWaylandWinsOverDisplay();
    void classify_emptyEverything_isOther();

    // ── name (pure mapping) ─────────────────────────────────────────────────
    void name_allKinds_haveLowercaseLabels();

    // ── detect (env-driven) ─────────────────────────────────────────────────
    void detect_noEnv_isOther();
    void detect_waylandDisplay_isWayland();
    void detect_displayOnly_isX11();
    void detect_bothSet_prefersWayland();

    // ── isXWayland ──────────────────────────────────────────────────────────
    void isXWayland_xcbOnX11Session_false();
    void isXWayland_xcbOnWaylandSession_true();
    void isXWayland_xcbWithWaylandSocket_true();
    void isXWayland_waylandPlatform_false();
    void isXWayland_emptyPlatformDisplayOnly_false();
    void isXWayland_emptyPlatformWithWaylandSocket_false();

    // ── describeCurrent (composed) ──────────────────────────────────────────
    void describeCurrent_xcbUnderWayland_isXwayland();
    void describeCurrent_xcbOnX11_isX11();
    void describeCurrent_nativeWayland_isWayland();
    void describeCurrent_emptyPlatformWaylandEnv_isWayland();
};

void TestDisplayServerUtil::init()
{
    // Each test starts from a known-empty env so qgetenv() returns isEmpty().
    qunsetenv("XDG_SESSION_TYPE");
    qunsetenv("WAYLAND_DISPLAY");
    qunsetenv("DISPLAY");
}

void TestDisplayServerUtil::cleanup()
{
    qunsetenv("XDG_SESSION_TYPE");
    qunsetenv("WAYLAND_DISPLAY");
    qunsetenv("DISPLAY");
}

// ── classify ────────────────────────────────────────────────────────────────

void TestDisplayServerUtil::classify_qtPlatformWayland_wins()
{
    QCOMPARE(DisplayServerUtil::classify("wayland", "", "", ""),
             DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::classify_qtPlatformWaylandEgl_wins()
{
    QCOMPARE(DisplayServerUtil::classify("wayland-egl", "", "", ""),
             DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::classify_qtPlatformXcb_isX11()
{
    QCOMPARE(DisplayServerUtil::classify("xcb", "", "", ""),
             DisplayServerUtil::Kind::X11);
}

void TestDisplayServerUtil::classify_qtPlatformOffscreen_isOffscreen()
{
    QCOMPARE(DisplayServerUtil::classify("offscreen", "", "", ""),
             DisplayServerUtil::Kind::Offscreen);
}

void TestDisplayServerUtil::classify_qtPlatformCocoa_isOther()
{
    QCOMPARE(DisplayServerUtil::classify("cocoa", "", "", ""),
             DisplayServerUtil::Kind::Other);
}

void TestDisplayServerUtil::classify_qtPlatformOverridesEnv()
{
    // The Qt plugin is the strongest signal: even if both DISPLAY and
    // WAYLAND_DISPLAY are set, Qt's choice is what the app is actually
    // talking to.
    QCOMPARE(DisplayServerUtil::classify("wayland", "x11",
                                          "wayland-0", ":0"),
             DisplayServerUtil::Kind::Wayland);
    QCOMPARE(DisplayServerUtil::classify("xcb", "wayland",
                                          "wayland-0", ":0"),
             DisplayServerUtil::Kind::X11);
}

void TestDisplayServerUtil::classify_envWaylandDisplay_isWayland()
{
    QCOMPARE(DisplayServerUtil::classify("", "", "wayland-0", ""),
             DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::classify_envXdgWayland_isWayland()
{
    QCOMPARE(DisplayServerUtil::classify("", "wayland", "", ""),
             DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::classify_envXdgX11_isX11()
{
    QCOMPARE(DisplayServerUtil::classify("", "x11", "", ""),
             DisplayServerUtil::Kind::X11);
}

void TestDisplayServerUtil::classify_envDisplayOnly_isX11()
{
    QCOMPARE(DisplayServerUtil::classify("", "", "", ":0"),
             DisplayServerUtil::Kind::X11);
}

void TestDisplayServerUtil::classify_envXdgCaseInsensitive_isWayland()
{
    QCOMPARE(DisplayServerUtil::classify("", "Wayland", "", ""),
             DisplayServerUtil::Kind::Wayland);
    QCOMPARE(DisplayServerUtil::classify("", "WAYLAND", "", ""),
             DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::classify_envWaylandWinsOverDisplay()
{
    // GNOME 50: both WAYLAND_DISPLAY and DISPLAY are set (DISPLAY points at
    // XWayland). Pre-Qt-app detection should still pick Wayland because the
    // wayland socket is the session's primary surface.
    QCOMPARE(DisplayServerUtil::classify("", "", "wayland-0", ":0"),
             DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::classify_emptyEverything_isOther()
{
    QCOMPARE(DisplayServerUtil::classify("", "", "", ""),
             DisplayServerUtil::Kind::Other);
}

// ── name ────────────────────────────────────────────────────────────────────

void TestDisplayServerUtil::name_allKinds_haveLowercaseLabels()
{
    QCOMPARE(DisplayServerUtil::name(DisplayServerUtil::Kind::Wayland),   QString("wayland"));
    QCOMPARE(DisplayServerUtil::name(DisplayServerUtil::Kind::X11),       QString("x11"));
    QCOMPARE(DisplayServerUtil::name(DisplayServerUtil::Kind::Offscreen), QString("offscreen"));
    QCOMPARE(DisplayServerUtil::name(DisplayServerUtil::Kind::Other),     QString("other"));
}

// ── detect ──────────────────────────────────────────────────────────────────

void TestDisplayServerUtil::detect_noEnv_isOther()
{
    QCOMPARE(DisplayServerUtil::detect(), DisplayServerUtil::Kind::Other);
}

void TestDisplayServerUtil::detect_waylandDisplay_isWayland()
{
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::detect(), DisplayServerUtil::Kind::Wayland);
}

void TestDisplayServerUtil::detect_displayOnly_isX11()
{
    qputenv("DISPLAY", ":0");
    QCOMPARE(DisplayServerUtil::detect(), DisplayServerUtil::Kind::X11);
}

void TestDisplayServerUtil::detect_bothSet_prefersWayland()
{
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    qputenv("DISPLAY", ":0");
    QCOMPARE(DisplayServerUtil::detect(), DisplayServerUtil::Kind::Wayland);
}

// ── isXWayland ──────────────────────────────────────────────────────────────

void TestDisplayServerUtil::isXWayland_xcbOnX11Session_false()
{
    qputenv("DISPLAY", ":0");
    qputenv("XDG_SESSION_TYPE", "x11");
    QCOMPARE(DisplayServerUtil::isXWayland("xcb"), false);
}

void TestDisplayServerUtil::isXWayland_xcbOnWaylandSession_true()
{
    // Qt was forced onto xcb (QT_QPA_PLATFORM=xcb) under a Wayland session.
    qputenv("DISPLAY", ":0");
    qputenv("XDG_SESSION_TYPE", "wayland");
    QCOMPARE(DisplayServerUtil::isXWayland("xcb"), true);
}

void TestDisplayServerUtil::isXWayland_xcbWithWaylandSocket_true()
{
    // GNOME 50 typical: WAYLAND_DISPLAY and DISPLAY both set; Qt picked xcb.
    qputenv("DISPLAY", ":0");
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::isXWayland("xcb"), true);
}

void TestDisplayServerUtil::isXWayland_waylandPlatform_false()
{
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::isXWayland("wayland"), false);
}

void TestDisplayServerUtil::isXWayland_emptyPlatformDisplayOnly_false()
{
    // No QGuiApplication yet, DISPLAY set, no Wayland signals → plain X11.
    qputenv("DISPLAY", ":0");
    QCOMPARE(DisplayServerUtil::isXWayland(""), false);
}

void TestDisplayServerUtil::isXWayland_emptyPlatformWithWaylandSocket_false()
{
    // With an empty platformName, the helper falls back to "DISPLAY set AND
    // no Wayland signals" for the isXClient half — which is false here
    // because WAYLAND_DISPLAY is set. So isXWayland("") returns false. The
    // pre-QApplication caller that knows it forced xcb should pass "xcb"
    // explicitly to reach the XWayland branch (covered by the xcb_* cases
    // above).
    qputenv("DISPLAY", ":0");
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::isXWayland(""), false);
}

// ── describeCurrent ─────────────────────────────────────────────────────────

void TestDisplayServerUtil::describeCurrent_xcbUnderWayland_isXwayland()
{
    qputenv("DISPLAY", ":0");
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::describeCurrent("xcb"), QString("xwayland"));
}

void TestDisplayServerUtil::describeCurrent_xcbOnX11_isX11()
{
    qputenv("DISPLAY", ":0");
    qputenv("XDG_SESSION_TYPE", "x11");
    QCOMPARE(DisplayServerUtil::describeCurrent("xcb"), QString("x11"));
}

void TestDisplayServerUtil::describeCurrent_nativeWayland_isWayland()
{
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::describeCurrent("wayland"), QString("wayland"));
}

void TestDisplayServerUtil::describeCurrent_emptyPlatformWaylandEnv_isWayland()
{
    // Pre-Qt callers: env says Wayland session.
    qputenv("WAYLAND_DISPLAY", "wayland-0");
    QCOMPARE(DisplayServerUtil::describeCurrent(""), QString("wayland"));
}

QTEST_APPLESS_MAIN(TestDisplayServerUtil)
#include "test_display_server_util.moc"
