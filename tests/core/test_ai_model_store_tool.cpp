// SSO-27099: local AI model store finder. Every test builds fixture stores
// (Ollama manifests + blobs, a Hugging Face hub cache, LM Studio folders, a
// llama.cpp cache) under a temporary home; nothing touches the real one and no
// real Trash or `ollama` binary is used.

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

#include "Tools/ai_model_store_tool.h"
#include "Tools/lifecycle_audit_log.h"

using namespace AiModelStore;

namespace {

const QString kShared = QString(64, 'a');   // used by gemma3 and llama3
const QString kGemmaOnly = QString(64, 'b');
const QString kLlamaOnly = QString(64, 'c');
const QString kHfOnly = QString(64, 'd');

void writeFile(const QString &path, qint64 size, const QByteArray &content = QByteArray())
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.isEmpty() ? QByteArray(int(size), 'x') : content);
}

QByteArray manifestJson(const QStringList &digests)
{
    QStringList layers;
    for (const QString &d : digests)
        layers << QStringLiteral("{\"digest\":\"%1\"}").arg(d);
    return QStringLiteral("{\"schemaVersion\":2,\"config\":{\"digest\":\"sha256:%1\"},\"layers\":[%2]}")
        .arg(QString(64, 'e'), layers.join(','))
        .toUtf8();
}

} // namespace

