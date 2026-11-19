#include "duplicate_finder_service.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSet>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <functional>

#include <sys/stat.h>
#include <unistd.h>

DuplicateFinderService *DuplicateFinderService::instance = nullptr;

DuplicateFinderService *DuplicateFinderService::ins()
{
    if (!instance)
        instance = new DuplicateFinderService;
    return instance;
}

DuplicateFinderService::DuplicateFinderService(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QList<DuplicateGroup>>("QList<DuplicateGroup>");
    qRegisterMetaType<QList<LargeFileEntry>>("QList<LargeFileEntry>");
    qRegisterMetaType<DuplicateGroup>("DuplicateGroup");
    qRegisterMetaType<LargeFileEntry>("LargeFileEntry");
    qRegisterMetaType<LinkReplaceResult>("LinkReplaceResult");
    qRegisterMetaType<QList<LinkReplaceResult>>("QList<LinkReplaceResult>");
}

DuplicateFinderService::~DuplicateFinderService()
{
    // FW-08: prevent the worker lambda from outliving the service in tests
    // (the production path is a singleton, so this only matters when a
    // TestableDuplicateFinderService instance goes out of scope between
    // an emit and the QtConcurrent::run lambda's return).
    cancel();
    mWorkerFuture.waitForFinished();
}

bool DuplicateFinderService::isScanning() const
{
    return mWorkerFuture.isRunning();
}

void DuplicateFinderService::cancel()
{
    bool wasPaused = false;
    {
        // Under the lock so a worker between "saw paused" and wait() cannot
        // miss the wake-up.
        QMutexLocker locker(&mPauseMutex);
        mCancelled.storeRelaxed(1);
        wasPaused = mPaused.fetchAndStoreOrdered(0) != 0;
        mPauseCondition.wakeAll();
    }
    if (wasPaused)
        emit pausedChanged(false);
}

void DuplicateFinderService::pause()
{
    if (mPaused.fetchAndStoreOrdered(1) == 0)
        emit pausedChanged(true);
}

void DuplicateFinderService::resume()
{
    bool wasPaused = false;
    {
        QMutexLocker locker(&mPauseMutex);
        wasPaused = mPaused.fetchAndStoreOrdered(0) != 0;
        mPauseCondition.wakeAll();
    }
    if (wasPaused)
        emit pausedChanged(false);
}

bool DuplicateFinderService::isPaused() const
{
    return mPaused.loadAcquire() != 0;
}

bool DuplicateFinderService::shouldStop()
{
    if (mPaused.loadAcquire()) {
        QMutexLocker locker(&mPauseMutex);
        while (mPaused.loadAcquire() && !mCancelled.loadRelaxed())
            mPauseCondition.wait(&mPauseMutex);
    }
    return mCancelled.loadRelaxed();
}

QList<CleanerService::ExclusionEntry> DuplicateFinderService::loadExclusions() const
{
    return CleanerService::ins()->loadExclusions();
}

bool DuplicateFinderService::moveToTrash(const QString &path)
{
    return QFile::moveToTrash(path);
}

void DuplicateFinderService::scan(const QStringList &directories,
                                   qint64 minSize,
                                   const QString &globFilter)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    mPaused.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    auto job = [this, directories, minSize, globFilter, exclusions]() {
        QList<DuplicateGroup> results =
            runPipeline(directories, minSize, globFilter, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit scanFinished(results);
    };

    if (runsAsynchronously())
        mWorkerFuture = QtConcurrent::run(job);
    else
        job();
}

void DuplicateFinderService::scanSimilarImages(const QStringList &directories,
                                                qint64 minSize,
                                                int maxDistance)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    mPaused.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    auto job = [this, directories, minSize, maxDistance, exclusions]() {
        QList<DuplicateGroup> results =
            runSimilarPipeline(directories, minSize, maxDistance, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit scanFinished(results);
    };

    if (runsAsynchronously())
        mWorkerFuture = QtConcurrent::run(job);
    else
        job();
}

void DuplicateFinderService::scanDuplicateDirectories(const QStringList &directories,
                                                       qint64 minSize)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    mPaused.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    auto job = [this, directories, minSize, exclusions]() {
        QList<DuplicateGroup> results =
            runDirectoryPipeline(directories, minSize, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit scanFinished(results);
    };

    if (runsAsynchronously())
        mWorkerFuture = QtConcurrent::run(job);
    else
        job();
}

void DuplicateFinderService::scanLargest(const QStringList &directories, int topN)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    mPaused.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    auto job = [this, directories, topN, exclusions]() {
        QList<LargeFileEntry> results =
            runLargestPipeline(directories, topN, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit largestScanFinished(results);
    };

    if (runsAsynchronously())
        mWorkerFuture = QtConcurrent::run(job);
    else
        job();
}

void DuplicateFinderService::scanEmptyFolders(const QStringList &directories)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    mPaused.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    auto job = [this, directories, exclusions]() {
        QStringList results = runEmptyFoldersPipeline(directories, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit emptyFoldersScanFinished(results);
    };

    if (runsAsynchronously())
        mWorkerFuture = QtConcurrent::run(job);
    else
        job();
}

QStringList DuplicateFinderService::trashFiles(
    const QStringList &paths,
    const QList<DuplicateGroup> &knownGroups)
{
    const QList<CleanerService::ExclusionEntry> exclusions = loadExclusions();

    QStringList allowed;
    allowed.reserve(paths.size());
    for (const QString &p : paths) {
        if (CleanerService::isExcluded(p, exclusions))
            continue;
        // SSO-27146: trashing a whole folder must not take a protected
        // entry inside it along.
        const bool holdsExcluded = std::any_of(exclusions.begin(), exclusions.end(),
            [&p](const CleanerService::ExclusionEntry &e) {
                return e.path.startsWith(p + '/');
            });
        if (holdsExcluded)
            continue;
        allowed.append(p);
    }

    const QStringList safe = filterSafeTrashCandidates(allowed, knownGroups);

    QStringList trashed;
    for (const QString &p : safe) {
        if (moveToTrash(p))
            trashed.append(p);
    }
    return trashed;
}

