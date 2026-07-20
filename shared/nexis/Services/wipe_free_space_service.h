#ifndef WIPE_FREE_SPACE_SERVICE_H
#define WIPE_FREE_SPACE_SERVICE_H

#include <QObject>
#include <QAtomicInt>
#include <QFuture>
#include <QString>

#include <Info/disk_info.h>

// SSO-15382: a mounted, writable volume as offered to the user, plus whether
// this service will use the TRIM-based reclaim path or the fill-and-delete
// overwrite path on it.
struct WipeTarget {
    QString rootPath;
    QString displayName;
    QString device;
    quint64 totalBytes = 0;
    quint64 freeBytes = 0;
    bool trimEligible = false;
    QString trimUnavailableReason;
};

// SSO-15382: "Wipe Free Space" — fill-and-delete pass over a volume's free
// space, with an fstrim-aware path on SSDs (TRIM-based reclaim instead of a
// full overwrite pass where the OS/filesystem supports it). Singleton +
// QtConcurrent worker + QAtomicInt cancel flag, mirrors DuplicateFinderService.
class WipeFreeSpaceService : public QObject
{
    Q_OBJECT

public:
    static WipeFreeSpaceService *ins();

    // Safety margin (SSO-15382 acceptance criteria): the fill pass must never
    // attempt to consume 100% of free space. Default headroom is 5% of the
    // volume's total capacity, floored at 1 GiB and capped at 8 GiB so tiny
    // volumes keep a meaningful margin and huge volumes don't lose an
    // excessive amount of usable space to the safety margin.
    static constexpr quint64 kMinHeadroomBytes = 1ULL * 1024 * 1024 * 1024;
    static constexpr quint64 kMaxHeadroomBytes = 8ULL * 1024 * 1024 * 1024;
    static constexpr double kHeadroomFraction = 0.05;

    static quint64 headroomForVolume(quint64 totalBytes);

    // Mounted, writable volumes the user may target, with TRIM eligibility
    // pre-computed for the preview step.
    QList<WipeTarget> listWipeableVolumes() const;

    void startWipe(const QString &rootPath);
    void cancel();
    bool isRunning() const;

    // SSO-15382 cleanup-on-interrupt: call once at app startup. If a
    // previous run was cancelled by a crash/kill mid-fill, this removes the
    // leftover temp fill file it recorded so the volume is never left
    // artificially full.
    static void recoverFromCrash();

signals:
    // For the TRIM path, estimatedTotalBytes is 0 (no byte-level progress).
    void progressUpdated(qint64 bytesWritten, qint64 estimatedTotalBytes, const QString &message);
    void finished(bool success, const QString &message);
    void cancelled();

protected:
    explicit WipeFreeSpaceService(QObject *parent = nullptr);
    ~WipeFreeSpaceService() override;

private:
    static WipeFreeSpaceService *instance;

    static QString markerFilePath();
    static void writeMarker(const QString &tempFilePath);
    static void clearMarker();

    bool detectTrimEligible(const Disk &disk, QString *reason) const;
    void runFillAndDelete(const QString &rootPath, quint64 headroom);
    void runTrim(const QString &rootPath);

    QAtomicInt mCancelled{0};
    QFuture<void> mWorkerFuture;
};

#endif // WIPE_FREE_SPACE_SERVICE_H
