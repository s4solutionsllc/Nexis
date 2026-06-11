#ifndef COMMAND_UTIL_H
#define COMMAND_UTIL_H

#include <QFuture>
#include <QStringList>

#include "nexis-core_global.h"

// Unified result type for all CommandUtil entry points. Callers that need to
// distinguish failure from "ran but produced no output" should branch on
// `ok()`/`exitCode`; callers that just want the trimmed stdout can read
// `output` directly.
struct NEXISCORESHARED_EXPORT ExecResult {
    QString output;
    QString error;
    int exitCode = -1;

    // True only if the child reached normal exit with code 0 and QProcess
    // reported no start/run error. Convenience used at the call site.
    bool ok() const { return exitCode == 0; }
};

class NEXISCORESHARED_EXPORT CommandUtil
{
public:
    // -----------------------------------------------------------------------
    // Authoritative non-throwing API (SSO-3367 / audit A1).
    //
    // All four entry points share one contract:
    //   * No path throws.
    //   * Failure is reported via ExecResult: exitCode != 0 and `error`
    //     populated (QProcess errorString and/or captured stderr).
    //   * On success, `output` is the trimmed stdout, `exitCode == 0`.
    //
    // Privileged variants accept the platform-specific bypass seam
    // NEXIS_SUDO_BYPASS=1, which routes the call through execWithStatus
    // without pkexec/osascript so unit tests can exercise the path.
    // WI-21 (SSO-3383): callers invoking a polkit/osascript prompt should
    // pass a much larger timeoutMs (5+ minutes) so a slow password entry
    // does not race the 30 s cap. Use -1 to wait indefinitely.
    // -----------------------------------------------------------------------
    static ExecResult execWithStatus(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);
    static ExecResult execWithStatus(const QString &cmd, QStringList args, QByteArray data, int timeoutMs = 30000);
    static ExecResult sudoExecWithStatus(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);
    static QFuture<ExecResult> execAsync(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);

    // -----------------------------------------------------------------------
    // Legacy QString-returning entry points.
    //
    // Kept stable for the ~130 existing call sites; they delegate to the
    // *WithStatus variants above. Behaviour change from the pre-SSO-3367
    // contract: exec() no longer throws on QProcess error — it logs the
    // failure via qCritical and returns an empty QString (same outward shape
    // sudoExec already had). Per-subsystem migration to the *WithStatus
    // API is tracked under the SSO-3367 follow-up issues.
    // -----------------------------------------------------------------------
    static QString sudoExec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);
    static QString exec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);

    static bool isExecutable(const QString &cmd);
};

#endif // COMMAND_UTIL_H