QStringList DuplicateFinderService::filterSafeTrashCandidates(
    const QStringList &paths,
    const QList<DuplicateGroup> &knownGroups)
{
    // Build a path → group-index map so we can drop entries that would
    // empty a group. A file that isn't in any known group is always safe
    // (the never-delete-last-copy rule applies to duplicate groups only).
    QHash<QString, int> pathToGroup;
    for (int i = 0; i < knownGroups.size(); ++i) {
        for (const QFileInfo &fi : knownGroups[i].files)
            pathToGroup.insert(fi.absoluteFilePath(), i);
    }

    const QSet<QString> requested(paths.constBegin(), paths.constEnd());

    // Count how many of each group's members are slated for removal.
    QHash<int, int> removalsByGroup;
    for (const QString &p : requested) {
        auto it = pathToGroup.constFind(p);
        if (it != pathToGroup.constEnd())
            removalsByGroup[it.value()] += 1;
    }

    QSet<int> overdrawn;
    for (auto it = removalsByGroup.constBegin();
         it != removalsByGroup.constEnd(); ++it) {
        const int groupIdx = it.key();
        const int totalInGroup = knownGroups[groupIdx].files.size();
        if (it.value() >= totalInGroup)
            overdrawn.insert(groupIdx);
    }

    if (overdrawn.isEmpty())
        return paths;

    // For each overdrawn group, keep at least one member alive. Pick the
    // candidate to retain deterministically (smallest path) so the choice
    // is reproducible across runs and matches the test expectations.
    QSet<QString> mustKeep;
    for (int groupIdx : overdrawn) {
        QStringList groupRequested;
        for (const QFileInfo &fi : knownGroups[groupIdx].files) {
            const QString p = fi.absoluteFilePath();
            if (requested.contains(p))
                groupRequested.append(p);
        }
        std::sort(groupRequested.begin(), groupRequested.end());
        if (!groupRequested.isEmpty())
            mustKeep.insert(groupRequested.first());
    }

    QStringList safe;
    safe.reserve(paths.size());
    for (const QString &p : paths) {
        if (!mustKeep.contains(p))
            safe.append(p);
    }
    return safe;
}

bool DuplicateFinderService::wouldRemoveLastCopy(
    const QStringList &toRemove,
    const QList<DuplicateGroup> &knownGroups)
{
    const QSet<QString> requested(toRemove.constBegin(), toRemove.constEnd());
    for (const DuplicateGroup &group : knownGroups) {
        int kept = 0;
        for (const QFileInfo &fi : group.files) {
            if (!requested.contains(fi.absoluteFilePath()))
                ++kept;
        }
        if (kept == 0 && !group.files.isEmpty())
            return true;
    }
    return false;
}

QList<LargeFileEntry> DuplicateFinderService::rankLargest(
    QList<LargeFileEntry> candidates, int topN)
{
    std::sort(candidates.begin(), candidates.end(),
              [](const LargeFileEntry &a, const LargeFileEntry &b) {
        if (a.size != b.size)
            return a.size > b.size;
        return a.info.absoluteFilePath() < b.info.absoluteFilePath();
    });

    if (topN > 0 && candidates.size() > topN)
        candidates = candidates.mid(0, topN);
    return candidates;
}

static QByteArray hashFilePartial(const QString &path, qint64 bytes = 4096)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = f.read(bytes);
    f.close();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

static QByteArray hashFileFull(const QString &path, const std::function<bool()> &shouldStop)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    const qint64 chunkSize = 65536;
    while (!f.atEnd()) {
        if (shouldStop && shouldStop())
            return {};
        hasher.addData(f.read(chunkSize));
    }
    f.close();
    return hasher.result();
}

namespace {

struct FileId {
    quint64 dev = 0;
    quint64 ino = 0;
    bool ok = false;
};

// Follows symlinks (stat), so a path and its link target compare equal.
FileId statId(const QString &path)
{
    struct stat st;
    if (::stat(QFile::encodeName(path).constData(), &st) != 0)
        return {};
    return {static_cast<quint64>(st.st_dev), static_cast<quint64>(st.st_ino), true};
}

bool isRegularFile(const QString &path)
{
    const QFileInfo fi(path);
    return fi.isFile() && !fi.isSymLink();
}

// Two names of one inode are one physical file, not a reclaimable copy.
// Returns true when the file's inode was already collected.
bool alreadyCollectedInode(const QString &path, QSet<std::pair<quint64, quint64>> &seen)
{
    const FileId id = statId(path);
    if (!id.ok)
        return false;
    const std::pair<quint64, quint64> key{id.dev, id.ino};
    if (seen.contains(key))
        return true;
    seen.insert(key);
    return false;
}

} // namespace

bool DuplicateFinderService::onSameFilesystem(const QString &pathA, const QString &pathB)
{
    const FileId a = statId(pathA);
    const FileId b = statId(pathB);
    return a.ok && b.ok && a.dev == b.dev;
}

bool DuplicateFinderService::isSameFile(const QString &pathA, const QString &pathB)
{
    const FileId a = statId(pathA);
    const FileId b = statId(pathB);
    return a.ok && b.ok && a.dev == b.dev && a.ino == b.ino;
}

