// Portions adapted from jithin-sabu/purge-app (MIT), 291efb552256bab08c80109230b444eaeae5bd17
#include "ai_model_store_tool.h"

#include "lifecycle_audit_log.h"
#include "lifecycle_deny_list.h"

#include <QCoreApplication>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

#include <sys/stat.h>

#include <algorithm>

namespace AiModelStore {

namespace {

QString tr(const char *text, int n = -1)
{
    return QCoreApplication::translate("AiModelStore", text, nullptr, n);
}

struct FileStat {
    bool ok = false;
    bool isRegular = false;
    quint64 size = 0;
    QString key;
    QDateTime touched;
};

// lstat, so a symlink is reported as a link and never as the file behind it.
FileStat statFile(const QString &path)
{
    FileStat out;
    struct stat st;
    if (::lstat(QFile::encodeName(path).constData(), &st) != 0)
        return out;
    out.ok = true;
    out.isRegular = S_ISREG(st.st_mode);
    out.size = static_cast<quint64>(st.st_size);
    out.key = QStringLiteral("%1:%2").arg(static_cast<qulonglong>(st.st_dev))
                                     .arg(static_cast<qulonglong>(st.st_ino));
#ifdef Q_OS_MACOS
    const qint64 atime = st.st_atimespec.tv_sec;
    const qint64 mtime = st.st_mtimespec.tv_sec;
#else
    const qint64 atime = st.st_atim.tv_sec;
    const qint64 mtime = st.st_mtim.tv_sec;
#endif
    out.touched = QDateTime::fromSecsSinceEpoch(qMax(atime, mtime));
    return out;
}

// Adds every regular file under `dir` to `model`, counting a file reachable
// by two hard links once. Symlinks are neither followed nor counted: the Hub
// cache's snapshot links point into blobs that are counted directly.
void addFilesUnder(Model *model, const QString &dir, QSet<QString> *seenKeys)
{
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const FileStat st = statFile(path);
        if (!st.ok || !st.isRegular || seenKeys->contains(st.key))
            continue;
        seenKeys->insert(st.key);
        Blob blob;
        blob.path = path;
        blob.size = st.size;
        blob.key = st.key;
        model->blobs.append(blob);
        if (st.touched > model->lastUsed)
            model->lastUsed = st.touched;
    }
}

bool isSameOrInside(const QString &parent, const QString &path)
{
    return path == parent || path.startsWith(parent + QLatin1Char('/'));
}

QString expandHome(const QString &value, const QString &home)
{
    if (value == QLatin1String("~"))
        return home;
    if (value.startsWith(QLatin1String("~/")))
        return QDir::cleanPath(home + value.mid(1));
    return QDir::cleanPath(value);
}

QString homePath(const Environment &env)
{
    return env.home.isEmpty() ? QDir::homePath() : env.home;
}

// ── Ollama ───────────────────────────────────────────────────────────────────

// Ollama honours OLLAMA_MODELS for a store moved to another drive. A value
// naming "/", the home folder or anything above it is ignored: no store lives
// there, and trusting it would put far more than models in reach.
QString ollamaRoot(const Environment &env)
{
    const QString home = homePath(env);
    const QString override = env.vars.value(QStringLiteral("OLLAMA_MODELS"));
    if (!override.isEmpty()) {
        const QString root = expandHome(override, home);
        if (QDir::isAbsolutePath(root) && root != QLatin1String("/") && !isSameOrInside(root, home))
            return root;
    }
    return QDir::cleanPath(home + QStringLiteral("/.ollama/models"));
}

QStringList ollamaRoots(const Environment &env)
{
    QStringList roots{ollamaRoot(env)};
#ifdef Q_OS_LINUX
    // The Linux install script runs Ollama as its own user and keeps the
    // store there.
    const QString service = QStringLiteral("/usr/share/ollama/.ollama/models");
    if (!roots.contains(service))
        roots.append(service);
#endif
    return roots;
}

// The only digest spelling Ollama writes. Anything else in a manifest is not
// turned into a path.
bool isValidDigest(const QString &digest)
{
    static const QRegularExpression rx(QStringLiteral("^sha256[:-][0-9a-f]{16,128}$"));
    return rx.match(digest).hasMatch();
}

// manifests/registry.ollama.ai/library/gemma3/latest reads back as
// "gemma3:latest", the name a user types to pull or remove it. A model from
// another registry keeps its host ("hf.co/user/repo:tag"), because
// `ollama rm` needs it.
QString ollamaModelName(const QString &relativeManifestPath)
{
    const QStringList parts = relativeManifestPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return parts.isEmpty() ? QString() : parts.last();

    const QString host = parts.first();
    const QString tag = parts.last();
    QStringList name = parts.mid(1, parts.size() - 2);
    if (host == QLatin1String("registry.ollama.ai")) {
        if (name.size() > 1 && name.first() == QLatin1String("library"))
            name.removeFirst();
    } else {
        name.prepend(host);
    }
    return QStringLiteral("%1:%2").arg(name.join(QLatin1Char('/')), tag);
}

void scanOllama(const QString &root, QList<Model> *out)
{
    const QString manifestRoot = root + QStringLiteral("/manifests");
    const QString blobRoot = root + QStringLiteral("/blobs");
    if (!QFileInfo(manifestRoot).isDir())
        return;

    QDirIterator it(manifestRoot, QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString manifestPath = it.next();
        if (QFileInfo(manifestPath).fileName() == QLatin1String(".DS_Store"))
            continue;

        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject manifest = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        if (!manifest.contains(QStringLiteral("layers")))
            continue;

        QSet<QString> digests;
        const QJsonValue config = manifest.value(QStringLiteral("config"));
        if (config.isObject())
            digests.insert(config.toObject().value(QStringLiteral("digest")).toString());
        const QJsonArray layers = manifest.value(QStringLiteral("layers")).toArray();
        for (const QJsonValue &layer : layers)
            digests.insert(layer.toObject().value(QStringLiteral("digest")).toString());

        Model model;
        model.tool = Tool::Ollama;
        model.storeRoot = root;
        model.name = ollamaModelName(QDir(manifestRoot).relativeFilePath(manifestPath));
        model.id = QStringLiteral("ollama:") + model.name;
        model.topLevelPaths = {manifestPath};

        QSet<QString> seen;
        const FileStat manifestStat = statFile(manifestPath);
        if (manifestStat.ok) {
            // The manifest is tiny but must go with the weights, or Ollama
            // keeps listing a model that no longer loads.
            Blob blob;
            blob.path = manifestPath;
            blob.size = manifestStat.size;
            blob.key = manifestStat.key;
            model.blobs.append(blob);
            seen.insert(manifestStat.key);
            model.lastUsed = manifestStat.touched;
        }

        for (const QString &digest : std::as_const(digests)) {
            if (!isValidDigest(digest))
                continue;
            QString fileName = digest;
            fileName.replace(QLatin1Char(':'), QLatin1Char('-'));
            const QString blobPath = blobRoot + QLatin1Char('/') + fileName;
            const FileStat st = statFile(blobPath);
            if (!st.ok || !st.isRegular || seen.contains(st.key))
                continue;
            seen.insert(st.key);
            Blob blob;
            blob.path = blobPath;
            blob.size = st.size;
            blob.key = st.key;
            model.blobs.append(blob);
            if (st.touched > model.lastUsed)
                model.lastUsed = st.touched;
        }
        out->append(model);
    }
}

// ── Hugging Face hub cache ───────────────────────────────────────────────────

// HF_HUB_CACHE names the hub folder itself, HF_HOME the folder it sits in,
// and huggingface_hub reads them in that order before falling back to
// $XDG_CACHE_HOME/huggingface/hub.
QString huggingFaceRoot(const Environment &env)
{
    const QString home = homePath(env);
    const QString hubCache = env.vars.value(QStringLiteral("HF_HUB_CACHE"));
    if (!hubCache.isEmpty())
        return expandHome(hubCache, home);
    const QString hfHome = env.vars.value(QStringLiteral("HF_HOME"));
    if (!hfHome.isEmpty())
        return expandHome(hfHome, home) + QStringLiteral("/hub");
    const QString xdg = env.vars.value(QStringLiteral("XDG_CACHE_HOME"));
    if (!xdg.isEmpty())
        return expandHome(xdg, home) + QStringLiteral("/huggingface/hub");
    return QDir::cleanPath(home + QStringLiteral("/.cache/huggingface/hub"));
}

// A model is `models--<org>--<name>/` holding blobs/, snapshots/<commit>/
// (symlinks into blobs/) and refs/. The folder is the unit: its blobs belong
// to that repo alone, and trashing it never takes a blob another model needs.
// Datasets and spaces in the same cache are not models and are left alone.
void scanHuggingFace(const QString &root, QList<Model> *out)
{
    const QDir hub(root);
    const QStringList dirs = hub.entryList({QStringLiteral("models--*")},
                                           QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden,
                                           QDir::Name);
    for (const QString &dirName : dirs) {
        QString name = dirName.mid(QStringLiteral("models--").size());
        name.replace(QStringLiteral("--"), QStringLiteral("/"));
        if (name.isEmpty())
            continue;

        Model model;
        model.tool = Tool::HuggingFace;
        model.storeRoot = root;
        model.name = name;
        model.id = QStringLiteral("huggingface:") + name;
        const QString dirPath = hub.filePath(dirName);
        model.topLevelPaths = {dirPath};
        QSet<QString> seen;
        addFilesUnder(&model, dirPath, &seen);
        if (!model.blobs.isEmpty())
            out->append(model);
    }
}

// ── LM Studio ────────────────────────────────────────────────────────────────

bool isWeightFile(const QString &fileName)
{
    static const QSet<QString> kSuffixes = {
        QStringLiteral("gguf"), QStringLiteral("safetensors"), QStringLiteral("bin"),
        QStringLiteral("pt"),   QStringLiteral("pth"),         QStringLiteral("onnx"),
    };
    return kSuffixes.contains(QFileInfo(fileName).suffix().toLower());
}

// LM Studio files models as <publisher>/<model>/<weights>. The model is the
// first folder that holds a weight file directly.
void findWeightFolders(const QString &dir, int depth, QStringList *found)
{
    const QDir d(dir);
    const QStringList files = d.entryList(QDir::Files | QDir::NoSymLinks);
    for (const QString &file : files) {
        if (isWeightFile(file)) {
            found->append(dir);
            return;
        }
    }
    if (depth >= 4)
        return;
    const QStringList subdirs = d.entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for (const QString &sub : subdirs)
        findWeightFolders(d.filePath(sub), depth + 1, found);
}

void scanLmStudio(const QString &root, QList<Model> *out)
{
    const QDir rootDir(root);
    QStringList folders;
    const QStringList top = rootDir.entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for (const QString &sub : top)
        findWeightFolders(rootDir.filePath(sub), 1, &folders);

    for (const QString &folder : std::as_const(folders)) {
        Model model;
        model.tool = Tool::LmStudio;
        model.storeRoot = root;
        model.name = rootDir.relativeFilePath(folder);
        model.id = QStringLiteral("lmstudio:") + model.name;
        model.topLevelPaths = {folder};
        QSet<QString> seen;
        addFilesUnder(&model, folder, &seen);
        if (!model.blobs.isEmpty())
            out->append(model);
    }
}

// ── llama.cpp ────────────────────────────────────────────────────────────────

// llama.cpp's download cache is one flat folder of <repo>_<file>.gguf files
// with .etag / .json sidecars named after them. A split model's shards
// (-00001-of-00003.gguf) are one model.
void scanLlamaCpp(const QString &root, QList<Model> *out)
{
    static const QRegularExpression shardRx(QStringLiteral("^(.*)-\\d{5}-of-\\d{5}$"));
    const QDir dir(root);
    const QStringList files = dir.entryList(QDir::Files | QDir::NoSymLinks | QDir::Hidden, QDir::Name);

    QMap<QString, QStringList> groups; // model stem -> weight file names
    for (const QString &file : files) {
        if (!file.endsWith(QLatin1String(".gguf"), Qt::CaseInsensitive))
            continue;
        QString stem = file.chopped(5);
        const QRegularExpressionMatch m = shardRx.match(stem);
        if (m.hasMatch())
            stem = m.captured(1);
        groups[stem].append(file);
    }

    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        Model model;
        model.tool = Tool::LlamaCpp;
        model.storeRoot = root;
        model.name = it.key();
        model.id = QStringLiteral("llamacpp:") + model.name;

        QStringList names = it.value();
        for (const QString &weight : it.value()) {
            for (const QString &file : files) {
                if (file != weight && file.startsWith(weight + QLatin1Char('.')))
                    names.append(file);
            }
        }
        QSet<QString> seen;
        for (const QString &fileName : std::as_const(names)) {
            const QString path = dir.filePath(fileName);
            const FileStat st = statFile(path);
            if (!st.ok || !st.isRegular || seen.contains(st.key))
                continue;
            seen.insert(st.key);
            model.topLevelPaths.append(path);
            Blob blob;
            blob.path = path;
            blob.size = st.size;
            blob.key = st.key;
            model.blobs.append(blob);
            if (st.touched > model.lastUsed)
                model.lastUsed = st.touched;
        }
        if (!model.blobs.isEmpty())
            out->append(model);
    }
}

