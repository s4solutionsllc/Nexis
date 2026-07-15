#include "network_usage_page.h"

#include "Managers/app_manager.h"
#include "Managers/data_refresh_service.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"

#include "net_usage_tracker.h"
#include "utilities.h"
#include <Utils/format_util.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QSettings>
#include <QScrollArea>
#include <QSizePolicy>
#include <QFont>
#include <QNetworkInterface>
#include <algorithm>

// ── BarChartWidget ────────────────────────────────────────────────────────────

class BarChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setData(const QList<DailyBucket> &buckets)
    {
        mBuckets = buckets;
        update();
    }

    void setColor(QColor rx, QColor tx)
    {
        mRxColor = rx;
        mTxColor = tx;
        update();
    }

    // DS §6: static plot chrome (background fill + gridlines), painted even
    // when there is no history yet — matches the approved empty capture.
    void setChrome(QColor background, QColor grid)
    {
        mBgColor = background;
        mGridColor = grid;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        const int w = width();
        const int h = height();
        const int labelH = 18;
        const int chartH = h - labelH;

        p.fillRect(0, 0, w, chartH, mBgColor);
        p.setPen(mGridColor);
        for (int i = 1; i < 4; ++i) {
            const int y = chartH * i / 4;
            p.drawLine(0, y, w, y);
        }
        for (int i = 1; i < 4; ++i) {
            const int x = w * i / 4;
            p.drawLine(x, 0, x, chartH);
        }

        if (mBuckets.isEmpty())
            return;

        const int n = mBuckets.size();
        const int barW = qMax(2, (w - (n + 1)) / n);
        const int gap = qMax(1, (w - barW * n) / (n + 1));

        quint64 maxVal = 1;
        for (const auto &b : mBuckets)
            maxVal = qMax(maxVal, b.rx + b.tx);

        for (int i = 0; i < n; ++i) {
            const DailyBucket &b = mBuckets.at(i);
            const quint64 total = b.rx + b.tx;
            const int barH = static_cast<int>(static_cast<double>(total) / maxVal * (chartH - 4));

            const int x = gap + i * (barW + gap);
            const int rxH = (total > 0)
                ? static_cast<int>(static_cast<double>(b.rx) / total * barH)
                : 0;
            const int txH = barH - rxH;

            // TX (upload) on top, RX (download) on bottom
            if (txH > 0) {
                p.fillRect(x, chartH - barH, barW, txH, mTxColor);
            }
            if (rxH > 0) {
                p.fillRect(x, chartH - rxH, barW, rxH, mRxColor);
            }

            // Date label every 7 days
            if (i % 7 == 0) {
                p.setPen(QColor(128, 128, 128));
                QFont f = p.font();
                f.setPointSize(7);
                p.setFont(f);
                p.drawText(x, chartH + 1, barW * 3, labelH,
                           Qt::AlignLeft | Qt::AlignVCenter,
                           b.date.toString("d MMM"));
            }
        }
    }

private:
    QList<DailyBucket> mBuckets;
    QColor mRxColor = QColor("#5294e2");
    QColor mTxColor = QColor("#5294e2").lighter(140);
    QColor mBgColor = QColor("#2A2C32");
    QColor mGridColor = QColor("#3A3D4A");
};

#include "network_usage_page.moc"

// ── Helpers ───────────────────────────────────────────────────────────────────

// DS §2 (NEX F1): opts a container into the shared elevated-card QSS recipe
// (style.qss [cardRole="elevated"]) and gives it the DS §7 container-level
// drop shadow. Never call per-row/child — shadows live on the container.
static void makeElevated(QFrame *card)
{
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(card, 90, 26);
}

