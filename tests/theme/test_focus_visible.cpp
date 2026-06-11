#include <QTest>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

// SSO-3502: assert the focus-ring QSS contract is present in general.qss.
// The actual focus visualization is a Qt runtime concern; this test pins the
// stylesheet selectors and the @focusRingColor token plumbing so a future
// edit that strips them out fails CI loudly.
class TestFocusVisible : public QObject
{
    Q_OBJECT

private:
    QString projectDir() const
    {
        return QStringLiteral(PROJECT_SOURCE_DIR);
    }

    QString qssPath() const
    {
        return projectDir() + "/shared/nexis/static/themes/default/style/style.qss";
    }

    QString darkValuesPath() const
    {
        return projectDir() + "/shared/nexis/static/themes/default/style/values.ini";
    }

    QString lightValuesPath() const
    {
        return projectDir() + "/shared/nexis/static/themes/light/style/values.ini";
    }

    QString readFile(const QString &path) const
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&f).readAll();
    }

    bool valuesContainKey(const QString &path, const QString &key) const
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(key + QStringLiteral("=")))
                return true;
        }
        return false;
    }

    bool qssHasFocusSelector(const QString &qss, const QString &widget) const
    {
        QRegularExpression rx(QStringLiteral("(^|[\\s,])") + QRegularExpression::escape(widget)
                              + QStringLiteral(":focus[\\s,{]"));
        return rx.match(qss).hasMatch();
    }

private slots:
    void darkTheme_definesFocusRingToken();
    void lightTheme_definesFocusRingToken();
    void qss_referencesFocusRingToken();
    void qss_focusSelectors_buttons();
    void qss_focusSelectors_lineEdits();
    void qss_focusSelectors_treeAndTable();
    void qss_dropsBlanketOutlineNone();
};

void TestFocusVisible::darkTheme_definesFocusRingToken()
{
    QVERIFY2(valuesContainKey(darkValuesPath(), "@focusRingColor"),
             "Dark theme values.ini missing @focusRingColor token");
}

void TestFocusVisible::lightTheme_definesFocusRingToken()
{
    QVERIFY2(valuesContainKey(lightValuesPath(), "@focusRingColor"),
             "Light theme values.ini missing @focusRingColor token");
}

void TestFocusVisible::qss_referencesFocusRingToken()
{
    QString qss = readFile(qssPath());
    QVERIFY2(!qss.isEmpty(), "Failed to read style.qss");
    QVERIFY2(qss.contains(QStringLiteral("@focusRingColor")),
             "style.qss does not reference @focusRingColor — focus ring will not render");
}

void TestFocusVisible::qss_focusSelectors_buttons()
{
    QString qss = readFile(qssPath());
    QVERIFY(!qss.isEmpty());

    QVERIFY2(qssHasFocusSelector(qss, "QPushButton"),
             "style.qss missing QPushButton:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QToolButton"),
             "style.qss missing QToolButton:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QCheckBox"),
             "style.qss missing QCheckBox:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QRadioButton"),
             "style.qss missing QRadioButton:focus selector");
}

void TestFocusVisible::qss_focusSelectors_lineEdits()
{
    QString qss = readFile(qssPath());
    QVERIFY(!qss.isEmpty());

    QVERIFY2(qssHasFocusSelector(qss, "QLineEdit"),
             "style.qss missing QLineEdit:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QPlainTextEdit"),
             "style.qss missing QPlainTextEdit:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QComboBox"),
             "style.qss missing QComboBox:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QSpinBox"),
             "style.qss missing QSpinBox:focus selector");
}

void TestFocusVisible::qss_focusSelectors_treeAndTable()
{
    QString qss = readFile(qssPath());
    QVERIFY(!qss.isEmpty());

    QVERIFY2(qssHasFocusSelector(qss, "QTreeView"),
             "style.qss missing QTreeView:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QTreeWidget"),
             "style.qss missing QTreeWidget:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QTableView"),
             "style.qss missing QTableView:focus selector");
    QVERIFY2(qssHasFocusSelector(qss, "QListWidget"),
             "style.qss missing QListWidget:focus selector");
}

void TestFocusVisible::qss_dropsBlanketOutlineNone()
{
    // The pre-SSO-3502 stylesheet had `QCheckBox:focus { outline: none; border: none; }`
    // which actively suppressed the focus ring. Guard against it coming back.
    QString qss = readFile(qssPath());
    QVERIFY(!qss.isEmpty());

    QRegularExpression rx(QStringLiteral(
        "QCheckBox:focus\\s*\\{[^}]*outline:\\s*none[^}]*\\}"));
    QVERIFY2(!rx.match(qss).hasMatch(),
             "style.qss contains `QCheckBox:focus { outline: none }` — focus ring will be invisible");
}

QTEST_MAIN(TestFocusVisible)
#include "test_focus_visible.moc"
