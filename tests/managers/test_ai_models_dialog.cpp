#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>

#include "ai_models_dialog.h"

// SSO-27099: the picker in front of the local-AI-model removal. It lists and
// summarises; the removal itself is covered by AiModelStoreToolTests.

using namespace AiModelStore;

namespace {

Blob blob(const QString &key, quint64 size, int refs)
{
    Blob b;
    b.path = "/store/" + key;
    b.key = key;
    b.size = size;
    b.refCount = refs;
    return b;
}

Model model(Tool tool, const QString &name, const QList<Blob> &blobs)
{
    Model m;
    m.tool = tool;
    m.id = QString::number(int(tool)) + ":" + name;
    m.name = name;
    m.topLevelPaths = {"/store/" + name};
    m.blobs = blobs;
    return m;
}

} // namespace

class TestAiModelsDialog : public QObject
{
    Q_OBJECT

private:
    // a and b share "shared" (1000 bytes); b also owns 2000, a owns 500.
    static QList<Model> models()
    {
        return {model(Tool::Ollama, "a:latest", {blob("a", 500, 1), blob("shared", 1000, 2)}),
                model(Tool::Ollama, "b:latest", {blob("b", 2000, 1), blob("shared", 1000, 2)}),
                model(Tool::LmStudio, "pub/lm", {blob("lm", 4000, 1)})};
    }

private slots:
    void nothingPreselected_removeDisabledUntilAModelIsChecked()
    {
        AiModelsDialog dialog(models(), Environment());
        auto *tree = dialog.findChild<QTreeWidget *>("aiModelTree");
        auto *remove = dialog.findChild<QPushButton *>("btnRemoveAiModels");
        QVERIFY(tree && remove);

        QCOMPARE(tree->topLevelItemCount(), 3);
        QVERIFY(dialog.selectedModels().isEmpty());
        QVERIFY(!remove->isEnabled());

        tree->topLevelItem(2)->setCheckState(0, Qt::Checked);
        QVERIFY(remove->isEnabled());
        QCOMPARE(dialog.selectedModels().size(), 1);
        QCOMPARE(dialog.selectedModels().first().name, QStringLiteral("pub/lm"));
    }

    void summary_countsSharedDataOnlyWhenEveryOwnerIsPicked()
    {
        const QList<Model> all = models();
        const QString none = AiModelsDialog::summaryFor({});
        QVERIFY(!none.isEmpty());

        // a alone frees 500 (the shared 1000 stays for b); a+b frees 3500.
        QCOMPARE(reclaimableBytes({all[0]}), quint64(500));
        QCOMPARE(reclaimableBytes({all[0], all[1]}), quint64(3500));

        AiModelsDialog dialog(all, Environment());
        auto *tree = dialog.findChild<QTreeWidget *>("aiModelTree");
        auto *summary = dialog.findChild<QLabel *>("lblAiModelSummary");
        tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
        const QString one = summary->text();
        tree->topLevelItem(1)->setCheckState(0, Qt::Checked);
        QVERIFY(summary->text() != one);
        QCOMPARE(summary->text(), AiModelsDialog::summaryFor({all[0], all[1]}));
    }

    void everyRowWarnsAboutRedownloadAndStaleToolLists()
    {
        const QList<Model> all = models();
        const QString ollamaCmd = AiModelsDialog::warningFor(all[0], true);
        QVERIFY(ollamaCmd.contains("Re-download"));
        QVERIFY(ollamaCmd.contains("ollama rm"));
        QVERIFY(ollamaCmd.contains("shared with another model"));

        const QString ollamaTrash = AiModelsDialog::warningFor(all[0], false);
        QVERIFY(ollamaTrash.contains("keep listing"));

        const QString lm = AiModelsDialog::warningFor(all[2], false);
        QVERIFY(lm.contains("Re-download"));
        QVERIFY(lm.contains("keep listing"));
        QVERIFY(lm.contains("Trash"));
    }

    void emptyList_showsAMessageAndNothingIsCheckable()
    {
        AiModelsDialog dialog({}, Environment());
        QVERIFY(dialog.selectedModels().isEmpty());
        auto *remove = dialog.findChild<QPushButton *>("btnRemoveAiModels");
        QVERIFY(!remove->isEnabled());
    }
};

QTEST_MAIN(TestAiModelsDialog)
#include "test_ai_models_dialog.moc"
