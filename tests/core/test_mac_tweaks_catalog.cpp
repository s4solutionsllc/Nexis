// SSO-23857: unit tests for the pure parts of MacTweaksCatalog — data
// integrity, category coverage, and per-OS-version gating. Deliberately does
// NOT call readCurrent()/toggleBoolTweak()/resetToDefault(): those shell out
// to the real `defaults`/`killall` binaries via MacDefaultsTool, and this
// test target is not platform-gated, so exercising them here would mutate
// real system preference domains on a macOS test runner.

#include <QTest>
#include <QSet>

#include "Tools/mac_tweaks_catalog.h"

class TestMacTweaksCatalog : public QObject
{
    Q_OBJECT

private slots:
    void all_isNonEmpty();
    void all_idsAreUnique();
    void all_everyTweakHasRequiredFields();
    void categories_matchesAcceptanceCriteria();
    void findById_knownId_returnsMatch();
    void findById_unknownId_returnsNull();

    void isSupported_noGate_alwaysTrue();
    void isSupported_belowMinVersion_false();
    void isSupported_atOrAboveMinVersion_true();
    void isSupported_unknownOsVersion_failsOpen();
    void supportedFor_excludesGatedTweaksBelowThreshold();

    void effectiveValue_usesReadWhenFound();
    void effectiveValue_fallsBackToDefaultWhenUnset();
};

void TestMacTweaksCatalog::all_isNonEmpty()
{
    QVERIFY(!MacTweaksCatalog::all().isEmpty());
}

void TestMacTweaksCatalog::all_idsAreUnique()
{
    QSet<QString> seen;
    for (const MacTweakDef &t : MacTweaksCatalog::all()) {
        QVERIFY2(!seen.contains(t.id), qPrintable(t.id));
        seen.insert(t.id);
    }
}

void TestMacTweaksCatalog::all_everyTweakHasRequiredFields()
{
    for (const MacTweakDef &t : MacTweaksCatalog::all()) {
        QVERIFY2(!t.id.isEmpty(), "id");
        QVERIFY2(!t.category.isEmpty(), qPrintable(t.id));
        QVERIFY2(!t.name.isEmpty(), qPrintable(t.id));
        QVERIFY2(!t.description.isEmpty(), qPrintable(t.id));
        QVERIFY2(!t.domain.isEmpty(), qPrintable(t.id));
        QVERIFY2(!t.key.isEmpty(), qPrintable(t.id));
        if (t.type == MacDefaultsValueType::Bool) {
            QVERIFY2(t.enabledValue != t.disabledValue, qPrintable(t.id));
        }
    }
}

void TestMacTweaksCatalog::categories_matchesAcceptanceCriteria()
{
    // SSO-23857 scope: "Finder, Dock, screenshots, animations, login window".
    const QStringList cats = MacTweaksCatalog::categories();
    QCOMPARE(cats.size(), 5);
    QVERIFY(cats.contains(QStringLiteral("Finder")));
    QVERIFY(cats.contains(QStringLiteral("Dock")));
    QVERIFY(cats.contains(QStringLiteral("Screenshots")));
    QVERIFY(cats.contains(QStringLiteral("Animations")));
    QVERIFY(cats.contains(QStringLiteral("Login Window")));
}

void TestMacTweaksCatalog::findById_knownId_returnsMatch()
{
    const MacTweakDef *t = MacTweaksCatalog::findById(QStringLiteral("finder.show_hidden_files"));
    QVERIFY(t != nullptr);
    QCOMPARE(t->domain, QStringLiteral("com.apple.finder"));
    QCOMPARE(t->key, QStringLiteral("AppleShowAllFiles"));
}

void TestMacTweaksCatalog::findById_unknownId_returnsNull()
{
    QVERIFY(MacTweaksCatalog::findById(QStringLiteral("nonexistent.id")) == nullptr);
}

void TestMacTweaksCatalog::isSupported_noGate_alwaysTrue()
{
    MacTweakDef t;
    t.minOsVersion = QVersionNumber();
    QVERIFY(!t.hasVersionGate());
    QVERIFY(MacTweaksCatalog::isSupported(t, QVersionNumber(10, 0)));
    QVERIFY(MacTweaksCatalog::isSupported(t, QVersionNumber(99, 0)));
}

void TestMacTweaksCatalog::isSupported_belowMinVersion_false()
{
    MacTweakDef t;
    t.minOsVersion = QVersionNumber(13, 0);
    QVERIFY(t.hasVersionGate());
    QVERIFY(!MacTweaksCatalog::isSupported(t, QVersionNumber(12, 6)));
}

void TestMacTweaksCatalog::isSupported_atOrAboveMinVersion_true()
{
    MacTweakDef t;
    t.minOsVersion = QVersionNumber(13, 0);
    QVERIFY(MacTweaksCatalog::isSupported(t, QVersionNumber(13, 0)));
    QVERIFY(MacTweaksCatalog::isSupported(t, QVersionNumber(14, 2)));
}

void TestMacTweaksCatalog::isSupported_unknownOsVersion_failsOpen()
{
    MacTweakDef t;
    t.minOsVersion = QVersionNumber(13, 0);
    QVERIFY(MacTweaksCatalog::isSupported(t, QVersionNumber()));
}

void TestMacTweaksCatalog::supportedFor_excludesGatedTweaksBelowThreshold()
{
    const QList<MacTweakDef> supportedOld = MacTweaksCatalog::supportedFor(QVersionNumber(10, 9));
    const QList<MacTweakDef> supportedNew = MacTweaksCatalog::supportedFor(QVersionNumber(99, 0));

    QVERIFY(supportedOld.size() < MacTweaksCatalog::all().size());
    QCOMPARE(supportedNew.size(), MacTweaksCatalog::all().size());

    for (const MacTweakDef &t : supportedOld)
        QVERIFY(MacTweaksCatalog::isSupported(t, QVersionNumber(10, 9)));
}

void TestMacTweaksCatalog::effectiveValue_usesReadWhenFound()
{
    MacTweakDef t;
    t.defaultValue = false;
    MacDefaultsReadResult read;
    read.found = true;
    read.value = true;
    QCOMPARE(MacTweaksCatalog::effectiveValue(t, read).toBool(), true);
}

void TestMacTweaksCatalog::effectiveValue_fallsBackToDefaultWhenUnset()
{
    MacTweakDef t;
    t.defaultValue = QStringLiteral("png");
    MacDefaultsReadResult read;
    read.found = false;
    QCOMPARE(MacTweaksCatalog::effectiveValue(t, read).toString(), QStringLiteral("png"));
}

QTEST_MAIN(TestMacTweaksCatalog)
#include "test_mac_tweaks_catalog.moc"