QList<LinkReplaceEntry> DuplicateFinderService::buildLinkReplacePlan(
    const QStringList &selected,
    const QList<DuplicateGroup> &knownGroups,
    LinkType type,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    using Status = LinkReplaceEntry::Status;

    QSet<QString> inExactGroup;
    for (const DuplicateGroup &g : knownGroups) {
        if (g.kind != DuplicateGroup::Kind::Exact)
            continue;
        for (const QFileInfo &fi : g.files)
            inExactGroup.insert(fi.absoluteFilePath());
    }

    QList<LinkReplaceEntry> plan;
    const QSet<QString> requested(selected.constBegin(), selected.constEnd());

    QSet<QString> reported;
    for (const QString &p : selected) {
        if (inExactGroup.contains(p) || reported.contains(p))
            continue;
        reported.insert(p);
        LinkReplaceEntry e;
        e.duplicatePath = p;
        e.type = type;
        e.status = Status::SkipNotInGroup;
        plan.append(e);
    }

    for (const DuplicateGroup &g : knownGroups) {
        if (g.kind != DuplicateGroup::Kind::Exact)
            continue;

        QStringList members;
        for (const QFileInfo &fi : g.files)
            members.append(fi.absoluteFilePath());

        QStringList chosen;
        for (const QString &m : members) {
            if (requested.contains(m))
                chosen.append(m);
        }
        if (chosen.isEmpty())
            continue;

        QString keeper;
        for (const QString &m : members) {
            if (!requested.contains(m)) {
                keeper = m;
                break;
            }
        }
        const bool keeperWasSelected = keeper.isEmpty();
        if (keeperWasSelected)
            keeper = *std::min_element(chosen.begin(), chosen.end());

        const bool keeperExcluded = CleanerService::isExcluded(keeper, exclusions);
        const bool keeperUsable = isRegularFile(keeper);

        for (const QString &m : chosen) {
            LinkReplaceEntry e;
            e.duplicatePath = m;
            e.keeperPath = keeper;
            e.type = type;
            e.size = g.fileSize;

            if (m == keeper)
                e.status = Status::KeptAsOriginal;
            else if (keeperExcluded || CleanerService::isExcluded(m, exclusions))
                e.status = Status::SkipExcluded;
            else if (!keeperUsable || !isRegularFile(m))
                e.status = Status::SkipMissing;
            else if (isSameFile(m, keeper))
                e.status = Status::SkipAlreadyLinked;
            else if (!onSameFilesystem(m, keeper)
                     || !onSameFilesystem(QFileInfo(m).absolutePath(), keeper))
                e.status = Status::SkipCrossFilesystem;
            else
                e.status = Status::Ready;
            plan.append(e);
        }
    }

    return plan;
}

QList<LinkReplaceEntry> DuplicateFinderService::previewLinkReplace(
    const QStringList &selected,
    const QList<DuplicateGroup> &knownGroups,
    LinkType type) const
{
    return buildLinkReplacePlan(selected, knownGroups, type, loadExclusions());
}

LinkReplaceResult DuplicateFinderService::applyLinkReplace(
    const LinkReplaceEntry &entry,
    const std::function<bool()> &shouldStop)
{
    LinkReplaceResult result;
    result.path = entry.duplicatePath;
    result.keeperPath = entry.keeperPath;

    auto fail = [&result](const QString &why) {
        result.error = why;
        return result;
    };

    if (!entry.isReady())
        return fail(tr("Not eligible for link replacement"));

    // Everything the plan checked is re-checked: the preview may be minutes
    // old and either file can have changed since the scan.
    if (!isRegularFile(entry.duplicatePath) || !isRegularFile(entry.keeperPath))
        return fail(tr("File no longer exists or is not a regular file"));
    if (isSameFile(entry.duplicatePath, entry.keeperPath))
        return fail(tr("Already the same file"));

    const QFileInfo dup(entry.duplicatePath);
    const QFileInfo keeper(entry.keeperPath);
    const QString dupDir = dup.absolutePath();
    if (!onSameFilesystem(entry.duplicatePath, entry.keeperPath)
        || !onSameFilesystem(dupDir, entry.keeperPath))
        return fail(tr("Keeper is on a different filesystem"));
    if (dup.size() != keeper.size())
        return fail(tr("Files differ — changed since the scan"));

    const QByteArray dupHash = hashFileFull(entry.duplicatePath, shouldStop);
    const QByteArray keeperHash = hashFileFull(entry.keeperPath, shouldStop);
    if (dupHash.isEmpty() || keeperHash.isEmpty())
        return fail(tr("Could not read the files"));
    if (dupHash != keeperHash)
        return fail(tr("Files differ — changed since the scan"));

    QString target = entry.keeperPath;
    if (entry.type == LinkType::Symbolic) {
        // Relative, so the link survives moving or remounting the tree.
        target = QDir(dup.dir().canonicalPath()).relativeFilePath(keeper.canonicalFilePath());
    }

    // Link under a temporary name, then rename over the duplicate: rename()
    // is atomic, so the path always resolves to a file and any failure
    // leaves the original duplicate exactly as it was.
    QString tmpPath;
    for (int attempt = 0; attempt < 8; ++attempt) {
        tmpPath = dupDir + QStringLiteral("/.nexis-link-")
            + QString::number(QRandomGenerator::global()->generate(), 16);
        const QByteArray tmp = QFile::encodeName(tmpPath);
        const int rc = (entry.type == LinkType::Hard)
            ? ::link(QFile::encodeName(entry.keeperPath).constData(), tmp.constData())
            : ::symlink(QFile::encodeName(target).constData(), tmp.constData());
        if (rc == 0)
            break;
        if (errno != EEXIST || attempt == 7)
            return fail(QString::fromLocal8Bit(std::strerror(errno)));
    }

    if (::rename(QFile::encodeName(tmpPath).constData(),
                 QFile::encodeName(entry.duplicatePath).constData()) != 0) {
        const QString why = QString::fromLocal8Bit(std::strerror(errno));
        ::unlink(QFile::encodeName(tmpPath).constData());
        return fail(why);
    }

    result.replaced = true;
    return result;
}

