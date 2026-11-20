#ifndef DOC_CAPTURE_MANIFEST_H
#define DOC_CAPTURE_MANIFEST_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Documentation screenshot manifest (`nexis --capture-docs`).
//
// The manifest is a JSON document listing every screenshot the docs use:
// the output file (relative to the output root, normally the repo root), the
// page to open, the steps that put the page into the documented state, and
// the platforms the shot belongs to. The bundled manifest lives at
// :/static/doc_capture/manifest.json; --capture-manifest overrides it.
//
// This header is deliberately free of widget code so the parser and its
// validation rules can be unit-tested without an App instance.

struct DocCaptureStep
{
    enum class Type {
        Click,          // click a button by objectName or visible text
        SetText,        // set a QLineEdit / editable QComboBox text
        Wait,           // sleep `seconds`
        WaitEnabled,    // wait until widget `object` is enabled (timeout `seconds`)
        WaitUptime,     // wait until the app has been running `seconds`
        Scroll,         // scroll the page's main scroll area to `position` (0..1)
        ExpandTrees,    // expand the top level of every tree view on the page
        Sort,           // sort the page's item view by header `text`, `order`
        ShowPalette,    // open the command palette
        Key,            // send key `text` (e.g. "Escape") to the focused window
        CloseDialog,    // close the top-level dialog whose title matches `text`
        ShowColumn,     // unhide the item-view column whose header is `text`
    };

    Type type = Type::Wait;
    QString object;     // objectName to find
    QString text;       // visible button text / header text / value / key / title
    double seconds = 0; // Wait / WaitEnabled timeout / WaitUptime
    double position = 1.0;          // Scroll
    bool descending = true;         // Sort
};

struct DocCaptureShot
{
    QString file;            // output path relative to the output root, *.png
    QString page;            // App page id (navigateTo), empty = stay on current page
    QStringList platforms;   // subset of {"macos","linux"}; empty = all
    QList<DocCaptureStep> steps;   // run before capture
    QList<DocCaptureStep> after;   // run after capture (undo UI state)
    double settle = 1.5;     // seconds to wait after navigation, before steps
    QString dialog;          // non-empty: capture the top-level window with this title
    bool frame = true;       // draw window chrome + shadow around the capture
    QStringList blur;        // objectNames whose visible widgets are blurred (redaction)
    QStringList blurColumns; // item-view header labels whose visible cells are blurred
};

namespace DocCaptureManifest {

// "macos" or "linux" for the build platform.
QString currentPlatform();

// Parse and validate a manifest. Returns the shots for `platform` in file
// order. On any validation error returns an empty list and sets *error
// (when non-null) to a message naming the offending shot.
QList<DocCaptureShot> parse(const QByteArray &json, const QString &platform,
                            QString *error = nullptr);

// True when `file` is a safe relative output path: non-empty, relative, no
// ".." segment, no backslashes, and ends in ".png".
bool isSafeOutputPath(const QString &file);

// Keep only shots whose file matches one of the comma-separated wildcard
// patterns (QRegularExpression::wildcardToRegularExpression, matched against
// the full relative path and against the file name). Empty filter = all.
QList<DocCaptureShot> filter(const QList<DocCaptureShot> &shots, const QString &patterns);

} // namespace DocCaptureManifest

#endif // DOC_CAPTURE_MANIFEST_H
