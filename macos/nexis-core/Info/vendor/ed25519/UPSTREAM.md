# Vendored: orlp/ed25519 (verify-only subset)

- **Source:** https://github.com/orlp/ed25519
- **Pinned commit:** `b1f19fab4aebe607805620d25a5e42566ce46a0e` (2022-10-02, GPG-signed by upstream author)
- **License:** zlib (`LICENSE.txt`, GPLv3-compatible)

## Why this library

Sparkle 2.x itself uses `orlp/ed25519` for appcast signature verification, so
Nexis's verifier and the appcasts it checks are exercised against the same
implementation. See SSO-15431 for the full rationale: macOS `SecKey` has no
EdDSA support (RSA/ECDSA/ECDH only — the code this replaces referenced
constants that do not exist in any Apple SDK), and a CryptoKit Swift shim was
considered but rejected because this build has no existing Swift/CMake
language integration to build or verify one against.

## Scope

Only the files needed for `ed25519_verify()` are vendored — signing, key
generation, key exchange, and seed generation (`sign.c`, `keypair.c`,
`add_scalar.c`, `key_exchange.c`, `seed.c`) are upstream files this verifier
never calls and are intentionally not copied in, per Build vs Buy / Blast
Radius.

Every vendored file is byte-identical to the pinned commit — no local edits.
Do not hand-edit these files; re-vendor from upstream instead.

## Manifest (sha256)

```
f68d76b9c1c2271422e55134ea94cf71f2ac3fc3ed6a7ec6b83a1a0538753214  LICENSE.txt
8b09800605ac592c1f7d6e701f3526f84156db15bbd68904d39ad398e7d7c8ce  ed25519.h
71082937da43def6ecaf7811c23410bd8ea763969dfec67383f8129b12b1926d  fe.c
0476ae2ebb86978d1e0b9fbeb63dcddd7566d6b74806db47dda7688df74aaafb  fe.h
451fd529865e7fc69156f4cbe7c548857704ae4913768ad9ed7103656efdfc17  fixedint.h
c3bb834817edea83d843828a75af19867b93144d15140eadfd9784d40cf11409  ge.c
b16be2ea1731d9212933e7beeaef6882145fdcdaebdff9996efb5cf97faa2651  ge.h
c7d63a8b7ecd7bc6a3ac2b826a540a69dee0fc7e8bf3b00ef5dd98ce00a7c224  precomp_data.h
c1bdb840416b34e79ceb7472d1a4ac4b1407c47b4c982cad981c72142ef42407  sc.c
b0ef537b4d4dad7bd2910e2a663a02a7ac25b473511a929c3c609d82f9bdf87d  sc.h
b34128b950036777c299943d624bd830a234a65932c394fdf455e4948258fb6d  sha512.c
647ed5784d4dcd682b9c42add2e39107582efb4df9e1362c8e5120613eb9a0ce  sha512.h
a2ca7197c5f6d193c9f526d432429dfb44a65853a7ec18e10a5761334bdcf48c  verify.c
```

Verify with `sha256sum -c` from this directory.
