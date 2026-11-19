#include <QtTest>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "cleaning_history_dialog.h"

// "Recently removed" dialog: lists runs with their items, and only offers
// Restore for items that went to the Trash and have not been restored yet.
// The restore refusals themselves are covered in LifecycleAuditLogTests.

using LifecycleAuditLog::Action;
using LifecycleAuditLog::Entry;

class TestCleaningHistoryDialog : public QObject
{
    Q_OBJECT

private:
    static Entry entry(const QString &path, Action action, const QString &trashDestination = QString())
    {
        Entry e;
        e.timestamp = QDateTime::currentDateTimeUtc();
        e.batchId = QStringLiteral("run-1");
        e.originalPath = path;
        e.canonicalPath = path;
        e.action = action;
        e.trashDestination = trashDestination;
        e.sizeBytes = 2048;
        e.runSource = LifecycleAuditLog::RunSource::Scheduled;
        e.category = QStringLiteral("Old Downloads");
        return e;
    }

    static QTreeWidgetItem *childByPath(const QTreeWidget *tree, const QString &path)
    {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *run = tree->topLevelItem(i);
            for (int j = 0; j < run->childCount(); ++j) {
                if (run->child(j)->text(0) == path)
                    return run->child(j);
            }
        }
        return nullptr;
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() { QFile::remove(LifecycleAuditLog::logFilePath()); }

    void restoreOnlyEnabledForTrashedItems()
    {
        QVERIFY(LifecycleAuditLog::appendAll({
            entry(QStringLiteral("/home/u/Downloads/a.zip"), Action::MovedToTrash,
                  QStringLiteral("/home/u/.local/share/Trash/files/a.zip")),
            entry(QStringLiteral("/home/u/.cache/b"), Action::PermanentlyDeleted),
            entry(QStringLiteral("/home/u/.cache/c"), Action::Skipped),
        }));

        CleaningHistoryDialog dlg;
        auto *tree = dlg.findChild<QTreeWidget *>();
        auto *restore = dlg.findChild<QPushButton *>(QStringLiteral("btnRestoreRemoved"));
        QVERIFY(tree && restore);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QVERIFY(!restore->isEnabled());

        tree->setCurrentItem(childByPath(tree, QStringLiteral("/home/u/Downloads/a.zip")));
        QVERIFY(restore->isEnabled());

        tree->setCurrentItem(childByPath(tree, QStringLiteral("/home/u/.cache/b")));
        QVERIFY(!restore->isEnabled());

        tree->setCurrentItem(childByPath(tree, QStringLiteral("/home/u/.cache/c")));
        QVERIFY(!restore->isEnabled());

        tree->setCurrentItem(tree->topLevelItem(0));
        QVERIFY(!restore->isEnabled());
    }

    void alreadyRestoredItemIsNotRestorable()
    {
        const QString trashed = QStringLiteral("/home/u/.local/share/Trash/files/a.zip");
        Entry restored = entry(QStringLiteral("/home/u/Downloads/a.zip"), Action::Restored, trashed);
        QVERIFY(LifecycleAuditLog::appendAll({
            entry(QStringLiteral("/home/u/Downloads/a.zip"), Action::MovedToTrash, trashed),
            restored,
        }));

        CleaningHistoryDialog dlg;
        auto *tree = dlg.findChild<QTreeWidget *>();
        QTreeWidgetItem *item = childByPath(tree, QStringLiteral("/home/u/Downloads/a.zip"));
        QVERIFY(item);
        QCOMPARE(item->text(3), QStringLiteral("Restored"));

        tree->setCurrentItem(item);
        QVERIFY(!dlg.findChild<QPushButton *>(QStringLiteral("btnRestoreRemoved"))->isEnabled());
    }

    void emptyLogShowsPlaceholder()
    {
        CleaningHistoryDialog dlg;
        auto *tree = dlg.findChild<QTreeWidget *>();
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 0);
    }

    void runSourcesAreLabelled()
    {
        QVERIFY(CleaningHistoryDialog::runSourceLabel(LifecycleAuditLog::RunSource::Interactive)
                != CleaningHistoryDialog::runSourceLabel(LifecycleAuditLog::RunSource::Scheduled));
        QVERIFY(!CleaningHistoryDialog::runSourceLabel(QString()).isEmpty());
        QVERIFY(CleaningHistoryDialog::skipReasonLabel(QStringLiteral("excluded"))
                    .contains(QStringLiteral("excluded")));
    }
};

QTEST_MAIN(TestCleaningHistoryDialog)
#include "test_cleaning_history_dialog.moc"
