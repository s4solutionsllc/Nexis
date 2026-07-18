#include <QTest>
#include "menu_bar_format_util.h"

class TestMenuBarFormatUtil : public QObject
{
    Q_OBJECT

private slots:
    void formatTitle_zero();
    void formatTitle_typical();
    void formatTitle_fullUsage();
    void formatTitle_clampsNegative();
    void formatTitle_clampsOver100();
};

void TestMenuBarFormatUtil::formatTitle_zero()
{
    QCOMPARE(MenuBarFormatUtil::formatTitle(0, 0), QString("C 0%  M 0%"));
}

void TestMenuBarFormatUtil::formatTitle_typical()
{
    QCOMPARE(MenuBarFormatUtil::formatTitle(12, 64), QString("C 12%  M 64%"));
}

void TestMenuBarFormatUtil::formatTitle_fullUsage()
{
    QCOMPARE(MenuBarFormatUtil::formatTitle(100, 100), QString("C 100%  M 100%"));
}

void TestMenuBarFormatUtil::formatTitle_clampsNegative()
{
    QCOMPARE(MenuBarFormatUtil::formatTitle(-5, 50), QString("C 0%  M 50%"));
}

void TestMenuBarFormatUtil::formatTitle_clampsOver100()
{
    QCOMPARE(MenuBarFormatUtil::formatTitle(150, 50), QString("C 100%  M 50%"));
}

QTEST_MAIN(TestMenuBarFormatUtil)
#include "test_menu_bar_format_util.moc"