QStringList llamaCppRoots(const Environment &env)
{
    const QString home = homePath(env);
    const QString override = env.vars.value(QStringLiteral("LLAMA_CACHE"));
    if (!override.isEmpty())
        return {expandHome(override, home)};
    const QString xdg = env.vars.value(QStringLiteral("XDG_CACHE_HOME"));
    const QString linuxRoot = (xdg.isEmpty() ? home + QStringLiteral("/.cache") : expandHome(xdg, home))
                              + QStringLiteral("/llama.cpp");
    return {linuxRoot, QDir::cleanPath(home + QStringLiteral("/Library/Caches/llama.cpp"))};
}

// ── Removal ──────────────────────────────────────────────────────────────────

QString findOllamaBinary(const Environment &env)
{
    if (!env.ollamaBinary.isEmpty())
        return env.ollamaBinary;
    if (!env.lookForOllama)
        return QString();
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("ollama"));
    if (!onPath.isEmpty())
        return onPath;
    const QStringList candidates = {
        QStringLiteral("/usr/local/bin/ollama"),
        QStringLiteral("/opt/homebrew/bin/ollama"),
        QStringLiteral("/Applications/Ollama.app/Contents/Resources/ollama"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QString();
}

// A path may be removed only when it lies strictly inside the store it was
// found in, is not itself a symlink, and passes the same deny-list as every
// other Nexis delete.
bool mayRemove(const Model &model, const QString &path)
{
    if (QFileInfo(path).isSymLink())
        return false;
    const QString canonical = QFileInfo(path).canonicalFilePath();
    const QString canonicalRoot = QFileInfo(model.storeRoot).canonicalFilePath();
    if (canonical.isEmpty() || canonicalRoot.isEmpty() || canonical == canonicalRoot
        || !isSameOrInside(canonicalRoot, canonical)) {
        return false;
    }
    return LifecycleDenyList::isSafeForCleaning(path);
}

quint64 sizeOfPath(const Model &model, const QString &path)
{
    quint64 size = 0;
    for (const Blob &blob : model.blobs) {
        if (blob.path == path || blob.path.startsWith(path + QLatin1Char('/')))
            size += blob.size;
    }
    return size;
}

LifecycleAuditLog::Entry auditEntry(const QString &batchId, const Model &model, const QString &path,
                                    LifecycleAuditLog::Action action, quint64 size)
{
    LifecycleAuditLog::Entry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.batchId = batchId;
    entry.originalPath = path;
    entry.canonicalPath = path;
    entry.action = action;
    entry.matchingRuleIds = {QStringLiteral("ai-model:") + toolDisplayName(model.tool)};
    entry.sizeBytes = size;
    entry.nexisVersion = QCoreApplication::applicationVersion();
    entry.runSource = LifecycleAuditLog::RunSource::Interactive;
    entry.category = QStringLiteral("AI models");
    return entry;
}

} // namespace

