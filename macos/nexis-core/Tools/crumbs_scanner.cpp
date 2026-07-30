#include "crumbs_scanner.h"

#include "Utils/file_util.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrent>

#include <algorithm>

namespace CrumbsScanner {

namespace {

QStringList searchRoots(const QString &home)
{
    return {
        home + "/Library/Preferences",
        home + "/Library/Application Support",
        home + "/Library/Caches",
        home + "/Library/Saved Application State",
        home + "/Library/Containers",
        home + "/Library/Logs",
    };
}

bool leafMatchesAnyId(const QString &leaf, const QStringList &bundleIds, QString *matched)
{
    for (const QString &id : bundleIds) {
        if (id.isEmpty())
            continue;
        if (leaf == id || leaf.startsWith(id + QLatin1String("."))
            || leaf.startsWith(id + QLatin1String("/"))
            || leaf.startsWith(id + QLatin1String("-")))
        {
            if (matched)
                *matched = id;
            return true;
        }
        // The Preferences dir typically contains "<bundleId>.plist" —
        // match as a filename-prefix too.
        if (leaf.startsWith(id)) {
            const int trailing = leaf.length() - id.length();
            const QChar next = leaf.at(id.length());
            if (trailing > 0 && !next.isLetterOrNumber()) {
                if (matched)
                    *matched = id;
                return true;
            }
        }
    }
    return false;
}

} // namespace

QList<CrumbCandidate> scanCrumbs(
    const QStringList &bundleIds,
    QAtomicInt *cancelled,
    const std::function<void(const CrumbCandidate &)> &itemFoundCb)
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return scanCrumbsUnderHome(home, bundleIds, cancelled, itemFoundCb);
}

QList<CrumbCandidate> scanCrumbsUnderHome(
    const QString &homeDir,
    const QStringList &bundleIds,
    QAtomicInt *cancelled,
    const std::function<void(const CrumbCandidate &)> &itemFoundCb)
{
    QList<CrumbCandidate> out;
    if (bundleIds.isEmpty())
        return out;

    for (const QString &rootPath : searchRoots(homeDir)) {
        if (cancelled && cancelled->loadRelaxed())
            break;

        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QFileInfoList entries = root.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo &fi : entries) {
            if (cancelled && cancelled->loadRelaxed())
                break;

            QString matched;
            if (!leafMatchesAnyId(fi.fileName(), bundleIds, &matched))
                continue;

            CrumbCandidate c;
            c.path = fi.absoluteFilePath();
            c.sizeBytes = static_cast<qint64>(FileUtil::getFileSize(c.path));
            c.matchedBundleId = matched;
            out.append(c);
            if (itemFoundCb)
                itemFoundCb(c);
        }
    }

    std::sort(out.begin(), out.end(),
        [](const CrumbCandidate &a, const CrumbCandidate &b) {
            return a.sizeBytes > b.sizeBytes;
        });

    return out;
}

} // namespace CrumbsScanner

CrumbsScanRunner::CrumbsScanRunner(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<CrumbsScanner::CrumbCandidate>("CrumbsScanner::CrumbCandidate");
    qRegisterMetaType<QList<CrumbsScanner::CrumbCandidate>>("QList<CrumbsScanner::CrumbCandidate>");
}

CrumbsScanRunner::~CrumbsScanRunner()
{
    // Wait for the worker before our `this` capture (used inside the
    // QtConcurrent lambda below) goes dangling — same rationale as
    // TrustSafetyRunner's destructor.
    if (mScanFuture.isRunning()) {
        mCancelled.storeRelaxed(1);
        mScanFuture.waitForFinished();
    }
}

void CrumbsScanRunner::startScan(const QStringList &bundleIds)
{
    if (mScanFuture.isRunning())
        return;
    mCancelled.storeRelaxed(0);

    mScanFuture = QtConcurrent::run([this, bundleIds]() {
        auto itemFoundCb = [this](const CrumbsScanner::CrumbCandidate &item) {
            emit itemFound(item);
        };
        const QList<CrumbsScanner::CrumbCandidate> items =
            CrumbsScanner::scanCrumbs(bundleIds, &mCancelled, itemFoundCb);
        if (mCancelled.loadRelaxed())
            emit scanCancelled();
        else
            emit scanFinished(items);
    });
}

void CrumbsScanRunner::cancelScan()
{
    mCancelled.storeRelaxed(1);
}

bool CrumbsScanRunner::isScanning() const
{
    return mScanFuture.isRunning();
}
