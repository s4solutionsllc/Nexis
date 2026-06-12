#ifndef OOMD_INFO_LINUX_H
#define OOMD_INFO_LINUX_H

#include <QMutex>
#include <QString>

#include "oomd_info_parser.h"
#include "oomd_snapshot.h"

// FW-11 (SSO-3739): Linux-only systemd-oomd / cgroup v2 provider. Runs
// `systemctl show systemd-oomd.service`, reads `/sys/fs/cgroup/memory.events`,
// and parses `journalctl -u systemd-oomd.service` into an OomdSnapshot.
class OomdInfoLinux
{
public:
    // CommandRunner seam (mirrors PackageTool/RepositoryTool) so tests can
    // inject canned outputs for `systemctl show` / `journalctl` without
    // forking real processes.
    struct CommandRunner {
        virtual ~CommandRunner() = default;
        virtual QByteArray run(const QString &cmd,
                               const QStringList &args,
                               int timeoutMs) = 0;
        virtual bool exists(const QString &cmd) = 0;
    };

    explicit OomdInfoLinux(CommandRunner *runner = nullptr);
    ~OomdInfoLinux();

    void update();
    OomdSnapshot getSnapshot() const;
    bool hasOomd() const;

    // Seam for tests — overrides the path the provider uses for the unified
    // hierarchy marker check and memory.events read. Default is "/sys/fs/cgroup".
    void setCgroupRootForTesting(const QString &root);

private:
    CommandRunner *mRunner;
    bool mOwnsRunner;
    QString mCgroupRoot;

    mutable QMutex mMutex;
    OomdSnapshot mSnapshot;
};

#endif // OOMD_INFO_LINUX_H