QString toolDisplayName(Tool tool)
{
    switch (tool) {
    case Tool::Ollama: return QStringLiteral("Ollama");
    case Tool::HuggingFace: return QStringLiteral("Hugging Face");
    case Tool::LmStudio: return QStringLiteral("LM Studio");
    case Tool::LlamaCpp: return QStringLiteral("llama.cpp");
    }
    return QString();
}

quint64 Model::exclusiveBytes() const
{
    quint64 bytes = 0;
    for (const Blob &blob : blobs) {
        if (blob.refCount <= 1)
            bytes += blob.size;
    }
    return bytes;
}

quint64 Model::totalBytes() const
{
    quint64 bytes = 0;
    for (const Blob &blob : blobs)
        bytes += blob.size;
    return bytes;
}

QString Model::removeCommandText() const
{
    return tool == Tool::Ollama ? QStringLiteral("ollama rm %1").arg(name) : QString();
}

quint64 reclaimableBytes(const QList<Model> &selection)
{
    QHash<QString, int> picked;
    QHash<QString, quint64> sizes;
    QHash<QString, int> refs;
    for (const Model &model : selection) {
        QSet<QString> countedInModel;
        for (const Blob &blob : model.blobs) {
            if (countedInModel.contains(blob.key))
                continue;
            countedInModel.insert(blob.key);
            picked[blob.key] += 1;
            sizes[blob.key] = blob.size;
            refs[blob.key] = qMax(refs.value(blob.key), blob.refCount);
        }
    }
    quint64 bytes = 0;
    for (auto it = picked.cbegin(); it != picked.cend(); ++it) {
        if (it.value() >= refs.value(it.key()))
            bytes += sizes.value(it.key());
    }
    return bytes;
}

