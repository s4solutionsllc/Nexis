#include "duplicate_finder_service.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QRegularExpression>
#include <QtConcurrent>

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
}

bool DuplicateFinderService::isScanning() const
{
    return mWorkerFuture.isRunning();
}

void DuplicateFinderService::cancel()
{
    mCancelled.storeRelaxed(1);
}

void DuplicateFinderService::scan(const QStringList &directories,
                                   qint64 minSize,
                                   const QString &globFilter)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);

    mWorkerFuture = QtConcurrent::run([this, directories, minSize, globFilter]() {
        QList<DuplicateGroup> results = runPipeline(directories, minSize, globFilter);
        emit scanFinished(results);
    });
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

static QByteArray hashFileFull(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    const qint64 chunkSize = 65536;
    while (!f.atEnd()) {
        hasher.addData(f.read(chunkSize));
    }
    f.close();
    return hasher.result();
}

QList<DuplicateGroup> DuplicateFinderService::runPipeline(
    const QStringList &directories, qint64 minSize, const QString &globFilter)
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

            if (info.size() < minSize)
                continue;

            if (globRe.isValid() && !globFilter.isEmpty()) {
                if (!globRe.match(info.fileName()).hasMatch())
                    continue;
            }

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
    for (auto it = partialHashGroups.constBegin(); it != partialHashGroups.constEnd(); ++it) {
        if (it.value().size() <= 1)
            hashesToRemove.append(it.key());
    }
    for (const QByteArray &h : hashesToRemove)
        partialHashGroups.remove(h);

    int stage3Candidates = 0;
    for (auto it = partialHashGroups.constBegin(); it != partialHashGroups.constEnd(); ++it)
        stage3Candidates += it.value().size();

    emit progressUpdated(2, totalCandidates, totalCandidates,
        tr("Stage 2 complete: %1 candidates remaining").arg(stage3Candidates));

    if (mCancelled.loadRelaxed())
        return {};

    // --- Stage 3: Full hash ---
    QHash<QByteArray, QList<QFileInfo>> fullHashGroups;
    int fullProgress = 0;

    for (auto it = partialHashGroups.constBegin(); it != partialHashGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (mCancelled.loadRelaxed())
                return {};

            QByteArray fullHash = hashFileFull(fi.absoluteFilePath());
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
    for (auto it = fullHashGroups.constBegin(); it != fullHashGroups.constEnd(); ++it) {
        if (it.value().size() >= 2) {
            DuplicateGroup group;
            group.files = it.value();
            group.fileSize = group.files.first().size();
            group.hash = it.key();
            results.append(group);
        }
    }

    // Sort by wasted space descending
    std::sort(results.begin(), results.end(), [](const DuplicateGroup &a, const DuplicateGroup &b) {
        return a.fileSize * (a.files.size() - 1) > b.fileSize * (b.files.size() - 1);
    });

    emit progressUpdated(3, stage3Candidates, stage3Candidates,
        tr("Scan complete: %1 duplicate groups found").arg(results.size()));

    return results;
}
