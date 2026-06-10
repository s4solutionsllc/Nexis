#include <QTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

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
            if (token == "fontFamily" || token == "monoFontFamily")
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

    // WI-25 (Q2): scan shared/ C++ for per-widget setStyleSheet(...) calls and
    // return any that still contain a raw `@tokenName` literal. Per-widget
    // stylesheets are NOT run through AppManager::updateStylesheet token
    // substitution (only the global qApp->setStyleSheet() is), so any `@token`
    // left inside the argument is dropped by Qt as an invalid declaration.
    // Returns a list of `path:lineno  excerpt` strings, one per offending call.
    QStringList findRawTokensInSetStyleSheet() const
    {
        QStringList offenders;
        // Token names registered in values.ini look like @camelCase or @colorNN;
        // exclude @dpN (DPI size tokens, handled by app_manager regex) and
        // @fontFamily / @monoFontFamily (handled by literal replace).
        QRegularExpression tokenRx("@([a-zA-Z][a-zA-Z0-9_]*)");
        const QString sharedDir = projectDir() + "/shared";

        QDirIterator it(sharedDir,
                        {"*.cpp", "*.h", "*.mm"},
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString src = QTextStream(&f).readAll();

            int searchFrom = 0;
            while (true) {
                const int call = src.indexOf("setStyleSheet(", searchFrom);
                if (call < 0)
                    break;

                // Walk forward paren-balanced to find the end of the call.
                int depth = 0;
                int end = -1;
                for (int i = call + int(qstrlen("setStyleSheet(")) - 1;
                     i < src.size(); ++i) {
                    const QChar c = src.at(i);
                    if (c == '(') {
                        ++depth;
                    } else if (c == ')') {
                        if (--depth == 0) { end = i; break; }
                    }
                }
                if (end < 0)
                    break;

                const QString arg = src.mid(call, end - call + 1);
                auto m = tokenRx.globalMatch(arg);
                while (m.hasNext()) {
                    auto match = m.next();
                    const QString name = match.captured(1);
                    // Skip DPI size tokens and font-family tokens — those have
                    // their own substitution path or are not stylesheet tokens.
                    if (QRegularExpression("^dp\\d+").match(name).hasMatch())
                        continue;
                    if (name == "fontFamily" || name == "monoFontFamily")
                        continue;
                    // A `value("@foo"` lookup inside the arg (e.g. an inline
                    // sv->value(...) read used to build the string) is not a
                    // raw token in the resulting stylesheet — it's a key.
                    const int matchStart = match.capturedStart(0);
                    const int valuePrefixStart = matchStart - int(qstrlen("value(\""));
                    if (valuePrefixStart >= 0
                        && arg.mid(valuePrefixStart, qstrlen("value(\""))
                               == QStringLiteral("value(\""))
                        continue;

                    // Locate line number of the match in the original file.
                    const int absPos = call + matchStart;
                    const int lineNo = src.left(absPos).count('\n') + 1;
                    offenders << QString("%1:%2  @%3")
                                     .arg(path)
                                     .arg(lineNo)
                                     .arg(name);
                }

                searchFrom = end + 1;
            }
        }
        return offenders;
    }

    // WI-24 (Q1): collect every @token literal passed to QSettings::value() inside
    // shared/ C++ so getStyleValues() reads are guarded the same way QSS tokens are.
    // Matches `value("@foo"` and `sv->value("@foo"` plus the default-arg form
    // `value("@foo", "#deadbeef"`.
    QSet<QString> extractCppTokens() const
    {
        QSet<QString> tokens;
        QRegularExpression re("value\\(\"(@[a-zA-Z][a-zA-Z0-9_]*)\"");
        const QString sharedDir = projectDir() + "/shared";

        QDirIterator it(sharedDir,
                        {"*.cpp", "*.h", "*.mm"},
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QString src = QTextStream(&f).readAll();
            auto m = re.globalMatch(src);
            while (m.hasNext())
                tokens.insert(m.next().captured(1));
        }
        return tokens;
    }

private slots:
    void darkTheme_allTokensResolved();
    void lightTheme_allTokensResolved();
    void darkTheme_colorsValid();
    void lightTheme_colorsValid();
    void themes_sameTokenSets();
    void noUnresolvedTokens_dark();
    void noUnresolvedTokens_light();
    void noRawTokensInPerWidgetStyleSheet();
    void darkTheme_allCppTokensResolved();
    void lightTheme_allCppTokensResolved();
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

void TestThemeTokens::noRawTokensInPerWidgetStyleSheet()
{
    const QStringList offenders = findRawTokensInSetStyleSheet();
    QVERIFY2(offenders.isEmpty(),
             qPrintable(QString(
                 "Per-widget setStyleSheet() calls must not contain raw "
                 "@token literals — only the global qApp->setStyleSheet() "
                 "goes through token substitution. Resolve via "
                 "sv->value(\"@token\", fallback).toString() and .arg(...) it "
                 "into the stylesheet string. Offenders:\n  %1")
                 .arg(offenders.join("\n  "))));
}

void TestThemeTokens::darkTheme_allCppTokensResolved()
{
    QSet<QString> cppTokens = extractCppTokens();
    QVERIFY2(!cppTokens.isEmpty(),
             "Failed to extract any @token literals from shared/ C++ — "
             "the regex or the source tree moved");

    QMap<QString, QString> values = loadValuesIni(darkValuesPath());
    QVERIFY2(!values.isEmpty(), "Failed to load dark theme values.ini");

    for (const QString &token : cppTokens) {
        QVERIFY2(values.contains(token),
                 qPrintable(QString("Dark theme missing C++-referenced token: %1").arg(token)));
    }
}

void TestThemeTokens::lightTheme_allCppTokensResolved()
{
    QSet<QString> cppTokens = extractCppTokens();
    QVERIFY2(!cppTokens.isEmpty(),
             "Failed to extract any @token literals from shared/ C++ — "
             "the regex or the source tree moved");

    QMap<QString, QString> values = loadValuesIni(lightValuesPath());
    QVERIFY2(!values.isEmpty(), "Failed to load light theme values.ini");

    for (const QString &token : cppTokens) {
        QVERIFY2(values.contains(token),
                 qPrintable(QString("Light theme missing C++-referenced token: %1").arg(token)));
    }
}

QTEST_MAIN(TestThemeTokens)
#include "test_theme_tokens.moc"
