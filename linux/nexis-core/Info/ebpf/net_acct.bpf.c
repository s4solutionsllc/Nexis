// SSO-15379: per-process network byte accounting.
//
// Two kprobes aggregate bytes into a pid-keyed hash map, the same mechanism
// BCC's tcptop.py uses (tcp_sendmsg for the outbound size argument,
// tcp_cleanup_rbuf for the inbound "just copied to userspace" count). Neither
// probe reads kernel struct fields — only scalar arguments — so this needs no
// vmlinux.h/BTF CO-RE struct relocation, just plain kprobe attachment. That
// keeps the build dependency to "libbpf + a BPF-target clang", nothing more.
//
// Known limitation: TCP only, matching tcptop's own scope. UDP per-process
// accounting would need a different (uprobe/udp_sendmsg+udp_recvmsg) pair and
// is left for a follow-up if this proves useful in practice.
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "../net_acct_shared.h"

char LICENSE[] SEC("license") = "GPL";

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, __u32);
    __type(value, struct net_acct_val);
} pid_net_bytes SEC(".maps");

static __always_inline void add_bytes(__u32 pid, __u64 tx, __u64 rx)
{
    struct net_acct_val *cur = bpf_map_lookup_elem(&pid_net_bytes, &pid);
    if (cur) {
        __sync_fetch_and_add(&cur->tx_bytes, tx);
        __sync_fetch_and_add(&cur->rx_bytes, rx);
        return;
    }

    struct net_acct_val init = { .tx_bytes = tx, .rx_bytes = rx };
    bpf_map_update_elem(&pid_net_bytes, &pid, &init, BPF_ANY);
}

// int tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size)
SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(trace_tcp_sendmsg, void *sk, void *msg, size_t size)
{
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    if (pid == 0)
        return 0;

    add_bytes(pid, size, 0);
    return 0;
}

// void tcp_cleanup_rbuf(struct sock *sk, int copied)
// Called from tcp_recvmsg() right after `copied` bytes were copied to the
// calling process's buffer — the standard place tcptop-style tools sample
// inbound TCP throughput per task.
SEC("kprobe/tcp_cleanup_rbuf")
int BPF_KPROBE(trace_tcp_cleanup_rbuf, void *sk, int copied)
{
    if (copied <= 0)
        return 0;

    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    if (pid == 0)
        return 0;

    add_bytes(pid, 0, (unsigned long long)copied);
    return 0;
}