Environment Environment::current()
{
    Environment env;
    env.home = QDir::homePath();
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    for (const char *key : {"OLLAMA_MODELS", "HF_HUB_CACHE", "HF_HOME", "XDG_CACHE_HOME", "LLAMA_CACHE"}) {
        const QString value = process.value(QString::fromLatin1(key));
        if (!value.isEmpty())
            env.vars.insert(QString::fromLatin1(key), value);
    }
    return env;
}

bool removesWithToolCommand(const Model &model, const Environment &env)
{
    return model.tool == Tool::Ollama && !findOllamaBinary(env).isEmpty();
}

QList<Model> scan(const Environment &env)
{
    QList<Model> models;
    for (const QString &root : ollamaRoots(env))
        scanOllama(root, &models);
    scanHuggingFace(huggingFaceRoot(env), &models);
    for (const QString &root : {homePath(env) + QStringLiteral("/.lmstudio/models"),
                                homePath(env) + QStringLiteral("/.cache/lm-studio/models")})
        scanLmStudio(root, &models);
    for (const QString &root : llamaCppRoots(env))
        scanLlamaCpp(root, &models);

    // A blob's reference count is how many scanned models use it. Only
    // Ollama shares blobs between models; for every other store each key is
    // seen once and stays at 1.
    QHash<QString, int> refs;
    for (const Model &model : std::as_const(models)) {
        for (const Blob &blob : model.blobs)
            refs[blob.key] += 1;
    }
    for (Model &model : models) {
        for (Blob &blob : model.blobs)
            blob.refCount = refs.value(blob.key, 1);
    }

    std::sort(models.begin(), models.end(), [](const Model &a, const Model &b) {
        if (a.tool != b.tool)
            return a.tool < b.tool;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return models;
}

QList<RemoveOutcome> remove(const QList<Model> &selection, const Environment &env, const Hooks &hooksIn)
{
    Hooks hooks = hooksIn;
    if (!hooks.moveToTrash) {
        hooks.moveToTrash = [](const QString &path, QString *trashedPath) {
            return QFile::moveToTrash(path, trashedPath);
        };
    }
    if (!hooks.run) {
        hooks.run = [](const QString &program, const QStringList &args) {
            return CommandUtil::execWithStatus(program, args, 120000);
        };
    }

    QHash<QString, Model> fresh;
    for (const Model &model : scan(env))
        fresh.insert(model.id, model);

    // How many of the picked models use each blob. A blob goes to the Trash
    // only when that equals everyone using it right now.
    QHash<QString, int> pickedRefs;
    for (const Model &picked : selection) {
        const auto it = fresh.constFind(picked.id);
        if (it == fresh.constEnd())
            continue;
        QSet<QString> seen;
        for (const Blob &blob : it->blobs) {
            if (!seen.contains(blob.key)) {
                seen.insert(blob.key);
                pickedRefs[blob.key] += 1;
            }
        }
    }

    const QString batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString ollama = findOllamaBinary(env);
    QSet<QString> handledKeys;
    QList<LifecycleAuditLog::Entry> audit;
    QList<RemoveOutcome> outcomes;

    for (const Model &picked : selection) {
        RemoveOutcome outcome;
        outcome.modelId = picked.id;
        outcome.name = picked.name;

        const auto it = fresh.constFind(picked.id);
        if (it == fresh.constEnd()) {
            outcome.status = RemoveOutcome::Status::Missing;
            outcome.message = tr("Already gone.");
            outcomes.append(outcome);
            continue;
        }
        const Model &model = *it;

        if (model.tool == Tool::Ollama && !ollama.isEmpty()) {
            outcome.usedToolCommand = true;
            const ExecResult result = hooks.run(ollama, {QStringLiteral("rm"), model.name});
            const bool manifestGone = !QFileInfo::exists(model.topLevelPaths.value(0));
            if (!result.ok() || !manifestGone) {
                outcome.status = RemoveOutcome::Status::Failed;
                const QString detail = result.error.isEmpty() ? result.output : result.error;
                outcome.message = result.ok()
                    ? tr("Ollama reported success but the model is still in this store.")
                    : tr("ollama rm failed: %1 Start Ollama and try again.").arg(detail.trimmed());
                outcomes.append(outcome);
                continue;
            }
            for (const Blob &blob : model.blobs) {
                if (!QFileInfo::exists(blob.path) && !handledKeys.contains(blob.key)) {
                    handledKeys.insert(blob.key);
                    outcome.freedBytes += blob.size;
                }
            }
            audit.append(auditEntry(batchId, model, model.topLevelPaths.value(0),
                                    LifecycleAuditLog::Action::PermanentlyDeleted, outcome.freedBytes));
            outcome.status = RemoveOutcome::Status::Removed;
            outcome.message = tr("Removed with ollama rm.");
            outcomes.append(outcome);
            continue;
        }

        // Trash route. The manifest goes first for Ollama: a failure after it
        // leaves orphan blobs, never a listed model with missing weights.
        QStringList paths = model.topLevelPaths;
        if (model.tool == Tool::Ollama) {
            QSet<QString> seen;
            for (const Blob &blob : model.blobs) {
                if (seen.contains(blob.key) || handledKeys.contains(blob.key))
                    continue;
                seen.insert(blob.key);
                if (blob.path != model.topLevelPaths.value(0)
                    && pickedRefs.value(blob.key) >= blob.refCount) {
                    paths.append(blob.path);
                }
            }
        }

        int failed = 0;
        for (const QString &path : std::as_const(paths)) {
            if (!mayRemove(model, path)) {
                ++failed;
                continue;
            }
            const quint64 size = sizeOfPath(model, path);
            QString trashed;
            if (!hooks.moveToTrash(path, &trashed)) {
                ++failed;
                continue;
            }
            LifecycleAuditLog::Entry entry = auditEntry(batchId, model, path,
                                                        LifecycleAuditLog::Action::MovedToTrash, size);
            entry.trashDestination = trashed;
            audit.append(entry);
            outcome.freedBytes += size;
            for (const Blob &blob : model.blobs) {
                if (blob.path == path || blob.path.startsWith(path + QLatin1Char('/')))
                    handledKeys.insert(blob.key);
            }
        }
        if (failed == 0) {
            outcome.status = RemoveOutcome::Status::Removed;
            outcome.message = tr("Moved to the Trash.");
        } else {
            outcome.status = RemoveOutcome::Status::Failed;
            outcome.message = tr("%n item(s) could not be moved to the Trash.", failed);
        }
        outcomes.append(outcome);
    }

    if (!audit.isEmpty())
        LifecycleAuditLog::appendAll(audit);
    return outcomes;
}

} // namespace AiModelStore
