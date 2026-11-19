#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "dev_artifacts_dialog.h"
#include "Managers/setting_manager.h"

// SSO-27098: the Developer Projects dialog lists stale projects with nothing
// pre-selected, hides recently active ones, and keeps the Trash action
// disabled until a project is ticked. The safety rules themselves are covered
// in DevArtifactScannerTests.

class TestDevArtifactsDialog : public QObject
{
    Q_OBJECT

private:
    static void writeFile(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    static void makeProject(const QString &root, const QDateTime &activity)
    {
        writeFile(root + "/package.json");
        writeFile(root + "/node_modules/dep/index.js");
        QFile f(root + "/package.json");
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(activity, QFile::FileModificationTime));
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void scan_listsStaleProjectUncheckedAndHidesRecentOne()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString base = QFileInfo(tmp.path()).canonicalFilePath();
        makeProject(base + "/stale", QDateTime::currentDateTime().addDays(-90));
        makeProject(base + "/fresh", QDateTime::currentDateTime().addDays(-2));
        SettingManager::ins()->setDevArtifactRoots({base});

        DevArtifactsDialog dialog;
        auto *tree = dialog.findChild<QTreeWidget *>("devArtifactTree");
        auto *trash = dialog.findChild<QPushButton *>("btnDevArtifactTrash");
        QVERIFY(tree && trash);
        QVERIFY(!trash->isEnabled());

        QVERIFY(QMetaObject::invokeMethod(&dialog, "onScan"));
        QTRY_COMPARE_WITH_TIMEOUT(tree->topLevelItemCount(), 1, 10000);

        const QTreeWidgetItem *row = tree->topLevelItem(0);
        QCOMPARE(row->text(0), QString("stale"));
        QCOMPARE(row->checkState(0), Qt::Unchecked);
        QCOMPARE(row->childCount(), 1);
        QVERIFY(!trash->isEnabled());

        const_cast<QTreeWidgetItem *>(row)->setCheckState(0, Qt::Checked);
        QVERIFY(trash->isEnabled());
        const_cast<QTreeWidgetItem *>(row)->setCheckState(0, Qt::Unchecked);
        QVERIFY(!trash->isEnabled());
    }

    void scan_homeFolderRootIsReportedNotSearched()
    {
        SettingManager::ins()->setDevArtifactRoots({QDir::homePath()});

        DevArtifactsDialog dialog;
        auto *tree = dialog.findChild<QTreeWidget *>("devArtifactTree");
        auto *status = dialog.findChild<QLabel *>("lblDevArtifactStatus");
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onScan"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("Not searched"), 10000);
        QCOMPARE(tree->topLevelItemCount(), 0);
    }

    void cleanupTestCase() { SettingManager::ins()->setDevArtifactRoots({}); }
};

QTEST_MAIN(TestDevArtifactsDialog)
#include "test_dev_artifacts_dialog.moc"
