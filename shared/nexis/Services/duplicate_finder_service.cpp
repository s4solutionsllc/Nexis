#include "duplicate_finder_service.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtConcurrent>

#include <algorithm>

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

    mWorkerFuture = QtConcurrent::run(
        [this, directories, minSize, globFilter, exclusions]() {
        QList<DuplicateGroup> results =
            runPipeline(directories, minSize, globFilter, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit scanFinished(results);
    });
}

void DuplicateFinderService::scanLargest(const QStringList &directories, int topN)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    mWorkerFuture = QtConcurrent::run([this, directories, topN, exclusions]() {
        QList<LargeFileEntry> results =
            runLargestPipeline(directories, topN, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit largestScanFinished(results);
    });
}

void DuplicateFinderService::scanEmptyFolders(const QStringList &directories)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    const auto exclusions = loadExclusions();

    mWorkerFuture = QtConcurrent::run([this, directories, exclusions]() {
        QStringList results = runEmptyFoldersPipeline(directories, exclusions);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit emptyFoldersScanFinished(results);
    });
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

    // Remove size groups with only one file
    QList<qint64> sizesToRemove;
    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it) {
        if (it.value().size() <= 1)
            sizesToRemove.append(it.key());
    }
    for (qint64 s : sizesToRemove)
        sizeGroups.remove(s);

    // Count candidates for stage 2
    int totalCandidates = 0;
    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it)
        totalCandidates += it.value().size();

    emit progressUpdated(1, totalScanned, totalScanned,
        tr("Stage 1 complete: %1 candidates in %2 size groups")
            .arg(totalCandidates).arg(sizeGroups.size()));

    if (mCancelled.loadRelaxed())
        return {};

    // --- Stage 2: Partial hash (first 4 KB) ---
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
                emit progressUpdated(2, hashProgress, totalCandidates,
                    tr("Stage 2: Partial hashing... %1/%2")
                        .arg(hashProgress).arg(totalCandidates));
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

    emit progressUpdated(2, totalCandidates, totalCandidates,
        tr("Stage 2 complete: %1 candidates remaining").arg(stage3Candidates));

    if (mCancelled.loadRelaxed())
        return {};

    // --- Stage 3: Full hash ---
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
                emit progressUpdated(3, fullProgress, stage3Candidates,
                    tr("Stage 3: Full hashing... %1/%2")
                        .arg(fullProgress).arg(stage3Candidates));
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
            results.append(group);
        }
    }

    // Sort by wasted space descending
    std::sort(results.begin(), results.end(),
              [](const DuplicateGroup &a, const DuplicateGroup &b) {
        return a.fileSize * (a.files.size() - 1) >
               b.fileSize * (b.files.size() - 1);
    });

    emit progressUpdated(3, stage3Candidates, stage3Candidates,
        tr("Scan complete: %1 duplicate groups found").arg(results.size()));

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
