// Portions adapted from jithin-sabu/purge-app (MIT), 291efb552256bab08c80109230b444eaeae5bd17
#ifndef AI_MODEL_STORE_TOOL_H
#define AI_MODEL_STORE_TOOL_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include "Utils/command_util.h"
#include "nexis-core_global.h"

// SSO-27099: finds the AI models other tools have downloaded onto this
// computer (Ollama, the Hugging Face hub cache, LM Studio, llama.cpp) and
// reports one row per model with the bytes removing it would really free.
//
// These stores are the biggest single consumers of disk on a machine that
// runs local models, and the raw files are meaningless to a user: Ollama names
// its weights by digest and shares them between models, the Hub cache is a
// tree of hash-named blobs behind symlinks. Listing models, not files, is the
// point.
//
// Removal is only ever an explicit user pick. Nothing in this namespace is
// reachable from the System Cleaner's scan, a cleaning schedule or the
// headless runner, and there is deliberately no "remove everything" entry
// point: remove() takes the exact models the user ticked.
namespace AiModelStore {

enum class Tool {
    Ollama,
    HuggingFace,
    LmStudio,
    LlamaCpp,
};

// Product name, not translated.
NEXISCORESHARED_EXPORT QString toolDisplayName(Tool tool);

// One file on disk that belongs to a model.
struct NEXISCORESHARED_EXPORT Blob {
    QString path;
    quint64 size = 0;
    // Identity of the bytes (device:inode). Two models whose blobs share a key
    // share the storage, so it is freed only once every one of them is gone.
    QString key;
    // How many scanned models reference this blob.
    int refCount = 1;
};

struct NEXISCORESHARED_EXPORT Model {
    Tool tool = Tool::Ollama;
    QString id;        // stable across scans, e.g. "ollama:gemma3:latest"
    QString name;      // what the owning tool calls it
    QString storeRoot; // the store directory the model was found in
    // Ollama: the manifest. Everything else: the folder or files that go to
    // the Trash as a whole.
    QStringList topLevelPaths;
    QList<Blob> blobs;
    QDateTime lastUsed;

    // Bytes freed by removing this model alone; a blob another model still
    // references does not count.
    quint64 exclusiveBytes() const;
    // Everything the model reads from, shared blobs included — what a later
    // re-download costs.
    quint64 totalBytes() const;
    // Blobs this model shares with at least one other model.
    quint64 sharedBytes() const { return totalBytes() - exclusiveBytes(); }
    // The tool's own removal command for display, or empty when there is none
    // and removal means moving files to the Trash.
    QString removeCommandText() const;
};

// Bytes a removal of exactly `selection` frees: each blob is counted once, and
// only when every model that references it is part of the selection.
NEXISCORESHARED_EXPORT quint64 reclaimableBytes(const QList<Model> &selection);

// Where the stores live. Tests build one over a temporary home.
struct NEXISCORESHARED_EXPORT Environment {
    QString home;
    // OLLAMA_MODELS, HF_HUB_CACHE, HF_HOME, XDG_CACHE_HOME, LLAMA_CACHE.
    QHash<QString, QString> vars;
    // Path of the `ollama` executable. Empty means look for it, unless
    // lookForOllama is false (tests that must not find a real install).
    QString ollamaBinary;
    bool lookForOllama = true;

    static Environment current();
};

// True when removing `model` runs the owning tool's own command (`ollama rm`)
// rather than moving files to the Trash.
NEXISCORESHARED_EXPORT bool removesWithToolCommand(const Model &model, const Environment &env);

// Every model found, ordered by tool and then name. Reads the filesystem only;
// nothing is changed.
NEXISCORESHARED_EXPORT QList<Model> scan(const Environment &env);

struct NEXISCORESHARED_EXPORT RemoveOutcome {
    enum class Status {
        Removed,
        Missing, // already gone since the scan
        Failed,
    };
    QString modelId;
    QString name;
    Status status = Status::Failed;
    bool usedToolCommand = false;
    quint64 freedBytes = 0;
    QString message; // user-facing, translated
};

// Test seams; production moves to the Trash with QFile::moveToTrash and runs
// the tool with CommandUtil.
struct NEXISCORESHARED_EXPORT Hooks {
    std::function<bool(const QString &path, QString *trashedPath)> moveToTrash;
    std::function<ExecResult(const QString &program, const QStringList &args)> run;
};

// Removes the picked models, rescanning first so a blob another model started
// to share since the list was shown is kept. Uses the tool's own command where
// it has one (`ollama rm`), and moves everything else to the Trash. Every
// removal goes through LifecycleDenyList::isSafeForCleaning and is recorded in
// the lifecycle audit log, so it appears under System Cleaner → History.
NEXISCORESHARED_EXPORT QList<RemoveOutcome> remove(const QList<Model> &selection,
                                                   const Environment &env,
                                                   const Hooks &hooks = Hooks());

} // namespace AiModelStore

#endif // AI_MODEL_STORE_TOOL_H
