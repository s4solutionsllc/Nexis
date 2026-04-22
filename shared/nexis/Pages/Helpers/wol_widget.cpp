#include "wol_widget.h"

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
    try {
        const QString out = CommandUtil::exec(QStringLiteral("arp"), {QStringLiteral("-a")});
        static const QRegularExpression re(
            QStringLiteral(R"(\((\d+\.\d+\.\d+\.\d+)\) at ([0-9a-fA-F:]{17}))"));
        QRegularExpressionMatchIterator it = re.globalMatch(out);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            WolHost h;
            h.ip  = m.captured(1);
            h.mac = m.captured(2).toLower();
            hosts << h;
        }
    } catch (...) {}
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
    QFont f   = mLblTitle->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    mLblTitle->setFont(f);
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

    mTable = new QTableWidget(0, 5, mCard);
    mTable->setHorizontalHeaderLabels({tr("IP"), tr("MAC"), tr("Hostname"), tr("Friendly Name"), tr("Action")});
    mTable->horizontalHeader()->setStretchLastSection(false);
    mTable->horizontalHeader()->setSectionResizeMode(ColIp,   QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(ColMac,  QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(ColHost, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(ColWake, QHeaderView::ResizeToContents);
    mTable->verticalHeader()->setVisible(false);
    mTable->setSelectionMode(QAbstractItemView::NoSelection);
    mTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
    mTable->setMinimumHeight(120);
    mTable->setMaximumHeight(300);
    connect(mTable, &QTableWidget::itemChanged,
            this,   &WolWidget::onItemChanged);
    cardLayout->addWidget(mTable);

    mLblStatus = new QLabel(mCard);
    mLblStatus->setWordWrap(true);
    mLblStatus->hide();
    cardLayout->addWidget(mLblStatus);

    auto *btnRow = new QHBoxLayout;
    mBtnDiscover = new QPushButton(tr("Discover Hosts"), mCard);
    mBtnDiscover->setAccessibleName("primary");
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

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (hosts.isEmpty()) {
        const QString warn = sv->value("@warningColor", "#e67e22").toString();
        mLblStatus->setStyleSheet(QStringLiteral("color:%1;").arg(warn));
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

        mTable->setItem(row, ColIp,   makeReadOnly(h.ip));
        mTable->setItem(row, ColMac,  makeReadOnly(h.mac));
        mTable->setItem(row, ColHost, makeReadOnly(h.hostname));

        auto *nameItem = new QTableWidgetItem(h.friendlyName);
        nameItem->setData(Qt::UserRole, h.mac);
        mTable->setItem(row, ColName, nameItem);

        auto *wakeBtn = new QPushButton(tr("Wake"), mTable);
        wakeBtn->setCursor(Qt::PointingHandCursor);
        const QString mac = h.mac;
        connect(wakeBtn, &QPushButton::clicked, this, [this, mac, row] {
            sendMagicPacket(mac);
            QSettings *sv = AppManager::ins()->getStyleValues();
            const QString ok = sv->value("@successColor", "#27ae60").toString();
            mLblStatus->setStyleSheet(QStringLiteral("color:%1;").arg(ok));
            mLblStatus->setText(tr("Magic packet sent to %1.").arg(mac));
            mLblStatus->show();
            Q_UNUSED(row)
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

void WolWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    const QString cardBg    = sv->value("@cardBg",      "#ffffff").toString();
    const QString borderCol = sv->value("@borderColor", "#e0e0e0").toString();

    mCard->setStyleSheet(
        QStringLiteral("QFrame#wolCard{"
                       "background-color:%1;"
                       "border:1px solid %2;"
                       "border-radius:8px;}")
            .arg(cardBg, borderCol));
}
