// SSO-3738 / FW-10: read-only row widget rendering a single sfltool dumpbtm
// record in the macOS Startup Apps page. Surfaces enabled/disabled state
// plus orphan / duplicate / Apple-managed badges; mutation of the BTM db is
// out of scope (Apple does not expose a public toggle).

#ifndef BTM_ROW_H
#define BTM_ROW_H

#include <QtGlobal>

#ifdef Q_OS_MACOS

#include <QWidget>

#include <Info/btm_parser.h>

class QHBoxLayout;
class QLabel;

class BtmRow : public QWidget
{
    Q_OBJECT

public:
    explicit BtmRow(const BtmRecord &record, QWidget *parent = nullptr);

    QString getName() const { return mRecord.name; }
    QString getIdentifier() const { return mRecord.identifier; }

    // Filter helper: substring match against name, identifier, executable.
    bool matches(const QString &needle) const;

    // SSO-25047: the page captures sizeHint() once and hands it straight to
    // QListWidgetItem::setSizeHint(), and the list's horizontal scrollbar is
    // off — so an unbounded name/subtitle sizeHint makes the row (and any
    // badges to its right) wider than the viewport with no way to reach the
    // clipped part. Call this before reading sizeHint() to elide name/subtext
    // to fit totalWidth instead.
    void setAvailableWidth(int totalWidth);

    static QString typeLabel(BtmRecordType type);

private:
    BtmRecord mRecord;
    QLabel *mLblName = nullptr;
    QLabel *mLblSub = nullptr;
    QHBoxLayout *mBadgesLayout = nullptr;
    QString mNameFull;
    QString mSecondaryFull;
};

#endif // Q_OS_MACOS

#endif // BTM_ROW_H
