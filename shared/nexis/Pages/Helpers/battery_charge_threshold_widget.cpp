#include "battery_charge_threshold_widget.h"

#ifdef Q_OS_LINUX

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/file_util.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QThreadPool>
#include <QVBoxLayout>

namespace {
constexpr int kIdMaximize = BatteryChargeThreshold::kPresetMaximize;
constexpr int kIdPreserve = BatteryChargeThreshold::kPresetPreserve;
constexpr int kIdCustom   = -1;
} // namespace

BatteryChargeThresholdWidget::BatteryChargeThresholdWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &BatteryChargeThresholdWidget::statusFetched,
            this, &BatteryChargeThresholdWidget::onStatusFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &BatteryChargeThresholdWidget::refreshThemeColors);
    refreshThemeColors();
}

void BatteryChargeThresholdWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void BatteryChargeThresholdWidget::refresh()
{
    mLoaded = true;
    mLblLoading->show();
    mBtnApply->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this]() {
        ChargeThresholdStatus s = fetchStatus();
        emit statusFetched(s);
    });
}

ChargeThresholdStatus BatteryChargeThresholdWidget::fetchStatus()
{
    return BatteryChargeThreshold::readStatus();
}

void BatteryChargeThresholdWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("Battery Charge Threshold"), this);
    mLblTitle->setObjectName("chargeThresholdTitle");
    QFont f = mLblTitle->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    mLblTitle->setFont(f);
    root->addWidget(mLblTitle);

    auto *intro = new QLabel(
        tr("Limit the maximum charge level to reduce battery wear. "
           "Preserve (~80%) is recommended for laptops that stay plugged in most of the time. "
           "Persistence writes a udev rule so the threshold survives reboots."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    // Detail card
    mDetailWidget = new QFrame(this);
    mDetailWidget->setObjectName("chargeThresholdCard");
    auto *card = new QVBoxLayout(mDetailWidget);
    card->setContentsMargins(16, 16, 16, 16);
    card->setSpacing(10);

    mLblCurrent = new QLabel(mDetailWidget);
    mLblCurrent->setObjectName("chargeThresholdCurrent");
    card->addWidget(mLblCurrent);

    // Preset row
    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(8);
    mPresetGroup = new QButtonGroup(this);
    mPresetGroup->setExclusive(true);

    auto mkBtn = [&](const QString &text) {
        auto *b = new QPushButton(text, mDetailWidget);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        presetRow->addWidget(b);
        return b;
    };
    mBtnMaximize = mkBtn(tr("Maximize (100%)"));
    mBtnPreserve = mkBtn(tr("Preserve (80%)"));
    mBtnCustom   = mkBtn(tr("Custom…"));
    mPresetGroup->addButton(mBtnMaximize, kIdMaximize);
    mPresetGroup->addButton(mBtnPreserve, kIdPreserve);
    mPresetGroup->addButton(mBtnCustom,   kIdCustom);
    presetRow->addStretch();
    card->addLayout(presetRow);

    connect(mPresetGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &BatteryChargeThresholdWidget::onPresetClicked);

    // Custom slider row
    auto *sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(12);
    mSldCustom = new QSlider(Qt::Horizontal, mDetailWidget);
    mSldCustom->setRange(BatteryChargeThreshold::kMinEndThreshold, 100);
    mSldCustom->setTickInterval(5);
    mSldCustom->setSingleStep(1);
    mSldCustom->setTickPosition(QSlider::TicksBelow);
    mSldCustom->hide();
    mLblCustomValue = new QLabel(mDetailWidget);
    mLblCustomValue->setMinimumWidth(40);
    mLblCustomValue->hide();
    sliderRow->addWidget(mSldCustom, 1);
    sliderRow->addWidget(mLblCustomValue);
    card->addLayout(sliderRow);
    connect(mSldCustom, &QSlider::valueChanged, this, [this](int v) {
        mLblCustomValue->setText(tr("%1%").arg(v));
    });

    mChkPersist = new QCheckBox(
        tr("Persist across reboots (writes %1)").arg(BatteryChargeThreshold::kUdevRulesPath),
        mDetailWidget);
    mChkPersist->setCursor(Qt::PointingHandCursor);
    card->addWidget(mChkPersist);

    root->addWidget(mDetailWidget);

    mLblNotAvail = new QLabel(this);
    mLblNotAvail->setWordWrap(true);
    mLblNotAvail->hide();
    root->addWidget(mLblNotAvail);

    // Actions row
    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);
    mBtnApply = new QPushButton(tr("Apply"), this);
    mBtnApply->setObjectName("chargeThresholdApply");
    mBtnApply->setAccessibleName("primary");
    mBtnApply->setCursor(Qt::PointingHandCursor);
    mBtnApply->setEnabled(false);
    connect(mBtnApply, &QPushButton::clicked, this, &BatteryChargeThresholdWidget::onApplyClicked);
    actions->addWidget(mBtnApply);

    mBtnRefresh = new QPushButton(tr("Refresh"), this);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    connect(mBtnRefresh, &QPushButton::clicked, this, &BatteryChargeThresholdWidget::refresh);
    actions->addWidget(mBtnRefresh);

    mLblLoading = new QLabel(tr("Loading…"), this);
    mLblLoading->hide();
    actions->addWidget(mLblLoading);

    actions->addStretch();

    mLblResult = new QLabel(this);
    mLblResult->setObjectName("chargeThresholdResult");
    actions->addWidget(mLblResult);

    root->addLayout(actions);
    root->addStretch();
}

