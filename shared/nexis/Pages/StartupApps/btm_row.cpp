#include "btm_row.h"

#ifdef Q_OS_MACOS

#include "utilities.h"

#include <QHBoxLayout>
#include <QLabel>
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

    auto *lblName = new QLabel(displayName, this);
    lblName->setObjectName(QStringLiteral("lblBtmRowName"));

    const QString secondary = record.identifier.isEmpty()
        ? record.executablePath
        : (record.executablePath.isEmpty()
               ? record.identifier
               : QStringLiteral("%1 — %2").arg(record.identifier, record.executablePath));
    auto *lblSub = new QLabel(secondary, this);
    lblSub->setObjectName(QStringLiteral("lblBtmRowSubtext"));
    lblSub->setWordWrap(false);
    lblSub->setTextInteractionFlags(Qt::TextSelectableByMouse);

    textCol->addWidget(lblName);
    textCol->addWidget(lblSub);
    root->addLayout(textCol, 1);

    // Right column: badges.
    auto *badges = new QHBoxLayout();
    badges->setSpacing(4);

    badges->addWidget(makeBadge(typeLabel(record.type),
                                QStringLiteral("btmBadgeType"), this));

    if (record.appleManaged) {
        badges->addWidget(makeBadge(tr("Apple"),
                                    QStringLiteral("btmBadgeApple"), this));
    }
    if (record.enabled) {
        badges->addWidget(makeBadge(tr("On"),
                                    QStringLiteral("btmBadgeOn"), this));
    } else {
        badges->addWidget(makeBadge(tr("Off"),
                                    QStringLiteral("btmBadgeOff"), this));
    }
    if (record.duplicateIdentifier || record.duplicateExecutable) {
        badges->addWidget(makeBadge(tr("Duplicate"),
                                    QStringLiteral("btmBadgeDuplicate"), this));
    }
    if (record.orphan) {
        badges->addWidget(makeBadge(tr("Orphan"),
                                    QStringLiteral("btmBadgeOrphan"), this));
    }

    root->addLayout(badges, 0);

    Utilities::addDropShadow(this, 40);
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