void DuplicateFinderService::replaceWithLinks(const QList<LinkReplaceEntry> &plan)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    mPaused.storeRelaxed(0);

    auto job = [this, plan]() {
        int total = 0;
        for (const LinkReplaceEntry &e : plan) {
            if (e.isReady())
                ++total;
        }

        QList<LinkReplaceResult> results;
        for (const LinkReplaceEntry &e : plan) {
            if (!e.isReady())
                continue;
            if (shouldStop())
                break;
            results.append(applyLinkReplace(e, [this] { return shouldStop(); }));
            emit progressUpdated(5, results.size(), total,
                tr("Replacing duplicates with links... %1/%2")
                    .arg(results.size()).arg(total));
        }
        emit linkReplaceFinished(results);
    };

    if (runsAsynchronously())
        mWorkerFuture = QtConcurrent::run(job);
    else
        job();
}

QList<DuplicateGroup> DuplicateFinderService::runPipeline(
    const QStringList &directories,
    qint64 minSize,
    const QString &globFilter,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    // Convert glob to regex if provided
    QRegularExpression globRe;
    if (!globFilter.isEmpty()) {
        QString pattern = QRegularExpression::wildcardToRegularExpression(globFilter);
        globRe.setPattern(pattern);
        globRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }

    // --- Stage 1: Collect files and group by size ---
    QHash<qint64, QList<QFileInfo>> sizeGroups;
    QSet<std::pair<quint64, quint64>> seenInodes;
    int totalScanned = 0;

    for (const QString &dir : directories) {
        if (shouldStop())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (shouldStop())
                return {};

            it.next();
            QFileInfo info = it.fileInfo();

            if (info.isSymLink())
                continue;

            if (info.size() < minSize)
                continue;

            if (globRe.isValid() && !globFilter.isEmpty()) {
                if (!globRe.match(info.fileName()).hasMatch())
                    continue;
            }

            // FW-08: respect the cleaner's exclusion engine so paths the
            // user explicitly protected never show up as deletion fodder.
            if (CleanerService::isExcluded(info.absoluteFilePath(), exclusions))
                continue;

            if (alreadyCollectedInode(info.absoluteFilePath(), seenInodes))
                continue;

            sizeGroups[info.size()].append(info);
            totalScanned++;

            if (totalScanned % 1000 == 0)
                emit progressUpdated(1, totalScanned, 0,
                    tr("Stage 1: Scanning files... %1 found").arg(totalScanned));
        }
    }

    if (shouldStop())
        return {};

    const QList<DuplicateGroup> results =
        groupExactByContent(sizeGroups, totalScanned, /*baseStage=*/2);

    if (shouldStop())
        return {};

    emit progressUpdated(3, results.size(), results.size(),
        tr("Scan complete: %1 duplicate groups found").arg(results.size()));

    return results;
}

QList<DuplicateGroup> DuplicateFinderService::groupExactByContent(
    QHash<qint64, QList<QFileInfo>> sizeGroups,
    int totalScanned,
    int baseStage,
    const QString &phaseLabel)
{
    const QString label = phaseLabel.isEmpty() ? tr("Stage %1").arg(baseStage)
                                                : phaseLabel;

    // Remove size groups with only one file
    QList<qint64> sizesToRemove;
    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it) {
        if (it.value().size() <= 1)
            sizesToRemove.append(it.key());
    }
    for (qint64 s : sizesToRemove)
        sizeGroups.remove(s);

    // Count candidates for the partial-hash pass
    int totalCandidates = 0;
    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it)
        totalCandidates += it.value().size();

    emit progressUpdated(baseStage, totalScanned, totalScanned,
        tr("%1 complete: %2 candidates in %3 size groups")
            .arg(label).arg(totalCandidates).arg(sizeGroups.size()));

    if (shouldStop())
        return {};

    // --- Partial hash (first 4 KB) ---
    QHash<QByteArray, QList<QFileInfo>> partialHashGroups;
    int hashProgress = 0;

    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (shouldStop())
                return {};

            QByteArray partialHash = hashFilePartial(fi.absoluteFilePath());
            if (!partialHash.isEmpty()) {
                QByteArray key = QByteArray::number(fi.size()) + partialHash;
                partialHashGroups[key].append(fi);
            }

            hashProgress++;
            if (hashProgress % 100 == 0)
                emit progressUpdated(baseStage, hashProgress, totalCandidates,
                    tr("%1: Partial hashing... %2/%3")
                        .arg(label).arg(hashProgress).arg(totalCandidates));
        }
    }

    // Remove partial hash groups with only one file
    QList<QByteArray> hashesToRemove;
    for (auto it = partialHashGroups.constBegin();
         it != partialHashGroups.constEnd(); ++it) {
        if (it.value().size() <= 1)
            hashesToRemove.append(it.key());
    }
    for (const QByteArray &h : hashesToRemove)
        partialHashGroups.remove(h);

    int stage3Candidates = 0;
    for (auto it = partialHashGroups.constBegin();
         it != partialHashGroups.constEnd(); ++it)
        stage3Candidates += it.value().size();

    emit progressUpdated(baseStage, totalCandidates, totalCandidates,
        tr("%1 complete: %2 candidates remaining").arg(label).arg(stage3Candidates));

    if (shouldStop())
        return {};

    // --- Full hash ---
    QHash<QByteArray, QList<QFileInfo>> fullHashGroups;
    int fullProgress = 0;

    for (auto it = partialHashGroups.constBegin();
         it != partialHashGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (shouldStop())
                return {};

            QByteArray fullHash = hashFileFull(fi.absoluteFilePath(), [this] { return shouldStop(); });
            if (!fullHash.isEmpty())
                fullHashGroups[fullHash].append(fi);

            fullProgress++;
            if (fullProgress % 10 == 0)
                emit progressUpdated(baseStage + 1, fullProgress, stage3Candidates,
                    tr("%1: Full hashing... %2/%3")
                        .arg(label).arg(fullProgress).arg(stage3Candidates));
        }
    }

    // Build result groups
    QList<DuplicateGroup> results;
    for (auto it = fullHashGroups.constBegin();
         it != fullHashGroups.constEnd(); ++it) {
        if (it.value().size() >= 2) {
            DuplicateGroup group;
            group.files = it.value();
            group.fileSize = group.files.first().size();
            group.hash = it.key();
            group.kind = DuplicateGroup::Kind::Exact;
            results.append(group);
        }
    }

    // Sort by wasted space descending
    std::sort(results.begin(), results.end(),
              [](const DuplicateGroup &a, const DuplicateGroup &b) {
        return a.fileSize * (a.files.size() - 1) >
               b.fileSize * (b.files.size() - 1);
    });

    return results;
}