void BatteryChargeThresholdWidget::onStatusFetched(ChargeThresholdStatus status)
{
    mCurrent = status;
    mLblLoading->hide();
    mBtnRefresh->setEnabled(true);
    renderStatus(status);
}

void BatteryChargeThresholdWidget::renderStatus(const ChargeThresholdStatus &s)
{
    if (!s.available) {
        mDetailWidget->hide();
        mLblNotAvail->setText(
            s.errorMsg.isEmpty()
                ? tr("Battery charge threshold control is not supported by this hardware.")
                : s.errorMsg);
        mLblNotAvail->show();
        mBtnApply->setEnabled(false);
        return;
    }

    mDetailWidget->show();
    mLblNotAvail->hide();

    QString currentText = tr("Current end threshold: <b>%1%</b>").arg(s.endPct);
    if (s.hasStart && s.startPct >= 0)
        currentText += tr("  ·  start: <b>%1%</b>").arg(s.startPct);
    mLblCurrent->setText(currentText);

    // Select preset button matching current value
    if (s.endPct == kIdMaximize) {
        mBtnMaximize->setChecked(true);
        mSldCustom->hide();
        mLblCustomValue->hide();
    } else if (s.endPct == kIdPreserve) {
        mBtnPreserve->setChecked(true);
        mSldCustom->hide();
        mLblCustomValue->hide();
    } else {
        mBtnCustom->setChecked(true);
        mSldCustom->setValue(s.endPct);
        mLblCustomValue->setText(tr("%1%").arg(s.endPct));
        mSldCustom->show();
        mLblCustomValue->show();
    }

    mChkPersist->setChecked(QFile::exists(BatteryChargeThreshold::kUdevRulesPath));
    mBtnApply->setEnabled(true);
}

void BatteryChargeThresholdWidget::onPresetClicked(int preset)
{
    if (preset < 0) {
        // Custom — seed slider with current value
        int seed = (mCurrent.endPct >= BatteryChargeThreshold::kMinEndThreshold)
                       ? mCurrent.endPct
                       : kIdPreserve;
        mSldCustom->setValue(seed);
        mLblCustomValue->setText(tr("%1%").arg(seed));
        mSldCustom->show();
        mLblCustomValue->show();
    } else {
        mSldCustom->hide();
        mLblCustomValue->hide();
    }
}

int BatteryChargeThresholdWidget::selectedEndPct() const
{
    const int id = mPresetGroup->checkedId();
    if (id >= 0)
        return id;
    return mSldCustom->value();
}

void BatteryChargeThresholdWidget::onApplyClicked()
{
    const int endPct = selectedEndPct();
    if (endPct < BatteryChargeThreshold::kMinEndThreshold || endPct > 100)
        return;

    mBtnApply->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblLoading->show();
    mLblResult->clear();

    const bool persist  = mChkPersist->isChecked();
    const ChargeThresholdStatus snap = mCurrent;

    QThreadPool::globalInstance()->start([this, endPct, persist, snap]() {
        int startPct = snap.hasStart ? BatteryChargeThreshold::kPresetPreserveStart : -1;
        if (endPct == kIdMaximize)
            startPct = -1; // no start needed at 100%

        ChargeThresholdResult r = BatteryChargeThreshold::writeThreshold(
            snap.batteryPath, snap.hasStart && startPct >= 0, endPct, startPct);

        if (r.ok && persist) {
            const QByteArray rule =
                BatteryChargeThreshold::buildUdevRule(snap.batteryName, endPct,
                                                      snap.hasStart ? startPct : -1)
                    .toUtf8();
            FileUtil::writeRootFile(BatteryChargeThreshold::kUdevRulesPath, rule);
        } else if (r.ok && !persist && QFile::exists(BatteryChargeThreshold::kUdevRulesPath)) {
            // Remove persistence
            FileUtil::writeRootFile(BatteryChargeThreshold::kUdevRulesPath, QByteArray());
        }

        QMetaObject::invokeMethod(this, [this, endPct, r]() {
            mLblLoading->hide();
            mBtnRefresh->setEnabled(true);
            if (r.ok) {
                mLblResult->setText(tr("✓ Threshold set to %1%").arg(endPct));
                refreshThemeColors();
                refresh();
            } else {
                mLblResult->setText(
                    tr("⚠ Apply failed: %1").arg(r.errorMsg));
                refreshThemeColors();
                mBtnApply->setEnabled(true);
            }
        }, Qt::QueuedConnection);
    });
}

void BatteryChargeThresholdWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg    = sv->value("@cardBg").toString();
    const QString border    = sv->value("@borderColor").toString();
    const QString successCol = sv->value("@successColor").toString();
    const QString warnCol   = sv->value("@warningColor").toString();

    mDetailWidget->setStyleSheet(QString(
        "QFrame#chargeThresholdCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}").arg(cardBg, border));

    const QString resultText = mLblResult->text();
    if (resultText.startsWith(QStringLiteral("✓")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(successCol));
    else if (resultText.startsWith(QStringLiteral("⚠")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
    else
        mLblResult->setStyleSheet(QString());
}

#endif // Q_OS_LINUX
