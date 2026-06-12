#include "oomd_info_linux.h"

#include "Utils/command_util.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMutexLocker>

namespace {

class DefaultRunner : public OomdInfoLinux::CommandRunner
{
public:
    QByteArray run(const QString &cmd,
                   const QStringList &args,
                   int timeoutMs) override
    {
        ExecResult r = CommandUtil::execWithStatus(cmd, args, timeoutMs);
        // FW-11 keeps these calls best-effort: a non-zero exit (oomd not
        // installed, journalctl restricted) is reported back as empty bytes so
        // the parser cleanly skips it.
        if (!r.ok())
            return {};
        return r.output.toUtf8();
    }

    bool exists(const QString &cmd) override
    {
        return CommandUtil::isExecutable(cmd);
    }
};

QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

OomdInfoLinux::OomdInfoLinux(CommandRunner *runner)
    : mRunner(runner ? runner : new DefaultRunner()),
      mOwnsRunner(runner == nullptr),
      mCgroupRoot(QStringLiteral("/sys/fs/cgroup"))
{
}

OomdInfoLinux::~OomdInfoLinux()
{
    if (mOwnsRunner)
        delete mRunner;
}

void OomdInfoLinux::setCgroupRootForTesting(const QString &root)
{
    mCgroupRoot = root;
}

void OomdInfoLinux::update()
{
    // Defensive: only proceed when the host exposes the cgroup v2 unified
    // hierarchy (FW-11 spec). systemd 259 removed v1 support entirely on the
    // 26.04 baseline, but we still ship to older distros — when the marker
    // is missing we skip the memory.events read and trust systemctl/journal
    // for whatever signal they can provide.
    const QString markerPath = mCgroupRoot + QStringLiteral("/cgroup.controllers");
    const bool cgroupV2 = QFile::exists(markerPath);

    QMap<QString, QString> systemctlProps;
    if (mRunner->exists(QStringLiteral("systemctl"))) {
        const QByteArray bytes = mRunner->run(
            QStringLiteral("systemctl"),
            { QStringLiteral("show"),
              QStringLiteral("systemd-oomd.service"),
              QStringLiteral("--property=LoadState,ActiveState,OOMKills,ManagedOOMKills"),
              QStringLiteral("--no-pager") },
            2000);
        systemctlProps = OomdInfoParser::parseSystemctlShow(bytes);
    }

    QMap<QString, quint64> memoryEvents;
    if (cgroupV2) {
        const QByteArray bytes = readFile(mCgroupRoot + QStringLiteral("/memory.events"));
        if (!bytes.isEmpty())
            memoryEvents = OomdInfoParser::parseCgroupV2KeyedFile(bytes);
    }

    QList<OomdEvent> recent;
    if (mRunner->exists(QStringLiteral("journalctl"))) {
        const QByteArray bytes = mRunner->run(
            QStringLiteral("journalctl"),
            { QStringLiteral("-u"),
              QStringLiteral("systemd-oomd.service"),
              QStringLiteral("-o"),
              QStringLiteral("short-iso"),
              QStringLiteral("--no-pager"),
              QStringLiteral("-n"),
              QStringLiteral("50") },
            3000);
        if (!bytes.isEmpty()) {
            // Reverse so newest first — journalctl prints chronological order.
            QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'),
                                                              Qt::SkipEmptyParts);
            std::reverse(lines.begin(), lines.end());
            recent = OomdInfoParser::parseOomdJournalLines(lines, 16);
        }
    }

    OomdSnapshot snap = OomdInfoParser::assembleSnapshot(
        cgroupV2, systemctlProps, memoryEvents, recent);

    QMutexLocker locker(&mMutex);
    mSnapshot = snap;
}

OomdSnapshot OomdInfoLinux::getSnapshot() const
{
    QMutexLocker locker(&mMutex);
    return mSnapshot;
}

bool OomdInfoLinux::hasOomd() const
{
    QMutexLocker locker(&mMutex);
    return mSnapshot.available;
}
