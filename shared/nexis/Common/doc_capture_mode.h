#ifndef DOC_CAPTURE_MODE_H
#define DOC_CAPTURE_MODE_H

#include <QString>
#include <atomic>

// Process-wide switch for `nexis --capture-docs`. While it is on, values that
// identify the capture machine come from fixed placeholders at the source, so
// every page that renders them is safe by construction and no per-shot blur
// list has to remember them. See docs/SCREENSHOTS.md.
namespace DocCaptureMode {

inline std::atomic<bool> &activeFlag()
{
    static std::atomic<bool> flag{false};
    return flag;
}

inline bool isActive() { return activeFlag().load(); }
inline void setActive(bool active) { activeFlag().store(active); }

inline QString placeholderHostname() { return QStringLiteral("nexis-demo"); }

} // namespace DocCaptureMode

#endif // DOC_CAPTURE_MODE_H