class TestAiModelStoreTool : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir mHome;
    QTemporaryDir mTrash;
    QStringList mCommands;
    int mTrashSeq = 0;

    QString home() const { return mHome.path(); }

    Environment env() const
    {
        Environment e;
        e.home = home();
        e.lookForOllama = false;
        return e;
    }

    Hooks trashHooks()
    {
        Hooks h;
        h.moveToTrash = [this](const QString &path, QString *dest) {
            const QString target = mTrash.path() + QLatin1Char('/') + QString::number(++mTrashSeq)
                                   + QLatin1Char('-') + QFileInfo(path).fileName();
            *dest = target;
            return QDir().rename(path, target);
        };
        return h;
    }

    void buildOllama()
    {
        const QString root = home() + "/.ollama/models";
        writeFile(root + "/blobs/sha256-" + kShared, 1000);
        writeFile(root + "/blobs/sha256-" + kGemmaOnly, 2000);
        writeFile(root + "/blobs/sha256-" + kLlamaOnly, 3000);
        writeFile(root + "/blobs/sha256-" + kHfOnly, 500);
        writeFile(root + "/blobs/sha256-" + QString(64, 'e'), 10);
        writeFile(root + "/manifests/registry.ollama.ai/library/gemma3/latest", 0,
                  manifestJson({"sha256:" + kShared, "sha256:" + kGemmaOnly}));
        writeFile(root + "/manifests/registry.ollama.ai/library/llama3/8b", 0,
                  manifestJson({"sha256:" + kShared, "sha256:" + kLlamaOnly}));
        writeFile(root + "/manifests/hf.co/someone/repo/Q4_K_M", 0, manifestJson({"sha256:" + kHfOnly}));
    }

    void buildHuggingFace()
    {
        const QString repo = home() + "/.cache/huggingface/hub/models--acme--tiny";
        writeFile(repo + "/blobs/h1", 500);
        writeFile(repo + "/blobs/h2", 700);
        QDir().mkpath(repo + "/snapshots/rev1");
        QDir().mkpath(repo + "/snapshots/rev2");
        QVERIFY(QFile::link("../../blobs/h1", repo + "/snapshots/rev1/a.bin"));
        QVERIFY(QFile::link("../../blobs/h1", repo + "/snapshots/rev2/a.bin"));
        QVERIFY(QFile::link("../../blobs/h2", repo + "/snapshots/rev2/b.bin"));
        writeFile(home() + "/.cache/huggingface/hub/datasets--acme--data/blobs/x", 9000);
    }

    void buildLmStudio()
    {
        writeFile(home() + "/.lmstudio/models/lmstudio-community/Qwen-7B-GGUF/qwen-7b-Q4.gguf", 4000);
        writeFile(home() + "/.lmstudio/models/lmstudio-community/Qwen-7B-GGUF/README.md", 20);
        writeFile(home() + "/.cache/lm-studio/models/oldpub/oldmodel/weights.safetensors", 1500);
    }

    void buildLlamaCpp()
    {
        const QString root = home() + "/.cache/llama.cpp";
        writeFile(root + "/org_repo_small-Q4.gguf", 800);
        writeFile(root + "/org_repo_small-Q4.gguf.etag", 5);
        writeFile(root + "/org_repo_big-00001-of-00002.gguf", 300);
        writeFile(root + "/org_repo_big-00002-of-00002.gguf", 400);
    }

    static const Model *find(const QList<Model> &models, const QString &id)
    {
        for (const Model &m : models) {
            if (m.id == id)
                return &m;
        }
        return nullptr;
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        QDir(home()).removeRecursively();
        QDir().mkpath(home());
        QDir(mTrash.path()).removeRecursively();
        QDir().mkpath(mTrash.path());
        QFile::remove(LifecycleAuditLog::logFilePath());
        mCommands.clear();
    }

    void scan_emptyHome_findsNothing() { QVERIFY(scan(env()).isEmpty()); }

    void scan_ollama_nameManifestAndSharedBlobs()
    {
        buildOllama();
        const QList<Model> models = scan(env());

        const Model *gemma = find(models, "ollama:gemma3:latest");
        const Model *llama = find(models, "ollama:llama3:8b");
        const Model *hf = find(models, "ollama:hf.co/someone/repo:Q4_K_M");
        QVERIFY(gemma && llama && hf);
        QCOMPARE(gemma->tool, Tool::Ollama);

        const quint64 manifestBytes = QFileInfo(gemma->topLevelPaths.first()).size();
        // manifest + config blob (10, used by all three models) + the weights
        // gemma3 and llama3 share (1000) + gemma3's own (2000)
        QCOMPARE(gemma->totalBytes(), quint64(10 + 1000 + 2000) + manifestBytes);
        QCOMPARE(gemma->sharedBytes(), quint64(1010));
        QCOMPARE(gemma->exclusiveBytes(), quint64(2000) + manifestBytes);
        QCOMPARE(hf->sharedBytes(), quint64(10));
        QCOMPARE(hf->removeCommandText(), QStringLiteral("ollama rm hf.co/someone/repo:Q4_K_M"));
    }

    void scan_ollama_ignoresManifestDigestsThatAreNotHashes()
    {
        const QString root = home() + "/.ollama/models";
        writeFile(root + "/blobs/sha256-" + kHfOnly, 500);
        writeFile(home() + "/secret.txt", 7777);
        writeFile(root + "/manifests/registry.ollama.ai/library/evil/latest", 0,
                  manifestJson({"sha256:../../../secret.txt", "sha256:" + kHfOnly}));
        const QList<Model> models = scan(env());
        const Model *evil = find(models, "ollama:evil:latest");
        QVERIFY(evil);
        for (const Blob &b : evil->blobs)
            QVERIFY2(!b.path.contains("secret"), qPrintable(b.path));
        QCOMPARE(evil->sharedBytes(), quint64(0));
    }

    void scan_ollama_honoursOllamaModelsButNotHomeOrRoot()
    {
        const QString moved = home() + "/external/models";
        writeFile(moved + "/blobs/sha256-" + kHfOnly, 500);
        writeFile(moved + "/manifests/registry.ollama.ai/library/moved/latest", 0,
                  manifestJson({"sha256:" + kHfOnly}));

        Environment e = env();
        e.vars["OLLAMA_MODELS"] = moved;
        QVERIFY(find(scan(e), "ollama:moved:latest"));

        e.vars["OLLAMA_MODELS"] = home();
        QVERIFY(scan(e).isEmpty());
        e.vars["OLLAMA_MODELS"] = "/";
        QVERIFY(scan(e).isEmpty());
    }

    void scan_huggingFace_countsEachBlobOnce_andSkipsDatasets()
    {
        buildHuggingFace();
        const QList<Model> models = scan(env());
        QCOMPARE(models.size(), 1);
        const Model &m = models.first();
        QCOMPARE(m.id, QStringLiteral("huggingface:acme/tiny"));
        QCOMPARE(m.totalBytes(), quint64(1200));
        QCOMPARE(m.exclusiveBytes(), quint64(1200));
        QCOMPARE(m.topLevelPaths, QStringList{home() + "/.cache/huggingface/hub/models--acme--tiny"});
    }

    void scan_huggingFace_cacheEnvOverridesInOrder()
    {
        writeFile(home() + "/hf-home/hub/models--a--b/blobs/x", 10);
        writeFile(home() + "/hub-cache/models--c--d/blobs/y", 20);
        Environment e = env();
        e.vars["HF_HOME"] = home() + "/hf-home";
        QVERIFY(find(scan(e), "huggingface:a/b"));
        e.vars["HF_HUB_CACHE"] = home() + "/hub-cache";
        const QList<Model> models = scan(e);
        QVERIFY(find(models, "huggingface:c/d"));
        QVERIFY(!find(models, "huggingface:a/b"));
    }

    void scan_hardLinkedFilesAreCountedOnce()
    {
        const QString repo = home() + "/.cache/huggingface/hub/models--acme--copy";
        writeFile(repo + "/blobs/h1", 600);
        QDir().mkpath(repo + "/snapshots/rev1");
        // A hard link to the same inode, as on filesystems without symlinks.
        QVERIFY(::link(QFile::encodeName(repo + "/blobs/h1"), QFile::encodeName(repo + "/snapshots/rev1/w.bin")) == 0);
        const Model *m = find(scan(env()), "huggingface:acme/copy");
        QVERIFY(m);
        QCOMPARE(m->totalBytes(), quint64(600));
    }

    void scan_lmStudio_oneRowPerWeightFolder_currentAndLegacyRoots()
    {
        buildLmStudio();
        const QList<Model> models = scan(env());
        const Model *qwen = find(models, "lmstudio:lmstudio-community/Qwen-7B-GGUF");
        const Model *old = find(models, "lmstudio:oldpub/oldmodel");
        QVERIFY(qwen && old);
        QCOMPARE(qwen->totalBytes(), quint64(4020));
        QCOMPARE(old->totalBytes(), quint64(1500));
        QCOMPARE(qwen->topLevelPaths,
                 QStringList{home() + "/.lmstudio/models/lmstudio-community/Qwen-7B-GGUF"});
    }

    void scan_llamaCpp_groupsShardsAndSidecars()
    {
        buildLlamaCpp();
        const QList<Model> models = scan(env());
        const Model *small = find(models, "llamacpp:org_repo_small-Q4");
        const Model *big = find(models, "llamacpp:org_repo_big");
        QVERIFY(small && big);
        QCOMPARE(small->totalBytes(), quint64(805));
        QCOMPARE(small->topLevelPaths.size(), 2);
        QCOMPARE(big->totalBytes(), quint64(700));
        QCOMPARE(big->topLevelPaths.size(), 2);
    }

    void reclaimableBytes_sharedBlobCountedOnceAndOnlyWhenAllOwnersPicked()
    {
        buildOllama();
        const QList<Model> models = scan(env());
        const Model gemma = *find(models, "ollama:gemma3:latest");
        const Model llama = *find(models, "ollama:llama3:8b");

        // Alone, each frees its own data; the shared 1000 stays with the other.
        QCOMPARE(reclaimableBytes({gemma}), gemma.exclusiveBytes());
        QCOMPARE(reclaimableBytes({llama}), llama.exclusiveBytes());
        // Together the 1000 they share is freed, once. The 10-byte config blob
        // is also used by the hf.co model, which is not picked, so it stays.
        const quint64 gemmaManifest = QFileInfo(gemma.topLevelPaths.first()).size();
        const quint64 llamaManifest = QFileInfo(llama.topLevelPaths.first()).size();
        QCOMPARE(reclaimableBytes({gemma, llama}),
                 quint64(1000 + 2000 + 3000) + gemmaManifest + llamaManifest);
        QVERIFY(reclaimableBytes({gemma, llama}) < gemma.totalBytes() + llama.totalBytes());
    }

    void remove_ollamaWithoutBinary_trashesManifestAndOnlyUnsharedBlobs()
    {
        buildOllama();
        const QList<Model> models = scan(env());
        const Model gemma = *find(models, "ollama:gemma3:latest");
        const QString root = home() + "/.ollama/models";

        const QList<RemoveOutcome> out = remove({gemma}, env(), trashHooks());
        QCOMPARE(out.size(), 1);
        QCOMPARE(out.first().status, RemoveOutcome::Status::Removed);
        QVERIFY(!out.first().usedToolCommand);
        QVERIFY(!QFileInfo::exists(root + "/manifests/registry.ollama.ai/library/gemma3/latest"));
        QVERIFY(!QFileInfo::exists(root + "/blobs/sha256-" + kGemmaOnly));
        // llama3 still needs the shared blob; the config blob is used by the
        // other models too.
        QVERIFY(QFileInfo::exists(root + "/blobs/sha256-" + kShared));
        QVERIFY(QFileInfo::exists(root + "/blobs/sha256-" + kLlamaOnly));
        QCOMPARE(out.first().freedBytes, gemma.exclusiveBytes());
    }

    void remove_ollamaBothSharingModels_freesSharedBlobOnce()
    {
        buildOllama();
        const QList<Model> models = scan(env());
        const Model gemma = *find(models, "ollama:gemma3:latest");
        const Model llama = *find(models, "ollama:llama3:8b");
        const QString root = home() + "/.ollama/models";

        const QList<RemoveOutcome> out = remove({gemma, llama}, env(), trashHooks());
        quint64 freed = 0;
        for (const RemoveOutcome &o : out) {
            QCOMPARE(o.status, RemoveOutcome::Status::Removed);
            freed += o.freedBytes;
        }
        QVERIFY(!QFileInfo::exists(root + "/blobs/sha256-" + kShared));
        QCOMPARE(freed, reclaimableBytes({gemma, llama}));
    }

    void remove_ollamaWithBinary_usesOllamaRm_andNeverTrashes()
    {
        buildOllama();
        const Model gemma = *find(scan(env()), "ollama:gemma3:latest");
        const QString root = home() + "/.ollama/models";
        const quint64 manifestBytes = QFileInfo(gemma.topLevelPaths.first()).size();

        Environment e = env();
        e.ollamaBinary = "/fake/ollama";
        Hooks h;
        h.moveToTrash = [](const QString &, QString *) { return false; };
        h.run = [&](const QString &program, const QStringList &args) {
            mCommands << program + " " + args.join(' ');
            // Stand-in for what the real command does to the store.
            QFile::remove(root + "/manifests/registry.ollama.ai/library/gemma3/latest");
            QFile::remove(root + "/blobs/sha256-" + kGemmaOnly);
            ExecResult r;
            r.exitCode = 0;
            return r;
        };

        const QList<RemoveOutcome> out = remove({gemma}, e, h);
        QCOMPARE(mCommands, QStringList{"/fake/ollama rm gemma3:latest"});
        QCOMPARE(out.first().status, RemoveOutcome::Status::Removed);
        QVERIFY(out.first().usedToolCommand);
        QCOMPARE(out.first().freedBytes, quint64(2000) + manifestBytes);
    }

    void remove_ollamaCommandFailure_isReportedAndNothingIsTouched()
    {
        buildOllama();
        const Model gemma = *find(scan(env()), "ollama:gemma3:latest");
        Environment e = env();
        e.ollamaBinary = "/fake/ollama";
        Hooks h;
        h.moveToTrash = [](const QString &, QString *) { return false; };
        h.run = [](const QString &, const QStringList &) {
            ExecResult r;
            r.exitCode = 1;
            r.error = "could not connect to ollama app";
            return r;
        };
        const RemoveOutcome o = remove({gemma}, e, h).first();
        QCOMPARE(o.status, RemoveOutcome::Status::Failed);
        QVERIFY(o.message.contains("could not connect"));
        QVERIFY(QFileInfo::exists(gemma.topLevelPaths.first()));
    }

    void remove_huggingFaceAndLmStudioAndLlamaCpp_goToTrashAsAUnit()
    {
        buildHuggingFace();
        buildLmStudio();
        buildLlamaCpp();
        const QList<Model> all = scan(env());
        const QList<Model> picked = {*find(all, "huggingface:acme/tiny"),
                                     *find(all, "lmstudio:lmstudio-community/Qwen-7B-GGUF"),
                                     *find(all, "llamacpp:org_repo_big")};

        const QList<RemoveOutcome> out = remove(picked, env(), trashHooks());
        for (const RemoveOutcome &o : out)
            QCOMPARE(o.status, RemoveOutcome::Status::Removed);
        QVERIFY(!QFileInfo::exists(home() + "/.cache/huggingface/hub/models--acme--tiny"));
        QVERIFY(!QFileInfo::exists(home() + "/.lmstudio/models/lmstudio-community/Qwen-7B-GGUF"));
        QVERIFY(!QFileInfo::exists(home() + "/.cache/llama.cpp/org_repo_big-00001-of-00002.gguf"));
        // Not picked, so untouched.
        QVERIFY(QFileInfo::exists(home() + "/.cache/llama.cpp/org_repo_small-Q4.gguf"));
        QVERIFY(QFileInfo::exists(home() + "/.cache/huggingface/hub/datasets--acme--data"));
        QCOMPARE(out.at(0).freedBytes, quint64(1200));
    }

    void remove_isRecordedInTheAuditLogAsRestorableTrashMoves()
    {
        buildLmStudio();
        const Model m = *find(scan(env()), "lmstudio:oldpub/oldmodel");
        remove({m}, env(), trashHooks());

        const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
        QCOMPARE(entries.size(), 1);
        const LifecycleAuditLog::Entry &e = entries.first();
        QCOMPARE(e.action, LifecycleAuditLog::Action::MovedToTrash);
        QCOMPARE(e.originalPath, m.topLevelPaths.first());
        QVERIFY(!e.trashDestination.isEmpty());
        QCOMPARE(e.sizeBytes, quint64(1500));
        QCOMPARE(e.runSource, LifecycleAuditLog::RunSource::Interactive);
    }

    void remove_modelGoneSinceScan_isReportedMissing()
    {
        buildLmStudio();
        const Model m = *find(scan(env()), "lmstudio:oldpub/oldmodel");
        QDir(m.topLevelPaths.first()).removeRecursively();
        const RemoveOutcome o = remove({m}, env(), trashHooks()).first();
        QCOMPARE(o.status, RemoveOutcome::Status::Missing);
    }

    void remove_symlinkedModelFolderIsNeitherFollowedNorTrashed()
    {
        buildLmStudio();
        const Model m = *find(scan(env()), "lmstudio:oldpub/oldmodel");
        const QString target = m.topLevelPaths.first();
        const QString moved = home() + "/elsewhere";
        QVERIFY(QDir().rename(target, moved));
        QVERIFY(QFile::link(moved, target));

        bool trashCalled = false;
        Hooks h;
        h.moveToTrash = [&](const QString &, QString *) { trashCalled = true; return true; };
        const RemoveOutcome o = remove({m}, env(), h).first();
        QVERIFY(!trashCalled);
        QCOMPARE(o.status, RemoveOutcome::Status::Missing);
        QVERIFY(QFileInfo::exists(moved + "/weights.safetensors"));
    }
};

QTEST_MAIN(TestAiModelStoreTool)
#include "test_ai_model_store_tool.moc"
