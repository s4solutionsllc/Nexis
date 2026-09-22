#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

// Source-level guard for the dialog button rules in
// shared/nexis/Common/dialog_buttons.h: confirm buttons are verbs (never "OK"),
// "Skip" is retired in favour of "Cancel", and a button styled as danger is
// never made the default button.
class TestDialogButtons : public QObject
{
    Q_OBJECT

    QStringList sourceFiles() const
    {
        QStringList files;
        const QString root = QString::fromLatin1(PROJECT_SOURCE_DIR);
        for (const QString &dir : {root + "/shared", root + "/macos", root + "/linux"}) {
            QDirIterator it(dir, {"*.cpp", "*.mm", "*.ui"}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
                files << it.next();
        }
        return files;
    }

    QString read(const QString &path) const
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(f.readAll());
    }

private slots:
    void sourcesAreFound()
    {
        QVERIFY(sourceFiles().size() > 100);
    }

    void noOkOrSkipButtons()
    {
        static const QRegularExpression rx(
            QStringLiteral("QPushButton\\(\\s*tr\\(\"(OK|Ok|Skip)\"\\)|<string>(OK|Skip)</string>"));
        QStringList offenders;
        for (const QString &path : sourceFiles()) {
            if (rx.match(read(path)).hasMatch())
                offenders << path;
        }
        QVERIFY2(offenders.isEmpty(), qPrintable("Use a verb / \"Cancel\" instead: " + offenders.join(", ")));
    }

    void dangerButtonsAreNeverDefault()
    {
        static const QRegularExpression dangerRx(
            QStringLiteral("(\\w+)->(?:setAccessibleName|setProperty)\\([^;]*\"danger\"[^;]*\\);"));
        QStringList offenders;
        for (const QString &path : sourceFiles()) {
            const QString text = read(path);
            auto it = dangerRx.globalMatch(text);
            while (it.hasNext()) {
                const QString var = it.next().captured(1);
                const QRegularExpression defaultRx(
                    QStringLiteral("\\b%1->setDefault\\(\\s*true\\s*\\)").arg(QRegularExpression::escape(var)));
                if (defaultRx.match(text).hasMatch())
                    offenders << QStringLiteral("%1 (%2)").arg(path, var);
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable("Destructive button is the default: " + offenders.join(", ")));
    }
};

QTEST_MAIN(TestDialogButtons)
#include "test_dialog_buttons.moc"
