// SSO-3738 / FW-10: read-only row widget rendering a single sfltool dumpbtm
// record in the macOS Startup Apps page. Surfaces enabled/disabled state
// plus orphan / duplicate / Apple-managed badges; mutation of the BTM db is
// out of scope (Apple does not expose a public toggle).

#ifndef BTM_ROW_H
#define BTM_ROW_H

// Q_OS_MACOS lives in <QtGlobal>; include a Qt header above the platform
// guard so the macro is defined when the guard is evaluated. Without this,
// the first translation unit to see this header (btm_row.cpp and AUTOMOC's
// scan) processes it before any Qt include, skips the entire class
// declaration, and the macOS ARM64 link fails with undefined BtmRow symbols
// (SSO-3765). The .cpp guard relies on the same chain.
#include <QWidget>

#ifdef Q_OS_MACOS

#include <Info/btm_parser.h>

class BtmRow : public QWidget
{
    Q_OBJECT

public:
    explicit BtmRow(const BtmRecord &record, QWidget *parent = nullptr);

    QString getName() const { return mRecord.name; }
    QString getIdentifier() const { return mRecord.identifier; }

    // Filter helper: substring match against name, identifier, executable.
    bool matches(const QString &needle) const;

    static QString typeLabel(BtmRecordType type);

private:
    BtmRecord mRecord;
};

#endif // Q_OS_MACOS

#endif // BTM_ROW_H
