#include "wipe_free_space_service.h"

#include "Managers/info_manager.h"
#include <Utils/command_util.h>
#include <Utils/file_util.h>
#include <Utils/format_util.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QtConcurrent>
#include <QDebug>

namespace {
// SSO-15382: fixed name (not QTemporaryFile) so recoverFromCrash() and a
// concurrent run of this singleton always agree on where the fill file
// lives; only one wipe runs at a time (guarded by mWorkerFuture.isRunning()).
const QString kFillFileName = QStringLiteral(".nexis-wipe-fill.tmp");
}

WipeFreeSpaceService *WipeFreeSpaceService::instance = nullptr;

WipeFreeSpaceService *WipeFreeSpaceService::ins()
{
    if (!instance)
        instance = new WipeFreeSpaceService;
    return instance;
}

WipeFreeSpaceService::WipeFreeSpaceService(QObject *parent)
    : QObject(parent)
{
}

WipeFreeSpaceService::~WipeFreeSpaceService()
{
    cancel();
    mWorkerFuture.waitForFinished();
}

bool WipeFreeSpaceService::isRunning() const
{
    return mWorkerFuture.isRunning();
}

void WipeFreeSpaceService::cancel()
{
    mCancelled.storeRelaxed(1);
}

quint64 WipeFreeSpaceService::headroomForVolume(quint64 totalBytes)
{
    const quint64 fraction = static_cast<quint64>(static_cast<double>(totalBytes) * kHeadroomFraction);
    return qBound(kMinHeadroomBytes, fraction, kMaxHeadroomBytes);
}

bool WipeFreeSpaceService::detectTrimEligible(const Disk &disk, QString *reason) const
{
#ifdef Q_OS_LINUX
    QString base = disk.device;
    base.remove(QStringLiteral("/dev/"));

    static const QRegularExpression nvmeRe(QStringLiteral(R"(^(nvme\d+n\d+)p\d+$)"));
    static const QRegularExpression mmcRe(QStringLiteral(R"(^(mmcblk\d+)p\d+$)"));
    static const QRegularExpression partRe(QStringLiteral(R"(^([A-Za-z]+)\d+$)"));

    QRegularExpressionMatch m = nvmeRe.match(base);
    if (m.hasMatch()) {
        base = m.captured(1);
    } else if ((m = mmcRe.match(base)).hasMatch()) {
        base = m.captured(1);
    } else if ((m = partRe.match(base)).hasMatch()) {
        base = m.captured(1);
    }

    const QString sysPath = QStringLiteral("/sys/block/%1/queue/rotational").arg(base);
    if (!QFile::exists(sysPath)) {
        if (reason)
            *reason = tr("Could not determine whether this volume is solid-state.");
        return false;
    }

    const bool isSsd = FileUtil::readStringFromFile(sysPath).trimmed() == QLatin1String("0");
    if (!isSsd) {
        if (reason)
            *reason = tr("Rotational (HDD) media — TRIM does not apply.");
        return false;
    }

    if (!CommandUtil::isExecutable(QStringLiteral("fstrim"))) {
        if (reason)
            *reason = tr("Solid-state, but fstrim is not installed.");
        return false;
    }

    return true;
#elif defined(Q_OS_MAC)
    ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("diskutil"), {QStringLiteral("info"), QStringLiteral("-plist"), disk.rootPath}, 5000);
    if (!r.ok()) {
        if (reason)
            *reason = tr("Could not determine whether this volume is solid-state.");
        return false;
    }

    static const QRegularExpression re(QStringLiteral(R"(<key>SolidState</key>\s*<(true|false)/>)"));
    const QRegularExpressionMatch m = re.match(r.output);
    const bool isSolidState = m.hasMatch() && m.captured(1) == QLatin1String("true");
    if (!isSolidState) {
        if (reason)
            *reason = tr("Rotational or unknown media — TRIM does not apply.");
        return false;
    }

    return true;
#else
    Q_UNUSED(disk);
    if (reason)
        *reason = tr("TRIM detection isn't available on this platform.");
    return false;
#endif
}

QList<WipeTarget> WipeFreeSpaceService::listWipeableVolumes() const
{
    QList<WipeTarget> targets;
    const QList<Disk> disks = InfoManager::ins()->collectDiskInfo();

    for (const Disk &disk : disks) {
        if (disk.rootPath.isEmpty())
            continue;
        if (!QFileInfo(disk.rootPath).isWritable())
            continue;

        WipeTarget t;
        t.rootPath = disk.rootPath;
        t.displayName = disk.name.isEmpty() ? disk.rootPath : disk.name;
        t.device = disk.device;
        t.totalBytes = disk.size;
        t.freeBytes = disk.free;

        QString reason;
        t.trimEligible = detectTrimEligible(disk, &reason);
        t.trimUnavailableReason = reason;

        targets << t;
    }

    return targets;
}

void WipeFreeSpaceService::startWipe(const QString &rootPath)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);

    const QList<Disk> disks = InfoManager::ins()->collectDiskInfo();
    Disk target;
    bool found = false;
    for (const Disk &d : disks) {
        if (d.rootPath == rootPath) {
            target = d;
            found = true;
            break;
        }
    }
    if (!found) {
        emit finished(false, tr("%1 is no longer available.").arg(rootPath));
        return;
    }

    const quint64 headroom = headroomForVolume(target.size);

    // Eligibility detection can shell out (diskutil on macOS) — run it on
    // the worker thread too so confirming the wipe never blocks the GUI.
    mWorkerFuture = QtConcurrent::run([this, target, rootPath, headroom]() {
        if (detectTrimEligible(target, nullptr))
            runTrim(rootPath);
        else
            runFillAndDelete(rootPath, headroom);
    });
}

