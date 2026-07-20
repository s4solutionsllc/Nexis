#ifndef NET_ACCT_BPF_LOADER_H
#define NET_ACCT_BPF_LOADER_H

#include <QString>

#include <sys/types.h>

struct bpf_object;
struct bpf_link;

// SSO-15379: loads the net_acct.bpf.o CO-RE-free program (two kprobes on
// tcp_sendmsg/tcp_cleanup_rbuf, see ebpf/net_acct.bpf.c) and exposes per-PID
// cumulative TCP byte counters. This is the preferred per-process network
// data source; ProcessInfoLinux falls back to nethogs (NetHogsStreamer), then
// to explicit "unavailable" messaging, when this reports anything other than
// Status::Loaded.
//
// Compiled unconditionally on Linux so call sites never need `#ifdef
// NEXIS_HAVE_EBPF`; when libbpf or a BPF-target clang weren't found at
// configure time (see CMakeLists.txt), NEXIS_HAVE_EBPF is undefined and every
// method degrades to a cheap Unavailable no-op.
class NetAcctBpfLoader
{
public:
    enum class Status {
        NotLoaded,
        Loaded,
        // bpf()/attach syscalls failed with EPERM/EACCES — needs CAP_BPF (or
        // CAP_SYS_ADMIN pre-5.8) or root. Distinct from Unavailable so the UI
        // can point the user at a concrete fix instead of a generic failure.
        PermissionDenied,
        // Object file missing, kernel too old for a used feature, verifier
        // rejected the program, or NEXIS_HAVE_EBPF wasn't compiled in at all.
        Unavailable,
    };

    NetAcctBpfLoader();
    ~NetAcctBpfLoader();

    NetAcctBpfLoader(const NetAcctBpfLoader &) = delete;
    NetAcctBpfLoader &operator=(const NetAcctBpfLoader &) = delete;

    // Idempotent: the first call attempts the real load/attach; later calls
    // just return once a terminal status has been reached.
    void ensureLoaded();

    Status status() const { return mStatus; }
    QString lastError() const { return mLastError; }

    // Cumulative tx/rx byte counters observed for `pid` since this loader
    // attached. Returns false (leaving the out-params untouched) if the map
    // has no entry yet — e.g. the process hasn't done any TCP I/O.
    bool lookup(pid_t pid, quint64 *txBytes, quint64 *rxBytes) const;

private:
    Status mStatus = Status::NotLoaded;
    QString mLastError;

#ifdef NEXIS_HAVE_EBPF
    struct bpf_object *mObj = nullptr;
    struct bpf_link *mSendLink = nullptr;
    struct bpf_link *mRecvLink = nullptr;
    int mMapFd = -1;

    void teardown();
#endif
};

#endif // NET_ACCT_BPF_LOADER_H
