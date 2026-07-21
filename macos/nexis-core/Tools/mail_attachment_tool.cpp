#include "mail_attachment_tool.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QFile>

MailAttachmentTool::MailAttachmentTool(QObject *parent)
    : QObject(parent)
{
}

bool MailAttachmentTool::isMailRunning() const
{
    QProcess pgrep;
    pgrep.start("pgrep", QStringList() << "-x" << "Mail");
    pgrep.waitForFinished(3000);
    return pgrep.exitCode() == 0;
}

// Attachment files live under:
//   ~/Library/Mail/V<N>/[account-uuid]/[mailbox]/[message-id].emlx/Attachments/<file>
// We walk all V* subtrees and collect regular files inside any "Attachments"
// directory that is not a hidden path segment.
void MailAttachmentTool::scan()
{
    MailAttachmentScanResult result;
    result.mailRunning = isMailRunning();

    const QString mailRoot = QDir::homePath() + "/Library/Mail";
    const QDir rootDir(mailRoot);
    if (!rootDir.exists()) {
        emit scanFinished(result);
        return;
    }

    // Iterate all V* subdirs (V2, V3, V9, V10 …)
    const QStringList vDirs = rootDir.entryList(QStringList() << "V*", QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &vName : vDirs) {
        const QString vPath = mailRoot + "/" + vName;
        QDirIterator it(vPath,
                        QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            // Only files inside an "Attachments" directory segment.
            if (!filePath.contains("/Attachments/"))
                continue;

            QFileInfo fi(filePath);
            if (!fi.isFile() || fi.size() == 0)
                continue;

            MailAttachmentEntry entry;
            entry.path     = filePath;
            entry.filename = fi.fileName();
            entry.size     = fi.size();
            entry.modified = fi.lastModified();

            // Try to extract sender/subject/date from the enclosing .emlx envelope
            parseMessageEnvelope(fi.absolutePath(), entry.sender, entry.subject, entry.messageDate);

            result.entries.append(entry);
            result.totalSize += entry.size;
        }
    }

    emit scanFinished(result);
}

void MailAttachmentTool::deleteAttachments(const QStringList &paths)
{
    qint64 freed    = 0;
    int    deleted  = 0;
    int    failed   = 0;
    const int total = paths.size();

    for (int i = 0; i < total; ++i) {
        const QString &p = paths.at(i);
        emit progress(i, total, p);

        QFileInfo fi(p);
        const qint64 sz = fi.size();
        if (QFile::remove(p)) {
            freed   += sz;
            ++deleted;
        } else {
            ++failed;
        }
    }

    emit deleteFinished(freed, deleted, failed);
}

// Parse the .emlx file immediately adjacent to (or containing) the
// Attachments/ directory for From: and Subject: headers.
// The .emlx path is the parent of "Attachments/" (which is a sibling of
// the emlx itself inside the message bundle: <id>.emlx/Attachments/).
// Fallback: walk up until we find an *.emlx file.
bool MailAttachmentTool::parseMessageEnvelope(const QString &attachmentDirPath,
                                              QString &outSender,
                                              QString &outSubject,
                                              QDateTime &outDate)
{
    // attachmentDirPath is the dir containing the attachment file,
    // which is itself typically inside <message>.emlx/Attachments/
    // so the .emlx bundle dir is the grandparent.
    QDir d(attachmentDirPath);
    if (!d.cdUp())
        return false; // go up from "Attachments" dir
    // Now d should be something like <id>.emlx  (a directory on disk)
    // The actual plist/headers are in the sibling file <id>.emlx.
    // macOS Mail stores a partial .emlx alongside the directory; look for it.
    const QString emlxPath = d.absolutePath() + ".emlx";
    QFile f(emlxPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // .emlx files begin with a size integer line, then the raw RFC-2822 message
    // headers, then a NUL-terminated plist. Read only the header section.
    QTextStream ts(&f);
    // Skip the first line (byte-count).
    ts.readLine();

    static const QRegularExpression reFrom(
        "^From:\\s*(?:.*<)?([^>\\r\\n]+?)>?\\s*$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reSubject(
        "^Subject:\\s*(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reDate(
        "^Date:\\s*(.+)$",
        QRegularExpression::CaseInsensitiveOption);

    bool gotFrom    = false;
    bool gotSubject = false;
    bool gotDate    = false;
    int  lineCount  = 0;

    while (!ts.atEnd() && lineCount < 200) {
        const QString line = ts.readLine();
        ++lineCount;
        if (line.isEmpty())
            break; // end of headers

        if (!gotFrom) {
            auto m = reFrom.match(line);
            if (m.hasMatch()) {
                outSender = m.captured(1).trimmed();
                gotFrom = true;
            }
        }
        if (!gotSubject) {
            auto m = reSubject.match(line);
            if (m.hasMatch()) {
                outSubject = m.captured(1).trimmed();
                gotSubject = true;
            }
        }
        if (!gotDate) {
            auto m = reDate.match(line);
            if (m.hasMatch()) {
                const QDateTime parsed = QDateTime::fromString(m.captured(1).trimmed(), Qt::RFC2822Date);
                if (parsed.isValid()) {
                    outDate = parsed;
                    gotDate = true;
                }
            }
        }
        if (gotFrom && gotSubject && gotDate)
            break;
    }

    return gotFrom || gotSubject || gotDate;
}