void WipeFreeSpaceService::runFillAndDelete(const QString &rootPath, quint64 headroom)
{
    QString base = rootPath;
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    const QString tempPath = base + kFillFileName;

    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit finished(false, tr("Failed to create a temporary file on %1 — %2")
                                  .arg(rootPath, file.errorString()));
        return;
    }

    // Written before the first byte lands: recoverFromCrash() only needs to
    // find this marker to know a fill file may be sitting on disk.
    writeMarker(tempPath);

    const qint64 chunkSize = 4 * 1024 * 1024; // 4 MiB
    const QByteArray zeros(chunkSize, '\0');

    qint64 totalWritten = 0;
    qint64 estimatedTotal = 0;
    {
        QStorageInfo info(rootPath);
        info.refresh();
        estimatedTotal = qMax<qint64>(
            0, static_cast<qint64>(info.bytesAvailable()) - static_cast<qint64>(headroom));
    }

    bool ok = true;
    QString errorMsg;

    while (true) {
        if (mCancelled.loadRelaxed())
            break;

        QStorageInfo info(rootPath);
        info.refresh();
        const quint64 available = static_cast<quint64>(qMax<qint64>(0, info.bytesAvailable()));
        if (available <= headroom)
            break;

        qint64 toWrite = chunkSize;
        if (available - headroom < static_cast<quint64>(chunkSize))
            toWrite = static_cast<qint64>(available - headroom);
        if (toWrite <= 0)
            break;

        const qint64 written = file.write(zeros.constData(), toWrite);
        if (written <= 0) {
            // Ran into ENOSPC (another process ate the remaining margin) or a
            // real I/O error. Either way, stop filling — a partial pass with
            // no bytes written at all means the file was never writable to
            // begin with (permission/I-O error), which is a hard failure.
            if (totalWritten == 0) {
                ok = false;
                errorMsg = file.errorString();
            }
            break;
        }

        totalWritten += written;
        emit progressUpdated(totalWritten, estimatedTotal,
            tr("Wiping free space… %1 written").arg(FormatUtil::formatBytes(static_cast<quint64>(totalWritten))));
    }

    file.close();

    const bool wasCancelled = mCancelled.loadRelaxed() != 0;

    // The volume must never be left artificially full: remove the fill file
    // whether this pass succeeded, failed, or was cancelled.
    QFile::remove(tempPath);
    clearMarker();

    if (!ok) {
        emit finished(false, tr("Wipe failed: %1").arg(errorMsg));
        return;
    }

    if (wasCancelled) {
        emit cancelled();
        return;
    }

    emit finished(true, tr("Done — %1 reclaimed.").arg(FormatUtil::formatBytes(static_cast<quint64>(totalWritten))));
}

void WipeFreeSpaceService::runTrim(const QString &rootPath)
{
    emit progressUpdated(0, 0, tr("Reclaiming free space via TRIM on %1…").arg(rootPath));

#ifdef Q_OS_LINUX
    // Mirrors TrimWidget::runFstrimNow() — fstrim needs root to issue the
    // FITRIM ioctl.
    ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("pkexec"), {QStringLiteral("fstrim"), QStringLiteral("-v"), rootPath}, 60000);

    if (mCancelled.loadRelaxed()) {
        emit cancelled();
        return;
    }

    if (!r.ok()) {
        const QString detail = r.error.isEmpty() ? r.output : r.error;
        emit finished(false, tr("TRIM failed: %1").arg(detail));
        return;
    }

    emit finished(true, tr("Done — free space on %1 signalled as reclaimable via TRIM.").arg(rootPath));
#elif defined(Q_OS_MAC)
    // APFS on solid-state media issues TRIM automatically as part of normal
    // filesystem operation — macOS exposes no user-space "run TRIM now" for
    // free space. Same SSD/TRIM honesty point as the shredder feature:
    // reclaiming via TRIM signals blocks as reusable, it is not a
    // cryptographic-erase guarantee.
    if (mCancelled.loadRelaxed()) {
        emit cancelled();
        return;
    }
    emit finished(true, tr("Done — APFS reclaims free space on solid-state volumes automatically; "
                           "no additional action was needed on %1.").arg(rootPath));
#else
    Q_UNUSED(rootPath);
    emit finished(false, tr("TRIM is not supported on this platform."));
#endif
}

QString WipeFreeSpaceService::markerFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/wipe_free_space_state.json");
}

void WipeFreeSpaceService::writeMarker(const QString &tempFilePath)
{
    QJsonObject obj;
    obj["tempFilePath"] = tempFilePath;

    QFile f(markerFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void WipeFreeSpaceService::clearMarker()
{
    QFile::remove(markerFilePath());
}

void WipeFreeSpaceService::recoverFromCrash()
{
    const QString marker = markerFilePath();
    QFile f(marker);
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    const QString tempFilePath = obj.value(QStringLiteral("tempFilePath")).toString();
    if (!tempFilePath.isEmpty() && QFile::exists(tempFilePath)) {
        if (QFile::remove(tempFilePath))
            qInfo() << "WipeFreeSpaceService: removed a wipe temp file left behind by a previous run:" << tempFilePath;
        else
            qWarning() << "WipeFreeSpaceService: failed to remove stale wipe temp file:" << tempFilePath;
    }

    QFile::remove(marker);
}