// DS §3 (NEX F2): shared section-header row (style.qss #sectionHeaderRow /
// #sectionHeaderAccent / #sectionHeaderTitle / #sectionHeaderSource) — an
// accent bar plus a title (and optional muted source line) below it.
// `compact` picks the shorter card-header bar instead of the page-header bar.
static QWidget *makeSectionHeader(const QString &title, bool compact,
                                  const QString &source, QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setObjectName("sectionHeaderRow");
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto *accent = new QFrame(row);
    accent->setObjectName("sectionHeaderAccent");
    accent->setFrameShape(QFrame::NoFrame);
    accent->setFixedWidth(3);
    accent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    accent->setProperty("accentToken", "network");
    if (compact)
        accent->setProperty("compact", true);
    rowLayout->addWidget(accent);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(0);
    auto *titleLbl = new QLabel(title, row);
    titleLbl->setObjectName("sectionHeaderTitle");
    textCol->addWidget(titleLbl);
    if (!source.isEmpty()) {
        auto *sourceLbl = new QLabel(source, row);
        sourceLbl->setObjectName("sectionHeaderSource");
        textCol->addWidget(sourceLbl);
    }
    rowLayout->addLayout(textCol);

    return row;
}

static QFrame *makeSummaryCard(const QString &title, QLabel *&valueOut, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("netUsageSummaryCard");
    makeElevated(card);
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(2);

    auto *lbl = new QLabel(title, card);
    lbl->setObjectName("netUsageCardTitle");
    lay->addWidget(lbl);

    valueOut = new QLabel(QStringLiteral("—"), card);
    valueOut->setObjectName("netUsageCardValue");
    QFont f = valueOut->font();
    f.setPointSize(f.pointSize() + 3);
    f.setBold(true);
    valueOut->setFont(f);
    lay->addWidget(valueOut);

    return card;
}

// ── NetworkUsagePage ──────────────────────────────────────────────────────────

NetworkUsagePage::NetworkUsagePage(QWidget *parent, DataRefreshService *drs)
    : NexisPage(parent),
      mDrs(drs ? drs : DataRefreshService::ins())
{
    buildUI();
    refreshThemeColors();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &NetworkUsagePage::refreshThemeColors);
    connect(NetUsageTracker::ins(), &NetUsageTracker::dataChanged,
            this, &NetworkUsagePage::onDataChanged);
}