namespace {

// SSO-26638 (spec D2): a BK-tree keyed by Hamming distance over 64-bit
// image hashes. Hamming distance is a proper metric (satisfies the triangle
// inequality), so the standard BK-tree child-by-exact-distance layout and
// range-query pruning both apply unmodified. Exists to keep clustering
// 50,000 hashes under the spec's 2-second budget — plain O(n^2) pairwise
// comparison does not.
class HammingBkTree
{
public:
    void insert(quint64 hash, int index)
    {
        if (mNodes.isEmpty()) {
            mNodes.append({hash, index, {}});
            return;
        }
        int cur = 0;
        while (true) {
            const int d = ImageHash::hammingDistance(mNodes[cur].hash, hash);
            auto it = mNodes[cur].children.constFind(d);
            if (it == mNodes[cur].children.constEnd()) {
                mNodes[cur].children.insert(d, mNodes.size());
                mNodes.append({hash, index, {}});
                return;
            }
            cur = it.value();
        }
    }

    // Calls `visit(index)` for every inserted point within `maxDistance` of
    // `query`, including a point equal to `query` itself.
    void queryWithin(quint64 query, int maxDistance,
                      const std::function<void(int)> &visit) const
    {
        if (mNodes.isEmpty())
            return;

        QVector<int> stack;
        stack.append(0);
        while (!stack.isEmpty()) {
            const int cur = stack.takeLast();
            const Node &node = mNodes[cur];
            const int d = ImageHash::hammingDistance(node.hash, query);
            if (d <= maxDistance)
                visit(node.index);

            const int lo = d - maxDistance;
            const int hi = d + maxDistance;
            for (auto it = node.children.constBegin(); it != node.children.constEnd(); ++it) {
                if (it.key() >= lo && it.key() <= hi)
                    stack.append(it.value());
            }
        }
    }

private:
    struct Node {
        quint64 hash = 0;
        int index = -1;
        QHash<int, int> children; // distance from this node -> index into mNodes
    };
    QVector<Node> mNodes;
};

} // namespace

QList<DuplicateGroup> DuplicateFinderService::clusterSimilarImages(
    QList<HashedImage> images, int maxDistance)
{
    // Spec D2: sort by path first so the seed chosen for each new group —
    // and therefore the final grouping — is deterministic regardless of
    // directory-walk order.
    std::sort(images.begin(), images.end(),
              [](const HashedImage &a, const HashedImage &b) {
        return a.info.absoluteFilePath() < b.info.absoluteFilePath();
    });

    HammingBkTree tree;
    for (int i = 0; i < images.size(); ++i)
        tree.insert(images[i].hash, i);

    QVector<bool> grouped(images.size(), false);
    QList<DuplicateGroup> results;

    for (int i = 0; i < images.size(); ++i) {
        if (grouped[i])
            continue;

        // Neighbours are measured against this seed only (never against
        // each other), so a chain A~B~C with A and C both far from the
        // other cannot pull A and C into the same group — the exact
        // failure mode union-find allows and the spec rejects.
        QList<int> neighbors;
        tree.queryWithin(images[i].hash, maxDistance, [&](int idx) {
            if (idx != i && !grouped[idx])
                neighbors.append(idx);
        });

        if (neighbors.isEmpty())
            continue;

        DuplicateGroup group;
        group.kind = DuplicateGroup::Kind::Similar;
        grouped[i] = true;
        group.files.append(images[i].info); // seed listed first
        group.fileSize = static_cast<quint64>(images[i].info.size());

        int maxMemberDistance = 0;
        for (int idx : neighbors) {
            grouped[idx] = true;
            group.files.append(images[idx].info);
            maxMemberDistance = qMax(maxMemberDistance,
                ImageHash::hammingDistance(images[i].hash, images[idx].hash));
        }
        group.maxDistance = maxMemberDistance;
        results.append(group);
    }

    return results;
}

namespace {

QStringList supportedImageExtensions()
{
    QStringList exts;
    for (const QByteArray &fmt : QImageReader::supportedImageFormats())
        exts.append(QString::fromLatin1(fmt).toLower());
    return exts;
}

bool hasImageExtension(const QFileInfo &info, const QStringList &extensions)
{
    return extensions.contains(info.suffix().toLower());
}

} // namespace

