#include "btm_row.h"

#ifdef Q_OS_MACOS

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QVBoxLayout>

namespace {

QLabel *makeBadge(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setObjectName(objectName);
    b->setProperty("badge", true);
    return b;
}

} // namespace

QString BtmRow::typeLabel(BtmRecordType type)
{
    switch (type) {
    case BtmRecordType::LegacyLoginItem: return tr("Login Item (legacy)");
    case BtmRecordType::LoginItem:       return tr("Login Item");
    case BtmRecordType::LaunchdAgent:    return tr("Launch Agent");
    case BtmRecordType::LaunchdDaemon:   return tr("Launch Daemon");
    case BtmRecordType::HelperLauncher:  return tr("Helper Launcher");
    case BtmRecordType::AppExtension:    return tr("App Extension");
    case BtmRecordType::MdmManaged:      return tr("MDM-Managed");
    case BtmRecordType::Daemon:          return tr("Daemon");
    case BtmRecordType::Unknown:         return tr("Unknown");
    }
    return tr("Unknown");
}

BtmRow::BtmRow(const BtmRecord &record, QWidget *parent)
    : QWidget(parent), mRecord(record)
{
    setObjectName(QStringLiteral("widgetBtmRow"));

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(12, 6, 12, 6);
    root->setSpacing(8);

    // Left column: name + identifier/path.
    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(2);

    const QString displayName = record.name.isEmpty()
        ? (record.identifier.isEmpty() ? tr("Unnamed BTM record") : record.identifier)
        : record.name;

    mNameFull = displayName;
    mLblName = new QLabel(displayName, this);
    mLblName->setObjectName(QStringLiteral("lblBtmRowName"));

    mSecondaryFull = record.identifier.isEmpty()
        ? record.executablePath
        : (record.executablePath.isEmpty()
               ? record.identifier
               : QStringLiteral("%1 — %2").arg(record.identifier, record.executablePath));
    mLblSub = new QLabel(mSecondaryFull, this);
    mLblSub->setObjectName(QStringLiteral("lblBtmRowSubtext"));
    mLblSub->setWordWrap(false);
    mLblSub->setTextInteractionFlags(Qt::TextSelectableByMouse);

    textCol->addWidget(mLblName);
    textCol->addWidget(mLblSub);
    root->addLayout(textCol, 1);

    // Right column: badges.
    mBadgesLayout = new QHBoxLayout();
    mBadgesLayout->setSpacing(4);

    mBadgesLayout->addWidget(makeBadge(typeLabel(record.type),
                                QStringLiteral("btmBadgeType"), this));

    if (record.appleManaged) {
        mBadgesLayout->addWidget(makeBadge(tr("Apple"),
                                    QStringLiteral("btmBadgeApple"), this));
    }
    if (record.enabled) {
        mBadgesLayout->addWidget(makeBadge(tr("On"),
                                    QStringLiteral("btmBadgeOn"), this));
    } else {
        mBadgesLayout->addWidget(makeBadge(tr("Off"),
                                    QStringLiteral("btmBadgeOff"), this));
    }
    if (record.duplicateIdentifier || record.duplicateExecutable) {
        mBadgesLayout->addWidget(makeBadge(tr("Duplicate"),
                                    QStringLiteral("btmBadgeDuplicate"), this));
    }
    if (record.orphan) {
        mBadgesLayout->addWidget(makeBadge(tr("Orphan"),
                                    QStringLiteral("btmBadgeOrphan"), this));
    }

    root->addLayout(mBadgesLayout, 0);
}

void BtmRow::setAvailableWidth(int totalWidth)
{
    if (totalWidth <= 0 || !mLblName || !mLblSub)
        return;

    const QMargins m = layout()->contentsMargins();
    const int badgesWidth = mBadgesLayout ? mBadgesLayout->sizeHint().width() : 0;
    const int reserved = m.left() + m.right() + layout()->spacing() + badgesWidth;

    // Floor so a very narrow window still leaves a few readable characters
    // instead of collapsing the elided text to just an ellipsis. Invariant:
    // sizeHint().width() never exceeds max(totalWidth, reserved + 40) — the
    // floor deliberately overrides totalWidth below that minimum usable width.
    int available = qMax(40, totalWidth - reserved);

    const QFontMetrics fmName(mLblName->font());
    const QFontMetrics fmSub(mLblSub->font());

    // QLabel::sizeHint() is derived from QFontMetrics::boundingRect(), which
    // can exceed the elidedText() advance width by a few pixels (glyph side
    // bearings) — font- and DPI-dependent. Re-elide against the measured
    // overshoot so the row's own sizeHint() honors `available`; bounded to a
    // few attempts so reflow (run per row from showEvent/resizeEvent) stays
    // O(rows) instead of O(rows * unbounded).
    for (int attempt = 0; attempt < 3; ++attempt) {
        mLblName->setText(fmName.elidedText(mNameFull, Qt::ElideMiddle, available));
        mLblSub->setText(fmSub.elidedText(mSecondaryFull, Qt::ElideMiddle, available));

        const int nameHint = mLblName->sizeHint().width();
        const int subHint = mLblSub->sizeHint().width();
        const int overshoot = qMax(nameHint, subHint) - available;
        qWarning("SSO-25051 diag: attempt=%d totalWidth=%d reserved=%d available=%d nameHint=%d subHint=%d overshoot=%d rowHint=%d",
                 attempt, totalWidth, reserved, available, nameHint, subHint, overshoot, sizeHint().width());
        if (overshoot <= 0)
            break;
        available = qMax(1, available - overshoot);
    }

    mLblName->setToolTip(mNameFull);
    mLblSub->setToolTip(mSecondaryFull);

    updateGeometry();
}

bool BtmRow::matches(const QString &needle) const
{
    if (needle.isEmpty())
        return true;
    return mRecord.name.contains(needle, Qt::CaseInsensitive)
        || mRecord.identifier.contains(needle, Qt::CaseInsensitive)
        || mRecord.executablePath.contains(needle, Qt::CaseInsensitive);
}

#endif // Q_OS_MACOS