void NetworkUsagePage::buildUI()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background-color:transparent;}");

    auto *container = new QWidget(scroll);
    container->setStyleSheet("background-color:transparent;");
    scroll->setWidget(container);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);

    auto *lay = new QVBoxLayout(container);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    // ── Page header (DS §3): accent bar + "Network Usage" title + source
    // line; interface selector stays top-right as an unchanged control. ──
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeSectionHeader(tr("Network Usage"), false,
                                          tr("Per-interface throughput and data usage"),
                                          container));
    titleRow->addStretch();

    mIfaceCombo = new QComboBox(container);
    mIfaceCombo->setMinimumWidth(140);
    connect(mIfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkUsagePage::onIfaceChanged);
    titleRow->addWidget(mIfaceCombo);
    lay->addLayout(titleRow);

    // ── Live rates ──
    mRateCard = new QFrame(container);
    mRateCard->setObjectName("netUsageRateCard");
    makeElevated(mRateCard);
    auto *rateL = new QHBoxLayout(mRateCard);
    rateL->setContentsMargins(12, 8, 12, 8);

    auto *downIcon = new QLabel(QStringLiteral("↓"), mRateCard);
    downIcon->setObjectName("netUsageRateIcon");
    mLblRateDown = new QLabel(QStringLiteral("— /s"), mRateCard);
    mLblRateDown->setObjectName("netUsageRateVal");
    auto *upIcon = new QLabel(QStringLiteral("↑"), mRateCard);
    upIcon->setObjectName("netUsageRateIcon");
    mLblRateUp = new QLabel(QStringLiteral("— /s"), mRateCard);
    mLblRateUp->setObjectName("netUsageRateVal");

    rateL->addWidget(downIcon);
    rateL->addWidget(mLblRateDown);
    rateL->addSpacing(24);
    rateL->addWidget(upIcon);
    rateL->addWidget(mLblRateUp);
    rateL->addStretch();
    lay->addWidget(mRateCard);

    // ── Summary cards ──
    auto *summaryRow = new QHBoxLayout;
    summaryRow->setSpacing(12);
    summaryRow->addWidget(makeSummaryCard(tr("Today"),      mLblTodayVal, container));
    summaryRow->addWidget(makeSummaryCard(tr("This Week"),  mLblWeekVal,  container));
    summaryRow->addWidget(makeSummaryCard(tr("This Month"), mLblMonthVal, container));
    lay->addLayout(summaryRow);

    // ── 30-day bar chart ──
    mChartCard = new QFrame(container);
    mChartCard->setObjectName("netUsageChartCard");
    makeElevated(mChartCard);
    auto *chartL = new QVBoxLayout(mChartCard);
    chartL->setContentsMargins(14, 12, 14, 12);
    chartL->setSpacing(8);

    chartL->addWidget(makeSectionHeader(tr("30-Day History (↓ Download  ↑ Upload)"),
                                        true, QString(), mChartCard));

    mBarChart = new BarChartWidget(mChartCard);
    chartL->addWidget(mBarChart);
    lay->addWidget(mChartCard);

    // ── Cap section ──
    mCapCard = new QFrame(container);
    mCapCard->setObjectName("netUsageCapCard");
    makeElevated(mCapCard);
    auto *capL = new QVBoxLayout(mCapCard);
    capL->setContentsMargins(14, 12, 14, 12);
    capL->setSpacing(8);

    capL->addWidget(makeSectionHeader(tr("Monthly Data Cap"), true, QString(), mCapCard));

    auto *capBarRow = new QHBoxLayout;
    mCapBar = new QProgressBar(mCapCard);
    mCapBar->setRange(0, 100);
    mCapBar->setTextVisible(false);
    mCapBar->setFixedHeight(14);
    capBarRow->addWidget(mCapBar);
    capL->addLayout(capBarRow);

    auto *capLabelRow = new QHBoxLayout;
    mLblCapUsed = new QLabel(mCapCard);
    mLblCapUsed->setObjectName("netUsageCapLabel");
    mLblCapLimit = new QLabel(mCapCard);
    mLblCapLimit->setObjectName("netUsageCapLabel");
    mLblCapLimit->setAlignment(Qt::AlignRight);
    capLabelRow->addWidget(mLblCapUsed);
    capLabelRow->addStretch();
    capLabelRow->addWidget(mLblCapLimit);
    capL->addLayout(capLabelRow);

    lay->addWidget(mCapCard);

    // ── Settings card ──
    mSettCard = new QFrame(container);
    mSettCard->setObjectName("netUsageSettCard");
    makeElevated(mSettCard);
    auto *settL = new QGridLayout(mSettCard);
    settL->setContentsMargins(14, 12, 14, 12);
    settL->setSpacing(10);
    settL->setColumnStretch(1, 1);

    settL->addWidget(makeSectionHeader(tr("Settings"), true, QString(), mSettCard), 0, 0, 1, 2);

    settL->addWidget(new QLabel(tr("Monthly cap (GB, 0 = no cap):"), mSettCard), 1, 0);
    mSpinCap = new QSpinBox(mSettCard);
    mSpinCap->setRange(0, 100000);
    mSpinCap->setValue(SettingManager::ins()->getNetCapGB());
    mSpinCap->setSuffix(tr(" GB"));
    mSpinCap->setSpecialValueText(tr("No cap"));
    connect(mSpinCap, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkUsagePage::onCapChanged);
    settL->addWidget(mSpinCap, 1, 1);

    settL->addWidget(new QLabel(tr("Billing cycle resets on day:"), mSettCard), 2, 0);
    mSpinResetDay = new QSpinBox(mSettCard);
    mSpinResetDay->setRange(1, 28);
    mSpinResetDay->setValue(SettingManager::ins()->getNetCapResetDay());
    connect(mSpinResetDay, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkUsagePage::onResetDayChanged);
    settL->addWidget(mSpinResetDay, 2, 1);

    mChkAlert = new QCheckBox(tr("Alert at 75%, 90%, 100% of cap"), mSettCard);
    mChkAlert->setChecked(SettingManager::ins()->getNetCapAlertEnabled());
    connect(mChkAlert, &QCheckBox::toggled, this, &NetworkUsagePage::onAlertToggled);
    settL->addWidget(mChkAlert, 3, 0, 1, 2);

    lay->addWidget(mSettCard);
    lay->addStretch();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void NetworkUsagePage::onPageActivated()
{
    mActive = true;
    mHaveLastRx = false;
    mDrs->subscribe(DataRefreshService::Signal::Network);
    connect(mDrs, &DataRefreshService::networkUpdated,
            this, &NetworkUsagePage::onNetworkTick,
            Qt::UniqueConnection);

    populateIfaceCombo();
    refreshStats();
}

