#ifndef MAIL_ATTACHMENT_TOOL_H
#define MAIL_ATTACHMENT_TOOL_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

// Represents one discovered Mail attachment file.
struct MailAttachmentEntry {
    QString  path;          // absolute path to the attachment file
    QString  filename;      // display name (basename)
    qint64   size = 0;      // file size in bytes
    QDateTime modified;     // last-modified timestamp (proxy for message date)
    QString  sender;        // parsed from enclosing message envelope, empty if unavailable
    QString  subject;       // parsed from enclosing message envelope, empty if unavailable
};

// Async scan result emitted by MailAttachmentTool::scanFinished.
struct MailAttachmentScanResult {
    QList<MailAttachmentEntry> entries;
    qint64 totalSize = 0;   // sum of entry sizes
    bool   mailRunning = false; // true if Mail.app was detected as running
};

// macOS-only utility class for scanning ~/Library/Mail attachment storage.
// All public methods are safe to call from a background thread.
// Call isMacOS() before constructing on non-macOS builds (always returns false
// there; this class is compiled only on Apple platforms).
class MailAttachmentTool : public QObject
{
    Q_OBJECT

public:
    explicit MailAttachmentTool(QObject *parent = nullptr);

    // Returns true when Mail.app (com.apple.mail) is running.
    bool isMailRunning() const;

    // Scans ~/Library/Mail/V*/…/Attachments/ paths.
    // Emits scanFinished() when done. Call from a worker thread.
    void scan();

    // Deletes the given file paths. Emits progress() and deleteFinished().
    // Call from a worker thread.
    void deleteAttachments(const QStringList &paths);

signals:
    void scanFinished(const MailAttachmentScanResult &result);
    void progress(int done, int total, const QString &currentFile);
    void deleteFinished(qint64 freedBytes, int deletedCount, int failedCount);

private:
    // Attempt to parse sender/subject from the emlx envelope adjacent to
    // the Attachments/ directory. Returns false if unavailable.
    static bool parseMessageEnvelope(const QString &attachmentDirPath,
                                     QString &outSender,
                                     QString &outSubject);
};

#endif // MAIL_ATTACHMENT_TOOL_H
