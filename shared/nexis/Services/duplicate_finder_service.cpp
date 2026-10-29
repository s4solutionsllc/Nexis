#include "duplicate_finder_service.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <QtConcurrent>

#include <algorithm>
#include <functional>

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
    mCancelled.storeRelaxed(1);
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

void DuplicateFinderService::scanLargest(const QStringList &directories, int topN)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
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

static QByteArray hashFileFull(const QString &path, const QAtomicInt &cancelled)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    const qint64 chunkSize = 65536;
    while (!f.atEnd()) {
        if (cancelled.loadRelaxed())
            return {};
        hasher.addData(f.read(chunkSize));
    }
    f.close();
    return hasher.result();
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
    int totalScanned = 0;

    for (const QString &dir : directories) {
        if (mCancelled.loadRelaxed())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (mCancelled.loadRelaxed())
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

            sizeGroups[info.size()].append(info);
            totalScanned++;

            if (totalScanned % 1000 == 0)
                emit progressUpdated(1, totalScanned, 0,
                    tr("Stage 1: Scanning files... %1 found").arg(totalScanned));
        }
    }

    if (mCancelled.loadRelaxed())
        return {};

    const QList<DuplicateGroup> results =
        groupExactByContent(sizeGroups, totalScanned, /*baseStage=*/2);

    if (mCancelled.loadRelaxed())
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

    if (mCancelled.loadRelaxed())
        return {};

    // --- Partial hash (first 4 KB) ---
    QHash<QByteArray, QList<QFileInfo>> partialHashGroups;
    int hashProgress = 0;

    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (mCancelled.loadRelaxed())
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

    if (mCancelled.loadRelaxed())
        return {};

    // --- Full hash ---
    QHash<QByteArray, QList<QFileInfo>> fullHashGroups;
    int fullProgress = 0;

    for (auto it = partialHashGroups.constBegin();
         it != partialHashGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (mCancelled.loadRelaxed())
                return {};

            QByteArray fullHash = hashFileFull(fi.absoluteFilePath(), mCancelled);
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
    int totalScanned = 0;

    for (const QString &dir : directories) {
        if (mCancelled.loadRelaxed())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (mCancelled.loadRelaxed())
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

    if (mCancelled.loadRelaxed())
        return {};

    // --- Stage 2: Exact matching over the image candidates ---
    const QList<DuplicateGroup> exactGroups =
        groupExactByContent(sizeGroups, totalScanned, /*baseStage=*/2, tr("Exact matching"));

    if (mCancelled.loadRelaxed())
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
        if (mCancelled.loadRelaxed())
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
    if (runsAsynchronously())
        attempts = QtConcurrent::blockingMapped(candidates, hashOne);
    else {
        attempts.reserve(candidates.size());
        for (const QFileInfo &fi : candidates)
            attempts.append(hashOne(fi));
    }

    if (mCancelled.loadRelaxed())
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

    if (mCancelled.loadRelaxed())
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

QList<LargeFileEntry> DuplicateFinderService::runLargestPipeline(
    const QStringList &directories,
    int topN,
    const QList<CleanerService::ExclusionEntry> &exclusions)
{
    QList<LargeFileEntry> candidates;

    for (const QString &dir : directories) {
        if (mCancelled.loadRelaxed())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (mCancelled.loadRelaxed())
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
        if (mCancelled.loadRelaxed())
            return {};

        // Walk directories only — visit every descendant directory and
        // ask whether it has any entries (excluding `.` and `..`).
        QDirIterator it(dir, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (mCancelled.loadRelaxed())
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