QList<DuplicateGroup> DuplicateFinderService::runSimilarPipeline(
    const QStringList &directories,
    qint64 minSize,
    int maxDistance,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    const QStringList imageExtensions = supportedImageExtensions();

    // --- Stage 1: Collect candidate images, grouped by size for the exact pass ---
    QHash<qint64, QList<QFileInfo>> sizeGroups;
    QList<QFileInfo> allImages;
    QSet<std::pair<quint64, quint64>> seenInodes;
    int totalScanned = 0;

    for (const QString &dir : directories) {
        if (shouldStop())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (shouldStop())
                return {};

            it.next();
            const QFileInfo info = it.fileInfo();

            if (info.isSymLink())
                continue;
            if (info.size() < minSize)
                continue;
            if (!hasImageExtension(info, imageExtensions))
                continue;
            if (CleanerService::isExcluded(info.absoluteFilePath(), exclusions))
                continue;

            if (alreadyCollectedInode(info.absoluteFilePath(), seenInodes))
                continue;

            sizeGroups[info.size()].append(info);
            allImages.append(info);
            totalScanned++;

            if (totalScanned % 500 == 0)
                emit progressUpdated(1, totalScanned, 0,
                    tr("Collecting images... %1 found").arg(totalScanned));
        }
    }

    emit progressUpdated(1, totalScanned, totalScanned,
        tr("Collected %1 candidate images").arg(totalScanned));

    if (shouldStop())
        return {};

    // --- Stage 2: Exact matching over the image candidates ---
    const QList<DuplicateGroup> exactGroups =
        groupExactByContent(sizeGroups, totalScanned, /*baseStage=*/2, tr("Exact matching"));

    if (shouldStop())
        return {};

    // One representative per exact group stands in for the whole set in
    // the similarity pass — its siblings are not hashed or clustered a
    // second time. This is the spec's explicit exception to "a file
    // appears in at most one group": the representative may also appear in
    // a Similar group, but no other member of its Exact group ever does.
    QSet<QString> exactMemberPaths;
    QList<QFileInfo> candidates;
    candidates.reserve(exactGroups.size() + allImages.size());
    for (const DuplicateGroup &g : exactGroups) {
        candidates.append(g.files.first());
        for (const QFileInfo &fi : g.files)
            exactMemberPaths.insert(fi.absoluteFilePath());
    }
    for (const QFileInfo &fi : allImages) {
        if (!exactMemberPaths.contains(fi.absoluteFilePath()))
            candidates.append(fi);
    }

    // --- Stage 3: Hash candidates (parallel; tolerant of corrupt files) ---
    struct HashAttempt {
        QFileInfo info;
        quint64 hash = 0;
        bool ok = false;
    };

    QAtomicInt skipped{0};
    QAtomicInt hashedCount{0};
    const int totalCandidates = candidates.size();

    auto hashOne = [this, &skipped, &hashedCount, totalCandidates](const QFileInfo &fi) {
        HashAttempt attempt;
        attempt.info = fi;
        if (shouldStop())
            return attempt;

        // Must not fail the scan on an undecodable/corrupt file — just
        // skip it and keep a count for the final status line.
        const auto hash = ImageHash::hashFile(fi.absoluteFilePath());
        if (!hash) {
            skipped.fetchAndAddRelaxed(1);
            return attempt;
        }
        attempt.hash = *hash;
        attempt.ok = true;

        const int done = hashedCount.fetchAndAddRelaxed(1) + 1;
        if (done % 200 == 0)
            emit progressUpdated(3, done, totalCandidates,
                tr("Hashing images... %1/%2").arg(done).arg(totalCandidates));
        return attempt;
    };

    QList<HashAttempt> attempts;
    if (runsAsynchronously()) {
        // Own pool: while paused every hashOne parks, and that must not
        // starve other users of the global pool.
        QThreadPool hashPool;
        attempts = QtConcurrent::blockingMapped(&hashPool, candidates, hashOne);
    } else {
        attempts.reserve(candidates.size());
        for (const QFileInfo &fi : candidates)
            attempts.append(hashOne(fi));
    }

    if (shouldStop())
        return {};

    QList<HashedImage> validHashed;
    validHashed.reserve(attempts.size());
    for (const HashAttempt &attempt : attempts) {
        if (attempt.ok)
            validHashed.append({attempt.info, attempt.hash});
    }

    emit progressUpdated(3, totalCandidates, totalCandidates,
        tr("Hashed %1 images — %2 skipped (corrupt or unsupported)")
            .arg(validHashed.size()).arg(skipped.loadRelaxed()));

    if (shouldStop())
        return {};

    // --- Stage 4: Grouping ---
    const QList<DuplicateGroup> similarGroups =
        clusterSimilarImages(validHashed, maxDistance);

    QList<DuplicateGroup> results;
    results.reserve(exactGroups.size() + similarGroups.size());
    results.append(exactGroups);
    results.append(similarGroups);

    emit progressUpdated(4, results.size(), results.size(),
        tr("Scan complete: %1 exact + %2 similar groups found — %3 images skipped")
            .arg(exactGroups.size()).arg(similarGroups.size()).arg(skipped.loadRelaxed()));

    return results;
}

namespace {

// SSO-27146: one entry of a folder, as far as folder comparison cares.
struct FolderEntry {
    enum class Type { File, Link, Dir };
    Type type = Type::File;
    QString name;
    QString path;
    qint64 size = 0;      // File
    QByteArray target;    // Link: the raw readlink() text, not resolved
    int dirIndex = -1;    // Dir: index into FolderTree::nodes
};

struct FolderNode {
    QString path;
    QList<FolderEntry> entries;
    quint64 totalSize = 0;
    int fileCount = 0;
    bool eligible = true;
    QByteArray structureSig;
};

// Walks the scan roots once, post-order, giving every folder a signature of
// its layout (entry names, file sizes, sub-folder layouts). Content hashes
// are added later and only for folders whose layout matches another's.
class FolderTree
{
public:
    FolderTree(const QList<CleanerService::ExclusionEntry> &exclusions,
               std::function<bool()> stop,
               std::function<void(int)> onFolder)
        : mExclusions(exclusions), mStop(std::move(stop)), mOnFolder(std::move(onFolder)) {}

