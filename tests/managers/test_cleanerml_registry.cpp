#include <QtTest>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>

#include "Managers/cleanerml_registry.h"
#include <Tools/cleanerml_platform.h>
#include "app_cleaners_dialog.h"

// SSO-25782: the shipped CleanerML definitions end to end — loaded from the
// compiled-in resource, reduced for the platform, and run against a temp
// "home" through the batch provider — plus the picker dialog.

using namespace CleanerML;

class TestCleanerMLRegistry : public QObject
{
    Q_OBJECT

private:
    QScopedPointer<QTemporaryDir> mTmp;
    CleanerActionInterpreter::SandboxRoots mRoots;

    static void writeFile(const QString &path, const QByteArray &content = "x")
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    static Cleaner byId(const QList<Cleaner> &cleaners, const QString &id)
    {
        for (const Cleaner &c : cleaners) {
            if (c.id == id)
                return c;
        }
        return Cleaner();
    }

    static QList<TrustSafetyActionItem> scan(TrustSafetyActionProvider &provider)
    {
        QList<TrustSafetyActionItem> items;
        provider.scan(nullptr, [&items](const TrustSafetyActionItem &item) { items.append(item); });
        return items;
    }

private slots:
    void init()
    {
        mTmp.reset(new QTemporaryDir());
        QVERIFY(mTmp->isValid());
        mRoots.home = mTmp->filePath("home");
        mRoots.cache = mRoots.home + "/.cache";
        QVERIFY(QDir().mkpath(mRoots.cache));
    }

    void bundledResource_loadsAndIsUsableOnThisPlatform()
    {
        QList<ParseError> errors;
        const QList<Cleaner> cleaners = CleanerMLRegistry::load({CleanerMLRegistry::bundledDir()}, currentOs(), &errors);
        QVERIFY(errors.isEmpty());
        QVERIFY2(cleaners.size() >= 20, qPrintable(QString::number(cleaners.size())));
        QVERIFY(!byId(cleaners, "bash").options.isEmpty());
        QVERIFY(QFile::exists(CleanerMLRegistry::bundledDir() + "/COPYING.bleachbit"));

        // Nothing offered to the user may still hold an action Nexis cannot run.
        for (const Cleaner &c : cleaners) {
            QVERIFY(!c.options.isEmpty());
            for (const Option &o : c.options) {
                QVERIFY(!o.actions.isEmpty());
                for (const Action &a : o.actions)
                    QVERIFY2(a.type != ActionType::Unsupported, qPrintable(c.id + "/" + o.id));
            }
        }
    }

    void userDirectory_overridesBundledById()
    {
        QTemporaryDir user;
        writeFile(user.filePath("bash.xml"),
            "<cleaner id=\"bash\"><label>My Bash</label>"
            "<option id=\"only\"><label>Only</label>"
            "<action command=\"delete\" search=\"file\" path=\"~/.my_history\"/></option></cleaner>");

        const QList<Cleaner> cleaners =
            CleanerMLRegistry::load({CleanerMLRegistry::bundledDir(), user.path()}, currentOs());
        const Cleaner bash = byId(cleaners, "bash");
        QCOMPARE(bash.label, QStringLiteral("My Bash"));
        QCOMPARE(bash.options.size(), 1);
    }

    void hasData_onlyWhenTheAppLeftSomething()
    {
        const QList<Cleaner> cleaners = CleanerMLRegistry::load({CleanerMLRegistry::bundledDir()}, currentOs());
        const Cleaner bash = byId(cleaners, "bash");
        const Cleaner thumbnails = byId(cleaners, "thumbnails");

        QVERIFY(!CleanerMLRegistry::hasData(thumbnails, mRoots));
        writeFile(mRoots.cache + "/thumbnails/large/a.png");
        QVERIFY(CleanerMLRegistry::hasData(thumbnails, mRoots));

        QVERIFY(CleanerMLRegistry::hasData(bash, mRoots)); // its glob's directory is home itself
    }

