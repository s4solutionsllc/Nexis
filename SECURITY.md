# Security Policy

Nexis is a Linux & macOS system optimizer and monitoring tool maintained by
[S4 Solutions](https://s4solutions.ai). We take security reports seriously and
patch credible vulnerabilities out within a fixed, published SLA.

## Supported versions

Security fixes ship on the current minor release line. The latest tagged
release on `native` is always supported; the immediately preceding minor line
is supported only for backports of credible CVEs while it is the most recent
maintenance branch.

| Version  | Supported          |
| -------- | ------------------ |
| 2.3.x    | :white_check_mark: |
| < 2.3    | :x:                |

Older versions do not receive patches. Upgrade to the current release line.

## Reporting a vulnerability

**Please do not open public GitHub issues for security reports.** Use one of
the two private channels below.

1. **GitHub private vulnerability reporting (preferred).** Open the repository's
   **Security** tab and click **Report a vulnerability**. This creates a private
   draft GitHub Security Advisory (GHSA) that only maintainers can see, lets us
   credit you on publication, and threads the fix and CVE assignment together.
2. **Email.** Send the report to **security@s4solutions.ai**. Include a clear
   reproducer, the affected version (`nexis --version` or the package version),
   the platform (Linux distribution + Qt version, or macOS version), and the
   impact you observed. If you need encryption, ask in the first message and
   we will arrange a key exchange.

Please do not file a public GitHub issue, post to discussions, or share the
report on social media until we have published a fix and a coordinated GHSA.

## What to expect

We follow the expedited path documented in
[`RELEASE.md` §6](RELEASE.md#6-cve--security-fix-expedited-path):

- **Acknowledgement within 48 hours** of receipt of a credible report.
- **Patched build within 7 calendar days** of a credible disclosure. If a
  working remote exploit is in the wild, we compress to ≤72 hours.
- **Coordinated disclosure.** For multi-party issues (Qt upstream, distro
  packagers), we follow the Qt embargo timeline and tag on the embargo date,
  not before.
- **Credit.** On publication of the GHSA we credit the reporter by name or
  handle, with their permission.

## Scope

Security reports against the following are in scope:

- The Nexis application source under `shared/`, `linux/`, and `macos/`.
- The release pipeline (`.github/workflows/`) where it materially affects the
  integrity of published artifacts.
- Packaging recipes we publish (`linux/aur/`, `linux/debian/`,
  `linux/flatpak/`, the macOS bundle).

Out of scope:

- Bugs in upstream dependencies (Qt, OpenSSL through Qt, third-party
  CLIs such as `smartctl` or `flatpak`). Please report those to the
  respective upstream first. We will track the impact on Nexis once an
  upstream advisory exists.
- Issues that require physical access to an already-compromised host or
  require root to exploit a code path that already runs as root.
- Social-engineering reports against contributors or maintainers.

## Maintainer note — GitHub setting required

The "Report a vulnerability" button above requires the repository's **Private
vulnerability reporting** setting to be enabled (Settings → Code security and
analysis → Private vulnerability reporting → Enable). This is a one-time
maintainer-controlled toggle that cannot be set from a pull request; the
maintainer should enable it alongside the merge of this file.