    QVector<FolderNode> nodes;

    int build(const QString &path)
    {
        const auto known = mIndexByPath.constFind(path);
        if (known != mIndexByPath.constEnd())
            return known.value();

        const int index = nodes.size();
        nodes.append(FolderNode());
        mIndexByPath.insert(path, index);

        FolderNode node;
        node.path = path;
        if (mOnFolder)
            mOnFolder(nodes.size());

        const QDir dir(path);
        if (!dir.isReadable() || CleanerService::isExcluded(path, mExclusions))
            node.eligible = false;

        QFileInfoList infos = dir.entryInfoList(
            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::NoSort);
        std::sort(infos.begin(), infos.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.fileName() < b.fileName();
        });

        for (const QFileInfo &fi : infos) {
            if (mStop && mStop())
                break;

            FolderEntry e;
            e.name = fi.fileName();
            e.path = fi.absoluteFilePath();

            if (CleanerService::isExcluded(e.path, mExclusions)) {
                node.eligible = false;
            } else if (fi.isSymLink()) {
                e.type = FolderEntry::Type::Link;
                char buf[4096];
                const ssize_t n = ::readlink(QFile::encodeName(e.path).constData(), buf, sizeof(buf));
                if (n < 0)
                    node.eligible = false;
                else
                    e.target = QByteArray(buf, static_cast<int>(n));
            } else if (fi.isDir()) {
                e.type = FolderEntry::Type::Dir;
                e.dirIndex = build(e.path);
                const FolderNode &child = nodes[e.dirIndex];
                node.totalSize += child.totalSize;
                node.fileCount += child.fileCount;
                if (!child.eligible)
                    node.eligible = false;
            } else if (fi.isFile()) {
                e.type = FolderEntry::Type::File;
                e.size = fi.size();
                node.totalSize += static_cast<quint64>(e.size);
                ++node.fileCount;
            } else {
                // socket, fifo, device node — cannot be compared
                node.eligible = false;
                continue;
            }
            node.entries.append(e);
        }

        QCryptographicHash sig(QCryptographicHash::Sha256);
        for (const FolderEntry &e : node.entries) {
            sig.addData(QByteArray(1, static_cast<char>(e.type)));
            sig.addData(e.name.toUtf8());
            sig.addData(QByteArrayView("\0", 1));
            switch (e.type) {
            case FolderEntry::Type::File:
                sig.addData(QByteArray::number(e.size));
                break;
            case FolderEntry::Type::Link:
                sig.addData(e.target);
                break;
            case FolderEntry::Type::Dir:
                sig.addData(nodes[e.dirIndex].structureSig);
                break;
            }
            sig.addData(QByteArrayView("\n", 1));
        }
        node.structureSig = sig.result();

        nodes[index] = node;
        return index;
    }

private:
    QList<CleanerService::ExclusionEntry> mExclusions;
    std::function<bool()> mStop;
    std::function<void(int)> mOnFolder;
    QHash<QString, int> mIndexByPath;
};

// Content signature of a folder: its layout plus the hash of every file.
// `partial` hashes only each file's first 4 KB — enough to split most layout
// matches cheaply before the full read. A file that cannot be hashed makes
// the folder's signature unique, so it never matches anything.
class FolderContentSigner
{
public:
    FolderContentSigner(const FolderTree &tree, bool partial,
                        std::function<bool()> stop, std::function<void(int)> onFileHashed)
        : mTree(tree), mPartial(partial), mStop(std::move(stop)),
          mOnFileHashed(std::move(onFileHashed)) {}

    QByteArray signatureOf(int index)
    {
        const auto cached = mMemo.constFind(index);
        if (cached != mMemo.constEnd())
            return cached.value();

        const FolderNode &node = mTree.nodes[index];
        QCryptographicHash sig(QCryptographicHash::Sha256);
        for (const FolderEntry &e : node.entries) {
            if (mStop && mStop())
                return {};

            sig.addData(QByteArray(1, static_cast<char>(e.type)));
            sig.addData(e.name.toUtf8());
            sig.addData(QByteArrayView("\0", 1));
            switch (e.type) {
            case FolderEntry::Type::File: {
                QByteArray h = mPartial ? hashFilePartial(e.path) : hashFileFull(e.path, mStop);
                if (h.isEmpty())
                    h = "!unreadable:" + e.path.toUtf8();
                sig.addData(h);
                if (mOnFileHashed)
                    mOnFileHashed(++mFilesHashed);
                break;
            }
            case FolderEntry::Type::Link:
                sig.addData(e.target);
                break;
            case FolderEntry::Type::Dir:
                sig.addData(signatureOf(e.dirIndex));
                break;
            }
            sig.addData(QByteArrayView("\n", 1));
        }

        const QByteArray result = sig.result();
        mMemo.insert(index, result);
        return result;
    }

private:
    const FolderTree &mTree;
    bool mPartial;
    std::function<bool()> mStop;
    std::function<void(int)> mOnFileHashed;
    QHash<int, QByteArray> mMemo;
    int mFilesHashed = 0;
};

} // namespace

