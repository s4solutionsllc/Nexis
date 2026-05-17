// SSO-381: verify the PNG fallback substitution used when the Qt SVG
// image plugin is missing (Linux Mint 22 "Zena" and other minimal
// Ubuntu-24.04 derivatives). The transform itself is a pure string
// operation, so it can be exercised without a QApplication.

#include <QtTest/QtTest>

#include "app_manager.h"

class TestIndicatorFallback : public QObject
{
    Q_OBJECT

private slots:
    void rewritesToggleIndicators()
    {
        const QString qss = QStringLiteral(
            "QCheckBox::indicator:checked   { image: url(:/static/themes/common/img/checkbox.svg); }\n"
            "QCheckBox::indicator:unchecked { image: url(:/static/themes/common/img/un-checkbox.svg); }\n");

        const QString out = AppManager::applyIndicatorPngFallback(qss);

        QVERIFY2(!out.contains(QStringLiteral("checkbox.svg")),
                 "checkbox.svg references must be rewritten");
        QVERIFY2(out.contains(QStringLiteral("checkbox.png")),
                 "checkbox.png reference is expected");
        QVERIFY2(out.contains(QStringLiteral("un-checkbox.png")),
                 "un-checkbox.png reference is expected");
    }

    void rewritesCircleIndicators()
    {
        const QString qss = QStringLiteral(
            "QCheckBox[accessibleName=\"circle\"]::indicator:unchecked { image: url(:/static/themes/common/img/circle-unchecked.svg); }\n"
            "QCheckBox[accessibleName=\"circle\"]::indicator:checked   { image: url(:/static/themes/common/img/circle-checked.svg); }\n");

        const QString out = AppManager::applyIndicatorPngFallback(qss);

        QVERIFY(!out.contains(QStringLiteral("circle-checked.svg")));
        QVERIFY(!out.contains(QStringLiteral("circle-unchecked.svg")));
        QVERIFY(out.contains(QStringLiteral("circle-checked.png")));
        QVERIFY(out.contains(QStringLiteral("circle-unchecked.png")));
    }

    // Regression guard: the substitution previously could clobber
    // `un-checkbox.svg` if the shorter `checkbox.svg` swap ran first
    // (substring overlap on the suffix). Verify both end up as their
    // distinct PNG siblings even when both appear in the same string.
    void respectsLongestKeyFirst()
    {
        const QString qss = QStringLiteral(
            "a { image: url(:/static/themes/common/img/un-checkbox.svg); }\n"
            "b { image: url(:/static/themes/common/img/checkbox.svg); }\n");

        const QString out = AppManager::applyIndicatorPngFallback(qss);

        QVERIFY2(out.contains(QStringLiteral("un-checkbox.png")),
                 "un-checkbox.svg must rewrite to un-checkbox.png");
        QVERIFY2(out.contains(QStringLiteral("/checkbox.png")),
                 "checkbox.svg must rewrite to checkbox.png (and stay distinct)");
        QVERIFY(!out.contains(QStringLiteral(".svg")));
    }

    // Healthy installs (SVG plugin present) should never see this code path,
    // but verify the transform is a no-op on QSS that has no indicator URLs.
    void leavesUnrelatedContentUntouched()
    {
        const QString qss = QStringLiteral(
            "QWidget { background: #112233; }\n"
            "QPushButton { image: url(:/static/themes/common/img/chevron-down.svg); }\n");

        const QString out = AppManager::applyIndicatorPngFallback(qss);

        QCOMPARE(out, qss);
    }

    // The renamed PNG resource paths in the QSS must actually exist as
    // compiled-in Qt resources. This protects against drift where the QSS
    // is updated but the .qrc isn't (or vice-versa).
    void resourcesArePresent()
    {
        for (const auto *path : {
                 ":/static/themes/common/img/checkbox.png",
                 ":/static/themes/common/img/un-checkbox.png",
                 ":/static/themes/common/img/circle-checked.png",
                 ":/static/themes/common/img/circle-unchecked.png",
             }) {
            QFile f(QString::fromLatin1(path));
            QVERIFY2(f.exists(),
                     qPrintable(QStringLiteral("missing PNG resource: %1").arg(QString::fromLatin1(path))));
        }
    }
};

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(static);
    QCoreApplication app(argc, argv);
    TestIndicatorFallback tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_indicator_fallback.moc"
