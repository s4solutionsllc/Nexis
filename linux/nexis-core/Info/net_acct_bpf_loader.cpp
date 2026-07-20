#include "net_acct_bpf_loader.h"
#include "net_acct_shared.h"

#ifdef NEXIS_HAVE_EBPF
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include <cerrno>
#include <cstring>

#include <QFile>
#include <QStringList>
#endif

NetAcctBpfLoader::NetAcctBpfLoader() = default;

NetAcctBpfLoader::~NetAcctBpfLoader()
{
#ifdef NEXIS_HAVE_EBPF
    teardown();
#endif
}

#ifdef NEXIS_HAVE_EBPF
void NetAcctBpfLoader::teardown()
{
    if (mRecvLink) {
        bpf_link__destroy(mRecvLink);
        mRecvLink = nullptr;
    }
    if (mSendLink) {
        bpf_link__destroy(mSendLink);
        mSendLink = nullptr;
    }
    if (mObj) {
        bpf_object__close(mObj);
        mObj = nullptr;
    }
    mMapFd = -1;
}
#endif

void NetAcctBpfLoader::ensureLoaded()
{
    if (mStatus != Status::NotLoaded)
        return;

#ifndef NEXIS_HAVE_EBPF
    mStatus = Status::Unavailable;
    mLastError = QStringLiteral(
        "Nexis was built without eBPF support (libbpf or a BPF-target clang "
        "were not found when it was compiled)");
#else
    // NEXIS_EBPF_OBJ_PATH (set in CMakeLists.txt) is a ':'-separated search
    // list: the installed share/nexis location first, then the in-build-tree
    // path so `cmake --build` + `ctest`/manual runs work without a full
    // `cmake --install`.
    QString objPath;
    const QStringList candidates = QString::fromUtf8(NEXIS_EBPF_OBJ_PATH).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            objPath = candidate;
            break;
        }
    }

    if (objPath.isEmpty()) {
        mStatus = Status::Unavailable;
        mLastError = QStringLiteral("net_acct.bpf.o not found (searched: %1)")
                         .arg(candidates.join(QStringLiteral(", ")));
        return;
    }

    mObj = bpf_object__open_file(objPath.toUtf8().constData(), nullptr);
    if (!mObj) {
        const int err = errno;
        mStatus = (err == EPERM || err == EACCES) ? Status::PermissionDenied : Status::Unavailable;
        mLastError = QStringLiteral("bpf_object__open_file(%1) failed: %2")
                         .arg(objPath, QString::fromUtf8(strerror(err)));
        return;
    }

    int err = bpf_object__load(mObj);
    if (err) {
        const int e = -err;
        mStatus = (e == EPERM || e == EACCES) ? Status::PermissionDenied : Status::Unavailable;
        mLastError = QStringLiteral("bpf_object__load failed: %1").arg(QString::fromUtf8(strerror(e)));
        teardown();
        return;
    }

    struct bpf_program *sendProg = bpf_object__find_program_by_name(mObj, "trace_tcp_sendmsg");
    struct bpf_program *recvProg = bpf_object__find_program_by_name(mObj, "trace_tcp_cleanup_rbuf");
    if (!sendProg || !recvProg) {
        mStatus = Status::Unavailable;
        mLastError = QStringLiteral("expected BPF programs missing from net_acct.bpf.o");
        teardown();
        return;
    }

    mSendLink = bpf_program__attach(sendProg);
    if (!mSendLink) {
        const int e = errno;
        mStatus = (e == EPERM || e == EACCES) ? Status::PermissionDenied : Status::Unavailable;
        mLastError = QStringLiteral("attach tcp_sendmsg kprobe failed: %1").arg(QString::fromUtf8(strerror(e)));
        teardown();
        return;
    }

    mRecvLink = bpf_program__attach(recvProg);
    if (!mRecvLink) {
        const int e = errno;
        mStatus = (e == EPERM || e == EACCES) ? Status::PermissionDenied : Status::Unavailable;
        mLastError = QStringLiteral("attach tcp_cleanup_rbuf kprobe failed: %1").arg(QString::fromUtf8(strerror(e)));
        teardown();
        return;
    }

    struct bpf_map *map = bpf_object__find_map_by_name(mObj, NET_ACCT_MAP_NAME);
    if (!map) {
        mStatus = Status::Unavailable;
        mLastError = QStringLiteral("%1 map missing from net_acct.bpf.o").arg(NET_ACCT_MAP_NAME);
        teardown();
        return;
    }

    mMapFd = bpf_map__fd(map);
    if (mMapFd < 0) {
        mStatus = Status::Unavailable;
        mLastError = QStringLiteral("could not get fd for %1 map").arg(NET_ACCT_MAP_NAME);
        teardown();
        return;
    }

    mStatus = Status::Loaded;
#endif
}

bool NetAcctBpfLoader::lookup(pid_t pid, quint64 *txBytes, quint64 *rxBytes) const
{
#ifdef NEXIS_HAVE_EBPF
    if (mStatus != Status::Loaded || mMapFd < 0)
        return false;

    struct net_acct_val val = {};
    __u32 key = static_cast<__u32>(pid);
    if (bpf_map_lookup_elem(mMapFd, &key, &val) != 0)
        return false;

    if (txBytes) *txBytes = val.tx_bytes;
    if (rxBytes) *rxBytes = val.rx_bytes;
    return true;
#else
    Q_UNUSED(pid);
    Q_UNUSED(txBytes);
    Q_UNUSED(rxBytes);
    return false;
#endif
}