void NetworkUsagePage::onPageDeactivated()
{
    mActive = false;
    mDrs->unsubscribe(DataRefreshService::Signal::Network);
    disconnect(mDrs, &DataRefreshService::networkUpdated,
               this, &NetworkUsagePage::onNetworkTick);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void NetworkUsagePage::onNetworkTick(quint64 rxAbs, quint64 txAbs)
{
    if (!mHaveLastRx) {
        mLastRx = rxAbs;
        mLastTx = txAbs;
        mHaveLastRx = true;
        return;
    }

    const quint64 dRx = (rxAbs >= mLastRx) ? rxAbs - mLastRx : 0;
    const quint64 dTx = (txAbs >= mLastTx) ? txAbs - mLastTx : 0;
    mLastRx = rxAbs;
    mLastTx = txAbs;

    mLblRateDown->setText(QString("%1/s").arg(FormatUtil::formatBytes(dRx)));
    mLblRateUp->setText(QString("%1/s").arg(FormatUtil::formatBytes(dTx)));
}

void NetworkUsagePage::onDataChanged()
{
    if (!mActive)
        return;
    populateIfaceCombo();
    refreshStats();
}

void NetworkUsagePage::onIfaceChanged(int)
{
    refreshStats();
}

void NetworkUsagePage::onCapChanged(int gb)
{
    SettingManager::ins()->setNetCapGB(gb);
    // Reset last-alerted so alerts can fire again in the new cap context
    SettingManager::ins()->setNetCapAlertLastPercent(0);
    refreshCapBar();
}

void NetworkUsagePage::onResetDayChanged(int day)
{
    SettingManager::ins()->setNetCapResetDay(day);
    refreshStats();
}

void NetworkUsagePage::onAlertToggled(bool enabled)
{
    SettingManager::ins()->setNetCapAlertEnabled(enabled);
}

// ── Refresh helpers ───────────────────────────────────────────────────────────

static QString friendlyIfaceName(const QString &name)
{
    // macOS: Qt reads the display name via SystemConfiguration (e.g. "Wi-Fi", "Ethernet").
    const QNetworkInterface iface = QNetworkInterface::interfaceFromName(name);
    if (iface.isValid()) {
        const QString human = iface.humanReadableName();
        if (!human.isEmpty() && human != name)
            return QString("%1 (%2)").arg(human, name);
    }

    // Linux fallback: classify by naming convention.
    const QString lower = name.toLower();
    QString type;
    if (lower.startsWith("wl") || lower.startsWith("wifi") || lower.startsWith("ath"))
        type = QObject::tr("Wi-Fi");
    else if (lower.startsWith("en") || lower.startsWith("eth") || lower.startsWith("eno")
             || lower.startsWith("enp") || lower.startsWith("ens"))
        type = QObject::tr("Ethernet");
    else if (lower.startsWith("wwan") || lower.startsWith("rmnet") || lower.startsWith("ppp"))
        type = QObject::tr("Cellular");
    else if (lower.startsWith("tun") || lower.startsWith("utun") || lower.startsWith("tap")
             || lower.startsWith("vpn") || lower.startsWith("wg"))
        type = QObject::tr("VPN");
    else if (lower.startsWith("bridge") || lower.startsWith("br"))
        type = QObject::tr("Bridge");

    return type.isEmpty() ? name : QString("%1 (%2)").arg(type, name);
}

void NetworkUsagePage::populateIfaceCombo()
{
    const QString current = mIfaceCombo->currentData().toString();
    mIfaceCombo->blockSignals(true);
    mIfaceCombo->clear();
    mIfaceCombo->addItem(tr("All Interfaces"), NetUsageTracker::kAllInterfaces);
    for (const QString &iface : NetUsageTracker::ins()->trackedInterfaces())
        mIfaceCombo->addItem(friendlyIfaceName(iface), iface);

    int idx = mIfaceCombo->findData(current);
    mIfaceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    mIfaceCombo->blockSignals(false);
}

void NetworkUsagePage::refreshStats()
{
    const QString iface = mIfaceCombo->currentData().toString();
    const int resetDay = SettingManager::ins()->getNetCapResetDay();
    auto *tracker = NetUsageTracker::ins();

    const quint64 todayTotal = tracker->todayRx(iface) + tracker->todayTx(iface);
    const quint64 weekTotal  = tracker->weekRx(iface)  + tracker->weekTx(iface);
    const quint64 monthTotal = tracker->monthRx(iface, resetDay) + tracker->monthTx(iface, resetDay);

    mLblTodayVal->setText(FormatUtil::formatBytes(todayTotal));
    mLblWeekVal->setText(FormatUtil::formatBytes(weekTotal));
    mLblMonthVal->setText(FormatUtil::formatBytes(monthTotal));

    refreshCapBar();
    refreshBarChart();
}

void NetworkUsagePage::refreshCapBar()
{
    const int capGB = SettingManager::ins()->getNetCapGB();
    const QString iface = mIfaceCombo->currentData().toString();
    const int resetDay = SettingManager::ins()->getNetCapResetDay();

    // The Monthly Data Cap card is always one of the page's elevated bands
    // (DS §2) — it stays visible with an empty track when no cap is set,
    // matching the approved capture, rather than disappearing.
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;
    const QString track = sv->value("@chartGridColor", "#e0e0e0").toString();

    if (capGB <= 0) {
        mCapBar->setValue(0);
        mLblCapUsed->setText(tr("Used: %1").arg(FormatUtil::formatBytes(
            NetUsageTracker::ins()->monthRx(iface, resetDay) + NetUsageTracker::ins()->monthTx(iface, resetDay))));
        mLblCapLimit->setText(tr("No cap"));
        mCapBar->setStyleSheet(QString(
            "QProgressBar { border-radius: 7px; background: %1; }"
            "QProgressBar::chunk { border-radius: 7px; background-color: %1; }"
        ).arg(track));
        return;
    }

    const quint64 capBytes = static_cast<quint64>(capGB) * 1073741824ULL;
    const quint64 used = NetUsageTracker::ins()->monthRx(iface, resetDay)
                       + NetUsageTracker::ins()->monthTx(iface, resetDay);

    const int pct = static_cast<int>(qMin(100ULL, used * 100 / capBytes));
    mCapBar->setValue(pct);

    mLblCapUsed->setText(tr("Used: %1").arg(FormatUtil::formatBytes(used)));
    mLblCapLimit->setText(tr("Cap: %1 GB  (%2%)")
        .arg(capGB).arg(pct));

    QString color;
    if (pct >= 90)
        color = sv->value("@destructiveColor", "#e74c3c").toString();
    else if (pct >= 75)
        color = sv->value("@warningColor", "#e67e22").toString();
    else
        color = sv->value("@successColor", "#27ae60").toString();

    mCapBar->setStyleSheet(QString(
        "QProgressBar { border-radius: 7px; background: %1; }"
        "QProgressBar::chunk { border-radius: 7px; background-color: %2; }"
    ).arg(track, color));
}

void NetworkUsagePage::refreshBarChart()
{
    const QString iface = mIfaceCombo->currentData().toString();
    mBarChart->setData(NetUsageTracker::ins()->history(iface, 30));
}

void NetworkUsagePage::refreshThemeColors()
{
    // Card chrome (DS §2 [cardRole="elevated"]) and section headers
    // (DS §3 #sectionHeaderAccent) restyle themselves via the global QSS on
    // theme change — no per-widget stylesheet needed here (BUG-47/DS §9-1).
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString netColor = sv->value("@networkDownloadColor", "#5294e2").toString();
    const QString txColor  = sv->value("@networkUploadColor", "#7ec8e3").toString();
    mBarChart->setColor(QColor(netColor), QColor(txColor));

    const QString chartBg   = sv->value("@chartBackgroundColor", "#2A2C32").toString();
    const QString chartGrid = sv->value("@chartGridColor", "#3A3D4A").toString();
    mBarChart->setChrome(QColor(chartBg), QColor(chartGrid));

    refreshCapBar();
}
