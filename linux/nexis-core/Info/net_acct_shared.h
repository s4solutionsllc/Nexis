#ifndef NET_ACCT_SHARED_H
#define NET_ACCT_SHARED_H

// SSO-15379: layout shared between net_acct.bpf.c (kernel-side, compiled with
// clang -target bpf) and net_acct_bpf_loader.cpp (userspace, compiled with the
// normal host toolchain). Keep this POD and identical on both sides — it is
// the value type of the `pid_net_bytes` BPF hash map.
struct net_acct_val {
    unsigned long long tx_bytes;
    unsigned long long rx_bytes;
};

#define NET_ACCT_MAP_NAME "pid_net_bytes"

#endif // NET_ACCT_SHARED_H