QList<DuplicateGroup> DuplicateFinderService::runDirectoryPipeline(
    const QStringList &directories,
    qint64 minSize,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    // --- Stage 1: walk every root, giving each folder a layout signature ---
    FolderTree tree(exclusions, [this] { return shouldStop(); },
        [this](int folders) {
            if (folders % 200 == 0)
                emit progressUpdated(1, folders, 0,
                    tr("Stage 1: Scanning folders... %1 found").arg(folders));
        });
    for (const QString &root : directories) {
        if (shouldStop())
            return {};
        tree.build(QFileInfo(root).absoluteFilePath());
    }
    if (shouldStop())
        return {};

    // --- Stage 2: folders with the same layout are the only candidates ---
    QHash<QByteArray, QList<int>> byLayout;
    for (int i = 0; i < tree.nodes.size(); ++i) {
        const FolderNode &n = tree.nodes[i];
        if (n.eligible && n.fileCount > 0 && static_cast<qint64>(n.totalSize) >= minSize)
            byLayout[n.structureSig].append(i);
    }

    QList<QList<int>> candidates;
    int candidateFolders = 0;
    for (auto it = byLayout.constBegin(); it != byLayout.constEnd(); ++it) {
        if (it.value().size() >= 2) {
            candidates.append(it.value());
            candidateFolders += it.value().size();
        }
    }
    emit progressUpdated(2, candidateFolders, candidateFolders,
        tr("Stage 2 complete: %1 candidate folders").arg(candidateFolders));

    // --- Stage 3/4: split candidates by partial, then full, content hash ---
    auto refine = [this, &tree](const QList<QList<int>> &groups, bool partial, int stage,
                                const QString &label) {
        FolderContentSigner signer(tree, partial, [this] { return shouldStop(); },
            [this, stage, label](int hashed) {
                if (hashed % 25 == 0)
                    emit progressUpdated(stage, hashed, 0, label.arg(hashed));
            });
        QList<QList<int>> refined;
        for (const QList<int> &group : groups) {
            QHash<QByteArray, QList<int>> bySig;
            for (int idx : group) {
                const QByteArray sig = signer.signatureOf(idx);
                if (shouldStop())
                    return QList<QList<int>>();
                bySig[sig].append(idx);
            }
            for (auto it = bySig.constBegin(); it != bySig.constEnd(); ++it) {
                if (it.value().size() >= 2)
                    refined.append(it.value());
            }
        }
        return refined;
    };

    candidates = refine(candidates, /*partial=*/true, 3,
                        tr("Stage 3: Partial hashing folders... %1 files"));
    if (shouldStop())
        return {};
    candidates = refine(candidates, /*partial=*/false, 4,
                        tr("Stage 4: Full hashing folders... %1 files"));
    if (shouldStop())
        return {};

    // --- Report only the outermost match ---
    QSet<QString> memberPaths;
    for (const QList<int> &group : candidates) {
        for (int idx : group)
            memberPaths.insert(tree.nodes[idx].path);
    }
    auto insideAnotherMember = [&memberPaths](const QString &path) {
        for (int slash = path.lastIndexOf('/'); slash > 0; slash = path.lastIndexOf('/', slash - 1)) {
            if (memberPaths.contains(path.left(slash)))
                return true;
        }
        return false;
    };

    QList<DuplicateGroup> results;
    for (const QList<int> &group : candidates) {
        QStringList paths;
        for (int idx : group) {
            const QString &path = tree.nodes[idx].path;
            if (!insideAnotherMember(path))
                paths.append(path);
        }
        if (paths.size() < 2)
            continue;
        std::sort(paths.begin(), paths.end());

        DuplicateGroup g;
        g.kind = DuplicateGroup::Kind::Directory;
        for (const QString &path : paths)
            g.files.append(QFileInfo(path));
        g.fileSize = tree.nodes[group.first()].totalSize;
        results.append(g);
    }

    std::sort(results.begin(), results.end(),
              [](const DuplicateGroup &a, const DuplicateGroup &b) {
        const quint64 wastedA = a.fileSize * (a.files.size() - 1);
        const quint64 wastedB = b.fileSize * (b.files.size() - 1);
        if (wastedA != wastedB)
            return wastedA > wastedB;
        return a.files.first().absoluteFilePath() < b.files.first().absoluteFilePath();
    });

    emit progressUpdated(5, results.size(), results.size(),
        tr("Scan complete: %1 duplicate folder groups found").arg(results.size()));

    return results;
}

QList<LargeFileEntry> DuplicateFinderService::runLargestPipeline(
    const QStringList &directories,
    int topN,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    QList<LargeFileEntry> candidates;

    for (const QString &dir : directories) {
        if (shouldStop())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (shouldStop())
                return {};

            it.next();
            const QFileInfo info = it.fileInfo();
            if (info.isSymLink())
                continue;
            if (CleanerService::isExcluded(info.absoluteFilePath(), exclusions))
                continue;

            LargeFileEntry e;
            e.info = info;
            e.size = static_cast<quint64>(info.size());
            candidates.append(e);
        }
    }

    return rankLargest(candidates, topN);
}

QStringList DuplicateFinderService::runEmptyFoldersPipeline(
    const QStringList &directories,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    QStringList empties;

    for (const QString &dir : directories) {
        if (shouldStop())
            return {};

        // Walk directories only — visit every descendant directory and
        // ask whether it has any entries (excluding `.` and `..`).
        QDirIterator it(dir, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (shouldStop())
                return {};

            it.next();
            const QFileInfo info = it.fileInfo();
            if (info.isSymLink())
                continue;
            if (CleanerService::isExcluded(info.absoluteFilePath(), exclusions))
                continue;

            QDir d(info.absoluteFilePath());
            if (d.isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                empties.append(info.absoluteFilePath());
        }
    }

    std::sort(empties.begin(), empties.end());
    return empties;
}