    void batchProvider_runsRealBundledCleaners_selectedOptionsOnly()
    {
        writeFile(mRoots.home + "/.bash_history", "secret commands");
        writeFile(mRoots.home + "/.bash_history-1234.tmp");
        writeFile(mRoots.cache + "/thumbnails/large/a.png");
        writeFile(mRoots.cache + "/thumbnails/normal/b.png");
        writeFile(mRoots.home + "/Documents/keep.txt");

        const QList<Cleaner> cleaners = CleanerMLRegistry::load({CleanerMLRegistry::bundledDir()}, currentOs());

        CleanerMLRegistry::BatchProvider provider;
        provider.add(byId(cleaners, "bash"), {"history"}, mRoots, {});
        provider.add(byId(cleaners, "thumbnails"), {"cache"}, mRoots, {});
        provider.add(byId(cleaners, "vlc"), {}, mRoots, {}); // nothing ticked: ignored
        QVERIFY(!provider.isEmpty());

        const QList<TrustSafetyActionItem> items = scan(provider);
        QCOMPARE(items.size(), 3);

        // Dry run touches nothing.
        for (const TrustSafetyActionItem &item : items)
            QVERIFY(provider.performItem(item, /*dryRun=*/true).succeeded);
        QVERIFY(QFile::exists(mRoots.home + "/.bash_history"));

        for (const TrustSafetyActionItem &item : items)
            QVERIFY(provider.performItem(item, /*dryRun=*/false).succeeded);

        QVERIFY(!QFile::exists(mRoots.home + "/.bash_history"));
        QVERIFY(!QFile::exists(mRoots.cache + "/thumbnails/large/a.png"));
        QVERIFY(!QFile::exists(mRoots.cache + "/thumbnails/normal/b.png"));
        // The unticked "tmp" option and unrelated files are untouched.
        QVERIFY(QFile::exists(mRoots.home + "/.bash_history-1234.tmp"));
        QVERIFY(QFile::exists(mRoots.home + "/Documents/keep.txt"));
    }

    void batchProvider_honoursExclusions_andRejectsUnknownCleaner()
    {
        writeFile(mRoots.cache + "/thumbnails/large/a.png");
        writeFile(mRoots.cache + "/thumbnails/normal/b.png");
        const QList<Cleaner> cleaners = CleanerMLRegistry::load({CleanerMLRegistry::bundledDir()}, currentOs());

        CleanerService::ExclusionEntry keep;
        keep.type = CleanerService::ExclusionEntry::Folder;
        keep.path = mRoots.cache + "/thumbnails/large";

        CleanerMLRegistry::BatchProvider provider;
        provider.add(byId(cleaners, "thumbnails"), {"cache"}, mRoots, {keep});
        const QList<TrustSafetyActionItem> items = scan(provider);
        QCOMPARE(items.size(), 1);
        QVERIFY(items.first().id.endsWith("/normal/b.png"));

        TrustSafetyActionItem stray;
        stray.id = "delete::" + mRoots.cache + "/thumbnails/normal/b.png";
        stray.categoryId = "not-loaded";
        QVERIFY(!provider.performItem(stray, /*dryRun=*/false).succeeded);
        QVERIFY(QFile::exists(mRoots.cache + "/thumbnails/normal/b.png"));
    }

    void dialog_nothingPreselected_runningAppLocked_filterHides()
    {
        const QList<Cleaner> cleaners = CleanerMLRegistry::load({CleanerMLRegistry::bundledDir()}, currentOs());
        const QList<Cleaner> shown = {byId(cleaners, "bash"), byId(cleaners, "thumbnails")};

        AppCleanersDialog dialog(shown, {"thumbnails"});
        auto *tree = dialog.findChild<QTreeWidget *>("appCleanersTree");
        auto *review = dialog.findChild<QPushButton *>("btnReviewAppCleaners");
        auto *filter = dialog.findChild<QLineEdit *>("editAppCleanerFilter");
        QVERIFY(tree && review && filter);

        QCOMPARE(tree->topLevelItemCount(), 2);
        QVERIFY(dialog.selection().isEmpty());
        QVERIFY(!review->isEnabled());

        QTreeWidgetItem *bash = tree->topLevelItem(0);
        QTreeWidgetItem *thumbnails = tree->topLevelItem(1);
        QCOMPARE(bash->childCount(), 2);
        QVERIFY(!(thumbnails->child(0)->flags() & Qt::ItemIsUserCheckable));

        bash->child(0)->setCheckState(0, Qt::Checked);
        QVERIFY(review->isEnabled());
        const auto selection = dialog.selection();
        QCOMPARE(selection.size(), 1);
        QCOMPARE(selection.value("bash"), QSet<QString>{"history"});

        filter->setText("thumb");
        QVERIFY(bash->isHidden());
        QVERIFY(!thumbnails->isHidden());
    }
};

QTEST_MAIN(TestCleanerMLRegistry)
#include "test_cleanerml_registry.moc"
