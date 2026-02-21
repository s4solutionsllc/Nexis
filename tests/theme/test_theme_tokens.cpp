#include <QTest>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>

class TestThemeTokens : public QObject
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

    QSet<QString> extractQssTokens() const
    {
        QFile f(qssPath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        QString qss = QTextStream(&f).readAll();
        QSet<QString> tokens;

        // Match @tokenName but exclude @dp\d+ patterns (size tokens)
        QRegularExpression re("@([a-zA-Z][a-zA-Z0-9_]*)");
        auto it = re.globalMatch(qss);
        while (it.hasNext()) {
            auto match = it.next();
            QString token = match.captured(1);
            // Exclude @dp patterns (e.g., dp8, dp4, dp30px) and
            // user-configurable tokens not stored in values.ini
            if (QRegularExpression("^dp\\d+").match(token).hasMatch())
                continue;
            if (token == "fontFamily")
                continue;
            tokens.insert("@" + token);
        }
        return tokens;
    }

    QMap<QString, QString> loadValuesIni(const QString &path) const
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        QMap<QString, QString> values;
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
                continue;
            int eq = line.indexOf('=');
            if (eq > 0) {
                QString key = line.left(eq).trimmed();
                QString val = line.mid(eq + 1).trimmed();
                values[key] = val;
            }
        }
        return values;
    }

    QString replaceTokens(const QString &qss, const QMap<QString, QString> &values) const
    {
        QString result = qss;
        for (auto it = values.begin(); it != values.end(); ++it) {
            result.replace(it.key(), it.value());
        }
        return result;
    }

private slots:
    void darkTheme_allTokensResolved();
    void lightTheme_allTokensResolved();
    void darkTheme_colorsValid();
    void lightTheme_colorsValid();
    void themes_sameTokenSets();
    void noUnresolvedTokens_dark();
    void noUnresolvedTokens_light();
};

void TestThemeTokens::darkTheme_allTokensResolved()
{
    QSet<QString> qssTokens = extractQssTokens();
    QVERIFY2(!qssTokens.isEmpty(), "Failed to extract tokens from style.qss");

    QMap<QString, QString> values = loadValuesIni(darkValuesPath());
    QVERIFY2(!values.isEmpty(), "Failed to load dark theme values.ini");

    for (const QString &token : qssTokens) {
        QVERIFY2(values.contains(token),
                 qPrintable(QString("Dark theme missing token: %1").arg(token)));
    }
}

void TestThemeTokens::lightTheme_allTokensResolved()
{
    QSet<QString> qssTokens = extractQssTokens();
    QVERIFY2(!qssTokens.isEmpty(), "Failed to extract tokens from style.qss");

    QMap<QString, QString> values = loadValuesIni(lightValuesPath());
    QVERIFY2(!values.isEmpty(), "Failed to load light theme values.ini");

    for (const QString &token : qssTokens) {
        QVERIFY2(values.contains(token),
                 qPrintable(QString("Light theme missing token: %1").arg(token)));
    }
}

void TestThemeTokens::darkTheme_colorsValid()
{
    QMap<QString, QString> values = loadValuesIni(darkValuesPath());
    QVERIFY(!values.isEmpty());

    QRegularExpression hexColor("^#([0-9a-fA-F]{3}|[0-9a-fA-F]{4}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$");

    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it.key() == "@themeName")
            continue;
        QVERIFY2(hexColor.match(it.value()).hasMatch(),
                 qPrintable(QString("Dark theme invalid color: %1=%2").arg(it.key(), it.value())));
    }
}

void TestThemeTokens::lightTheme_colorsValid()
{
    QMap<QString, QString> values = loadValuesIni(lightValuesPath());
    QVERIFY(!values.isEmpty());

    QRegularExpression hexColor("^#([0-9a-fA-F]{3}|[0-9a-fA-F]{4}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$");

    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it.key() == "@themeName")
            continue;
        QVERIFY2(hexColor.match(it.value()).hasMatch(),
                 qPrintable(QString("Light theme invalid color: %1=%2").arg(it.key(), it.value())));
    }
}

void TestThemeTokens::themes_sameTokenSets()
{
    QMap<QString, QString> dark = loadValuesIni(darkValuesPath());
    QMap<QString, QString> light = loadValuesIni(lightValuesPath());

    QSet<QString> darkKeys(dark.keyBegin(), dark.keyEnd());
    QSet<QString> lightKeys(light.keyBegin(), light.keyEnd());

    QSet<QString> onlyInDark = darkKeys - lightKeys;
    QSet<QString> onlyInLight = lightKeys - darkKeys;

    QVERIFY2(onlyInDark.isEmpty(),
             qPrintable(QString("Tokens only in dark: %1").arg(QStringList(onlyInDark.begin(), onlyInDark.end()).join(", "))));
    QVERIFY2(onlyInLight.isEmpty(),
             qPrintable(QString("Tokens only in light: %1").arg(QStringList(onlyInLight.begin(), onlyInLight.end()).join(", "))));
}

void TestThemeTokens::noUnresolvedTokens_dark()
{
    QFile f(qssPath());
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString qss = QTextStream(&f).readAll();

    QMap<QString, QString> values = loadValuesIni(darkValuesPath());
    QString resolved = replaceTokens(qss, values);

    QRegularExpression unresolvedToken("@(color\\d+|accent[A-Z]\\w*|card[A-Z]\\w*|border[A-Z]\\w*|success[A-Z]\\w*|warning[A-Z]\\w*|destructive[A-Z]\\w*|pageContent|sidebar|themeName)");
    QRegularExpressionMatch match = unresolvedToken.match(resolved);
    QVERIFY2(!match.hasMatch(),
             qPrintable(QString("Dark theme has unresolved token: @%1").arg(match.captured(1))));
}

void TestThemeTokens::noUnresolvedTokens_light()
{
    QFile f(qssPath());
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString qss = QTextStream(&f).readAll();

    QMap<QString, QString> values = loadValuesIni(lightValuesPath());
    QString resolved = replaceTokens(qss, values);

    QRegularExpression unresolvedToken("@(color\\d+|accent[A-Z]\\w*|card[A-Z]\\w*|border[A-Z]\\w*|success[A-Z]\\w*|warning[A-Z]\\w*|destructive[A-Z]\\w*|pageContent|sidebar|themeName)");
    QRegularExpressionMatch match = unresolvedToken.match(resolved);
    QVERIFY2(!match.hasMatch(),
             qPrintable(QString("Light theme has unresolved token: @%1").arg(match.captured(1))));
}

QTEST_MAIN(TestThemeTokens)
#include "test_theme_tokens.moc"
