#include "wol_widget.h"
#include <QStyle>

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Managers/setting_manager.h>
#include <Utils/command_util.h>
#include <Utils/file_util.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolButton>
#include <QSettings>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThreadPool>
#include <QUdpSocket>
#include <QVBoxLayout>

namespace {

QList<WolHost> discoverHosts()
{
    QList<WolHost> hosts;

#ifdef Q_OS_LINUX
    const QString arp = FileUtil::readStringFromFile(QStringLiteral("/proc/net/arp"));
    static const QRegularExpression re(
        QStringLiteral(R"(^(\d+\.\d+\.\d+\.\d+)\s+\S+\s+0x2\s+([0-9a-fA-F:]{17}))"),
        QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = re.globalMatch(arp);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        const QString mac = m.captured(2).toLower();
        if (mac == QLatin1String("00:00:00:00:00:00"))
            continue;
        WolHost h;
        h.ip  = m.captured(1);
        h.mac = mac;
        hosts << h;
    }
#else
    ExecResult r = CommandUtil::execWithStatus(QStringLiteral("arp"), {QStringLiteral("-a")});
    if (r.ok()) {
        static const QRegularExpression re(
            QStringLiteral(R"(\((\d+\.\d+\.\d+\.\d+)\) at ([0-9a-fA-F:]{17}))"));
        QRegularExpressionMatchIterator it = re.globalMatch(r.output);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            WolHost h;
            h.ip  = m.captured(1);
            h.mac = m.captured(2).toLower();
            hosts << h;
        }
    }
#endif

    return hosts;
}

QByteArray buildMagicPacket(const QString &mac)
{
    // Parse MAC: "aa:bb:cc:dd:ee:ff"
    const QStringList parts = mac.split(':');
    if (parts.size() != 6)
        return {};

    QByteArray macBytes;
    for (const QString &p : parts) {
        bool ok = false;
        macBytes.append(static_cast<char>(p.toUInt(&ok, 16)));
        if (!ok)
            return {};
    }

    QByteArray packet;
    packet.fill('\xff', 6);           // 6 x 0xFF header
    for (int i = 0; i < 16; ++i)
        packet.append(macBytes);      // 16 x MAC

    return packet;
}

} // namespace

WolWidget::WolWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &WolWidget::hostsFetched, this, &WolWidget::onHostsFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &WolWidget::refreshThemeColors);
    refreshThemeColors();
}

void WolWidget::loadIfNeeded()
{
    if (!mLoaded) {
        mLoaded = true;
        loadNames();
    }
}

void WolWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("Wake-on-LAN"), this);
    mLblTitle->setProperty("textRole", "panelTitle");
    root->addWidget(mLblTitle);

    auto *intro = new QLabel(
        tr("Discover hosts in the local ARP cache, assign friendly names, "
           "and wake them with a magic packet (UDP port 9). "
           "Wake-on-LAN must be enabled in the target machine's firmware."),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    mCard = new QFrame(this);
    mCard->setObjectName("wolCard");
    auto *cardLayout = new QVBoxLayout(mCard);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(10);

    mTable = new QTableWidget(0, 4, mCard);
    mTable->setHorizontalHeaderLabels({tr("IP"), tr("MAC"), tr("Friendly Name"), tr("Action")});
    mTable->horizontalHeader()->setStretchLastSection(false);
    mTable->horizontalHeader()->setSectionResizeMode(ColIp,   QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(ColMac,  QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(ColWake, QHeaderView::Fixed);
    mTable->setColumnWidth(ColWake, 64);
    mTable->verticalHeader()->setVisible(false);
    mTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mTable->setSelectionMode(QAbstractItemView::NoSelection);
    mTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
    mTable->setMinimumHeight(120);
    connect(mTable, &QTableWidget::itemChanged,
            this,   &WolWidget::onItemChanged);
    cardLayout->addWidget(mTable);

    mLblStatus = new QLabel(mCard);
    mLblStatus->setWordWrap(true);
    mLblStatus->hide();
    cardLayout->addWidget(mLblStatus);

    auto *btnRow = new QHBoxLayout;
    mBtnDiscover = new QPushButton(tr("Discover Hosts"), mCard);
    mBtnDiscover->setProperty("variant", "primary");
    mBtnDiscover->setCursor(Qt::PointingHandCursor);
    connect(mBtnDiscover, &QPushButton::clicked, this, &WolWidget::onDiscoverClicked);
    btnRow->addWidget(mBtnDiscover);
    btnRow->addStretch();
    cardLayout->addLayout(btnRow);

    root->addWidget(mCard);
    root->addStretch();
}

void WolWidget::onDiscoverClicked()
{
    mBtnDiscover->setEnabled(false);
    mBtnDiscover->setText(tr("Scanning…"));
    mLblStatus->hide();

    QThreadPool::globalInstance()->start([this]() {
        QList<WolHost> hosts = discoverHosts();
        emit hostsFetched(hosts);
    });
}

void WolWidget::onHostsFetched(QList<WolHost> hosts)
{
    mBtnDiscover->setEnabled(true);
    mBtnDiscover->setText(tr("Discover Hosts"));

    // Attach saved friendly names
    for (WolHost &h : hosts) {
        if (mFriendlyNames.contains(h.mac))
            h.friendlyName = mFriendlyNames.value(h.mac);
    }

    populateTable(hosts);

    if (hosts.isEmpty()) {
        setStatusRole(QStringLiteral("warning"));
        mLblStatus->setText(tr("No hosts found in ARP cache. Try pinging devices on your network first."));
        mLblStatus->show();
    } else {
        mLblStatus->hide();
    }
}

void WolWidget::populateTable(const QList<WolHost> &hosts)
{
    mIgnoreItemChanged = true;
    mTable->setRowCount(0);

    for (const WolHost &h : hosts) {
        const int row = mTable->rowCount();
        mTable->insertRow(row);

        auto makeReadOnly = [](const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            return item;
        };

        mTable->setItem(row, ColIp,  makeReadOnly(h.ip));
        mTable->setItem(row, ColMac, makeReadOnly(h.mac));

        auto *nameItem = new QTableWidgetItem(h.friendlyName);
        nameItem->setData(Qt::UserRole, h.mac);
        mTable->setItem(row, ColName, nameItem);

        auto *wakeBtn = new QToolButton(mTable);
        wakeBtn->setText(QStringLiteral("⚡"));
        wakeBtn->setToolTip(tr("Send magic packet to %1").arg(h.mac));
        wakeBtn->setAutoRaise(true);
        wakeBtn->setCursor(Qt::PointingHandCursor);
        const QString mac = h.mac;
        connect(wakeBtn, &QToolButton::clicked, this, [this, mac] {
            sendMagicPacket(mac);
            setStatusRole(QStringLiteral("success"));
            mLblStatus->setText(tr("Magic packet sent to %1.").arg(mac));
            mLblStatus->show();
        });
        mTable->setCellWidget(row, ColWake, wakeBtn);
    }

    mIgnoreItemChanged = false;
}

void WolWidget::onItemChanged(QTableWidgetItem *item)
{
    if (mIgnoreItemChanged || !item || item->column() != ColName)
        return;

    const QString mac  = item->data(Qt::UserRole).toString();
    const QString name = item->text();
    if (mac.isEmpty())
        return;

    if (name.isEmpty())
        mFriendlyNames.remove(mac);
    else
        mFriendlyNames[mac] = name;

    saveNames();
}

void WolWidget::sendMagicPacket(const QString &mac)
{
    const QByteArray packet = buildMagicPacket(mac);
    if (packet.isEmpty())
        return;

    QUdpSocket socket;
    socket.writeDatagram(packet, QHostAddress::Broadcast, 9);
}

void WolWidget::saveNames()
{
    QJsonObject obj;
    for (auto it = mFriendlyNames.cbegin(); it != mFriendlyNames.cend(); ++it)
        obj.insert(it.key(), it.value());
    SettingManager::ins()->setWolHostNames(
        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void WolWidget::loadNames()
{
    const QString json = SettingManager::ins()->getWolHostNames();
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    mFriendlyNames.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        mFriendlyNames.insert(it.key(), it.value().toString());
}

// [status="…"] selectors in style.qss follow the theme on their own; a
// dynamic property needs an explicit re-polish (BUG-56).
void WolWidget::setStatusRole(const QString &role)
{
    mLblStatus->setProperty("status", role);
    mLblStatus->style()->unpolish(mLblStatus);
    mLblStatus->style()->polish(mLblStatus);
}

void WolWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    const QString cardBg    = sv->value("@cardBgElevated",      "#ffffff").toString();
    const QString borderCol = sv->value("@borderColor", "#e0e0e0").toString();

    mCard->setStyleSheet(
        QStringLiteral("QFrame#wolCard{"
                       "background-color:%1;"
                       "border:1px solid %2;"
                       "border-radius: 12px;}")
            .arg(cardBg, borderCol));
}
