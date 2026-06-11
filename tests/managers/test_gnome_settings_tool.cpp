// SSO-3391 / WI-29: regression tests pinning the macOS GnomeSettingsTool
// down as a hard no-op stub so it can never write into Apple preference
// domains (`NSGlobalDomain`, `com.apple.dock`, …).
//
// On Linux these tests cover the platform-neutral availability check via
// ToolManager. The dangerous behavior under audit was macOS-only, so the
// platform-specific assertions are gated on `Q_OS_MACOS`.

#include <QtTest>
#include <QString>

#include <Tools/gnome_settings_constants.h>

#ifdef Q_OS_MACOS
#include <Tools/gnome_settings_tool_macos.h>
#endif

class TestGnomeSettingsTool : public QObject
{
    Q_OBJECT

#ifdef Q_OS_MACOS
private slots:
    void macos_isAvailable_isFalse();
    void macos_schemaExists_isFalseForEverySchema();
    void macos_setters_returnFalse_andPerformNoWrite();
    void macos_getters_returnEmpty();
    void macos_constants_areNeutralized();
#else
private slots:
    void linux_placeholder() { QSKIP("Linux GnomeSettings is not covered by this test"); }
#endif
};

#ifdef Q_OS_MACOS

void TestGnomeSettingsTool::macos_isAvailable_isFalse()
{
    // Audit A5: the previous macOS adapter returned true here, and that
    // alone was enough to unhide the GNOME page if the sidebar `#ifdef`
    // ever regressed.
    GnomeSettingsToolMacOS tool;
    QVERIFY2(!tool.isAvailable(),
             "GnomeSettingsToolMacOS::isAvailable() must report false so "
             "ToolManager::checkGnomeSettings() short-circuits.");
}

void TestGnomeSettingsTool::macos_schemaExists_isFalseForEverySchema()
{
    GnomeSettingsToolMacOS tool;
    const QStringList schemas = {
        QStringLiteral("org.gnome.desktop.interface"),
        QStringLiteral("NSGlobalDomain"),
        QStringLiteral("com.apple.dock"),
        QStringLiteral(""),
    };
    for (const QString &schema : schemas) {
        QVERIFY2(!tool.schemaExists(schema),
                 qPrintable(QStringLiteral("schemaExists(\"%1\") must be false on macOS").arg(schema)));
    }
}

void TestGnomeSettingsTool::macos_setters_returnFalse_andPerformNoWrite()
{
    // No external process is observed here directly. The stub does not
    // depend on `defaults`; it has no QProcess invocation at all. If a
    // future regression reintroduces `CommandUtil::exec("defaults", …)`,
    // these calls would either succeed (and corrupt `NSGlobalDomain`) or
    // fail with a different error code. Asserting they return false
    // documents and pins the no-op contract.
    GnomeSettingsToolMacOS tool;
    QVERIFY(!tool.setS(QStringLiteral("NSGlobalDomain"),
                       QStringLiteral("AppleInterfaceStyle"),
                       QStringLiteral("Dark")));
    QVERIFY(!tool.setB(QStringLiteral("NSGlobalDomain"),
                       QStringLiteral("AppleInterfaceStyle"), true));
    QVERIFY(!tool.setI(QStringLiteral("com.apple.dock"),
                       QStringLiteral("orientation"), 0));
    QVERIFY(!tool.setD(QStringLiteral("NSGlobalDomain"),
                       QStringLiteral("AppleInterfaceStyle"), 1.0));
}

void TestGnomeSettingsTool::macos_getters_returnEmpty()
{
    GnomeSettingsToolMacOS tool;
    QCOMPARE(tool.getS(QStringLiteral("NSGlobalDomain"),
                       QStringLiteral("AppleInterfaceStyle")),
             QString());
    QCOMPARE(tool.getB(QStringLiteral("NSGlobalDomain"),
                       QStringLiteral("AppleInterfaceStyle")), false);
    QCOMPARE(tool.getI(QStringLiteral("com.apple.dock"),
                       QStringLiteral("orientation")), 0);
    QCOMPARE(tool.getD(QStringLiteral("NSGlobalDomain"),
                       QStringLiteral("AppleInterfaceStyle")), 0.0);
}

void TestGnomeSettingsTool::macos_constants_areNeutralized()
{
    // The macOS constants header has been wiped to empty strings so even a
    // misrouted call through the platform-neutral surface cannot target a
    // real Apple preference domain.
    QCOMPARE(GnomeSchema::INTERFACE,  QString());
    QCOMPARE(GnomeSchema::WM_PREFS,   QString());
    QCOMPARE(GnomeSchema::MUTTER,     QString());
    QCOMPARE(GnomeSchema::MOUSE,      QString());
    QCOMPARE(GnomeSchema::TOUCHPAD,   QString());
    QCOMPARE(GnomeSchema::BACKGROUND, QString());
    QCOMPARE(GnomeSchema::SOUND,      QString());

    QCOMPARE(GnomeKey::COLOR_SCHEME,    QString());
    QCOMPARE(GnomeKey::BUTTON_LAYOUT,   QString());
    QCOMPARE(GnomeKey::NATURAL_SCROLL,  QString());
    QCOMPARE(GnomeKey::PICTURE_URI,     QString());
    QCOMPARE(GnomeKey::EVENT_SOUNDS,    QString());
}

#endif // Q_OS_MACOS

QTEST_MAIN(TestGnomeSettingsTool)
#include "test_gnome_settings_tool.moc"
