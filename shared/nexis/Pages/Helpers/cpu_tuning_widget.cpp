#include "cpu_tuning_widget.h"

#ifdef Q_OS_LINUX

#include "signal_mapper.h"
#include <Info/power_profile_info.h>
#include <Managers/app_manager.h>
#include <Managers/info_manager.h>
#include <Managers/setting_manager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QThreadPool>
#include <QVBoxLayout>

namespace {

QString formatMHz(quint64 kHz)
{
    if (kHz == 0)
        return QStringLiteral("—");
    return QStringLiteral("%1 MHz").arg(QString::number(kHz / 1000.0, 'f', 0));
}

} // namespace

void CpuTuningWidget::applyPersistedSettings()
{
    SettingManager *sm = SettingManager::ins();
    if (!sm->getCpuTuningPersist())
        return;

    QThreadPool::globalInstance()->start([sm]() {
        // Read the current snapshot first so we skip writes that would be
        // identical (avoids an unnecessary pkexec prompt on every launch).
        const CpuTuning::Snapshot s = CpuTuning::readSnapshot();
        if (!s.available || s.cores.isEmpty())
            return;

        const auto &first = s.cores.first();
        const quint64 wantedMin = static_cast<quint64>(sm->getCpuTuningMinFreqKHz());
        const quint64 wantedMax = static_cast<quint64>(sm->getCpuTuningMaxFreqKHz());
        if (wantedMin > 0 && wantedMax > 0
            && (wantedMin != first.scalingMinKHz || wantedMax != first.scalingMaxKHz)) {
            CpuTuning::writeFreqRange(wantedMin, wantedMax);
        }

        const bool wantedTurbo = sm->getCpuTuningTurboOn();
        if (s.turbo != CpuTuning::Turbo::Unsupported) {
            const bool have = (s.turbo == CpuTuning::Turbo::On);
            if (have != wantedTurbo)
                CpuTuning::writeTurbo(wantedTurbo);
        }
    });
}

CpuTuningWidget::CpuTuningWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &CpuTuningWidget::snapshotFetched,
            this, &CpuTuningWidget::onSnapshotFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &CpuTuningWidget::refreshThemeColors);
    refreshThemeColors();
}

void CpuTuningWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void CpuTuningWidget::refresh()
{
    mLoaded = true;
    mLblLoading->show();
    mBtnApply->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblResult->clear();

    // PPD detection runs on the main thread — it's cheap.
    mPPDBackend = false;
    if (InfoManager::ins()->hasPowerProfiles()) {
        const auto data = InfoManager::ins()->getPowerProfileData();
        mPPDBackend = (data.backend == PowerBackend::PowerProfilesDaemon);
        if (!data.conflictWarning.isEmpty())
            mLblConflict->setText(QStringLiteral("⚠ %1").arg(data.conflictWarning));
        else
            mLblConflict->clear();
    }

    QThreadPool::globalInstance()->start([this]() {
        CpuTuning::Snapshot snap = fetchSnapshot();
        emit snapshotFetched(snap);
    });
}

CpuTuning::Snapshot CpuTuningWidget::fetchSnapshot()
{
    return CpuTuning::readSnapshot();
}

void CpuTuningWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("CPU Tuning"), this);
    QFont titleFont = mLblTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    mLblTitle->setFont(titleFont);
    root->addWidget(mLblTitle);

    mLblDriver = new QLabel(this);
    mLblDriver->setObjectName("cpuTuningDriver");
    root->addWidget(mLblDriver);

    mLblConflict = new QLabel(this);
    mLblConflict->setObjectName("cpuTuningConflict");
    mLblConflict->setWordWrap(true);
    root->addWidget(mLblConflict);

    mLblPPDNotice = new QLabel(tr(
        "power-profiles-daemon is active — CPU tuning is managed by it. "
        "Stop the service to edit turbo / frequency directly."), this);
    mLblPPDNotice->setWordWrap(true);
    mLblPPDNotice->hide();
    root->addWidget(mLblPPDNotice);

    mCard = new QFrame(this);
    mCard->setObjectName("cpuTuningCard");
    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 16, 16, 16);
    card->setSpacing(10);

    // Turbo row
    auto *turboRow = new QHBoxLayout();
    mChkTurbo = new QCheckBox(tr("Turbo boost"), mCard);
    mChkTurbo->setCursor(Qt::PointingHandCursor);
    turboRow->addWidget(mChkTurbo);
    turboRow->addStretch();
    mLblTurbo = new QLabel(mCard);
    mLblTurbo->setObjectName("cpuTuningTurboState");
    turboRow->addWidget(mLblTurbo);
    card->addLayout(turboRow);

    // Governor row (whole-CPU)
    auto *govRow = new QHBoxLayout();
    govRow->setSpacing(8);
    govRow->addWidget(new QLabel(tr("Governor:")));
    mCmbGovernor = new QComboBox(mCard);
    govRow->addWidget(mCmbGovernor, 1);
    card->addLayout(govRow);

    // Min/max frequency sliders
    auto *minRow = new QHBoxLayout();
    minRow->setSpacing(8);
    minRow->addWidget(new QLabel(tr("Min:")));
    mSldMin = new QSlider(Qt::Horizontal, mCard);
    minRow->addWidget(mSldMin, 1);
    mLblMinVal = new QLabel(mCard);
    mLblMinVal->setMinimumWidth(90);
    minRow->addWidget(mLblMinVal);
    card->addLayout(minRow);

    auto *maxRow = new QHBoxLayout();
    maxRow->setSpacing(8);
    maxRow->addWidget(new QLabel(tr("Max:")));
    mSldMax = new QSlider(Qt::Horizontal, mCard);
    maxRow->addWidget(mSldMax, 1);
    mLblMaxVal = new QLabel(mCard);
    mLblMaxVal->setMinimumWidth(90);
    maxRow->addWidget(mLblMaxVal);
    card->addLayout(maxRow);

    connect(mSldMin, &QSlider::valueChanged, this, [this](int v) {
        mLblMinVal->setText(formatMHz(v));
        if (v > mSldMax->value())
            mSldMax->setValue(v);
    });
    connect(mSldMax, &QSlider::valueChanged, this, [this](int v) {
        mLblMaxVal->setText(formatMHz(v));
        if (v < mSldMin->value())
            mSldMin->setValue(v);
    });

    // Advanced (per-core governors)
    mChkAdvanced = new QCheckBox(tr("Show per-core governors"), mCard);
    mChkAdvanced->setCursor(Qt::PointingHandCursor);
    connect(mChkAdvanced, &QCheckBox::toggled,
            this, &CpuTuningWidget::onAdvancedToggled);
    card->addWidget(mChkAdvanced);

    mAdvancedPanel = new QWidget(mCard);
    mAdvancedPanel->hide();
    auto *advLayout = new QVBoxLayout(mAdvancedPanel);
    advLayout->setContentsMargins(0, 0, 0, 0);
    advLayout->setSpacing(6);
    mPerCoreGrid = new QFrame(mAdvancedPanel);
    mPerCoreGrid->setObjectName("cpuTuningPerCoreGrid");
    advLayout->addWidget(mPerCoreGrid);
    card->addWidget(mAdvancedPanel);

    // Persist toggle
    mChkPersist = new QCheckBox(
        tr("Re-apply these settings when Nexis launches"), mCard);
    mChkPersist->setCursor(Qt::PointingHandCursor);
    mChkPersist->setChecked(SettingManager::ins()->getCpuTuningPersist());
    card->addWidget(mChkPersist);

    root->addWidget(mCard);

    mLblNotAvail = new QLabel(this);
    mLblNotAvail->setWordWrap(true);
    mLblNotAvail->hide();
    root->addWidget(mLblNotAvail);

    // Action row
    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);

    mBtnApply = new QPushButton(tr("Apply"), this);
    mBtnApply->setObjectName("cpuTuningApply");
    mBtnApply->setAccessibleName("primary");
    mBtnApply->setCursor(Qt::PointingHandCursor);
    mBtnApply->setEnabled(false);
    connect(mBtnApply, &QPushButton::clicked, this, &CpuTuningWidget::onApplyClicked);
    actions->addWidget(mBtnApply);

    mBtnRefresh = new QPushButton(tr("Refresh"), this);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    connect(mBtnRefresh, &QPushButton::clicked, this, &CpuTuningWidget::refresh);
    actions->addWidget(mBtnRefresh);

    mLblLoading = new QLabel(tr("Loading…"), this);
    mLblLoading->hide();
    actions->addWidget(mLblLoading);
    actions->addStretch();

    mLblResult = new QLabel(this);
    mLblResult->setObjectName("cpuTuningResult");
    actions->addWidget(mLblResult);

    root->addLayout(actions);
    root->addStretch();
}

void CpuTuningWidget::buildPerCoreGrid(const CpuTuning::Snapshot &snap)
{
    // Teardown previous contents.
    if (auto *old = mPerCoreGrid->layout()) {
        QLayoutItem *child;
        while ((child = old->takeAt(0)) != nullptr) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }
        delete old;
    }
    mCoreGovernorCombos.clear();

    auto *grid = new QGridLayout(mPerCoreGrid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    const int cols = 4;
    for (int i = 0; i < snap.cores.size(); ++i) {
        const auto &c = snap.cores.at(i);
        auto *lbl = new QLabel(QStringLiteral("cpu%1").arg(c.index), mPerCoreGrid);
        auto *cmb = new QComboBox(mPerCoreGrid);
        cmb->addItems(snap.availableGovernors);
        if (!c.governor.isEmpty())
            cmb->setCurrentText(c.governor);
        cmb->setDisabled(mPPDBackend);
        const int row = i / cols;
        const int col = (i % cols) * 2;
        grid->addWidget(lbl, row, col);
        grid->addWidget(cmb, row, col + 1);
        mCoreGovernorCombos << cmb;
    }
}

void CpuTuningWidget::onSnapshotFetched(CpuTuning::Snapshot snap)
{
    mCurrent = snap;
    mLblLoading->hide();
    mBtnRefresh->setEnabled(true);
    renderSnapshot(snap);
}

void CpuTuningWidget::renderSnapshot(const CpuTuning::Snapshot &snap)
{
    if (!snap.available || snap.cores.isEmpty()) {
        mCard->hide();
        mLblNotAvail->setText(tr(
            "cpufreq isn't exposed on this system. CPU tuning isn't available."));
        mLblNotAvail->show();
        mBtnApply->setEnabled(false);
        return;
    }

    mCard->show();
    mLblNotAvail->hide();

    mLblDriver->setText(tr("Driver: %1").arg(
        snap.scalingDriver.isEmpty() ? tr("(unknown)") : snap.scalingDriver));

    mLblPPDNotice->setVisible(mPPDBackend);

    // Turbo
    if (snap.turbo == CpuTuning::Turbo::Unsupported) {
        mChkTurbo->setEnabled(false);
        mChkTurbo->setChecked(false);
        mLblTurbo->setText(tr("(not supported)"));
    } else {
        mChkTurbo->setEnabled(!mPPDBackend);
        mChkTurbo->setChecked(snap.turbo == CpuTuning::Turbo::On);
        mLblTurbo->setText(snap.turbo == CpuTuning::Turbo::On ? tr("ON") : tr("OFF"));
    }

    // Governor combo
    mCmbGovernor->clear();
    mCmbGovernor->addItems(snap.availableGovernors);
    if (!snap.cores.isEmpty() && !snap.cores.first().governor.isEmpty())
        mCmbGovernor->setCurrentText(snap.cores.first().governor);
    mCmbGovernor->setEnabled(!mPPDBackend);

    // Frequency sliders — use the min/max across all cores; modern desktops
    // have a single range but heterogeneous big.LITTLE can differ.
    quint64 hardMin = snap.cores.first().cpuinfoMinKHz;
    quint64 hardMax = snap.cores.first().cpuinfoMaxKHz;
    for (const auto &c : snap.cores) {
        if (c.cpuinfoMinKHz > 0)
            hardMin = qMin(hardMin, c.cpuinfoMinKHz);
        hardMax = qMax(hardMax, c.cpuinfoMaxKHz);
    }

    const bool editable = !mPPDBackend && hardMax > hardMin;
    mSldMin->setEnabled(editable);
    mSldMax->setEnabled(editable);

    if (hardMax > hardMin) {
        mSldMin->setRange(static_cast<int>(hardMin), static_cast<int>(hardMax));
        mSldMax->setRange(static_cast<int>(hardMin), static_cast<int>(hardMax));
        const quint64 currentMin = snap.cores.first().scalingMinKHz;
        const quint64 currentMax = snap.cores.first().scalingMaxKHz;
        mSldMin->setValue(static_cast<int>(currentMin));
        mSldMax->setValue(static_cast<int>(currentMax));
        mLblMinVal->setText(formatMHz(currentMin));
        mLblMaxVal->setText(formatMHz(currentMax));
    }

    buildPerCoreGrid(snap);
    mBtnApply->setEnabled(!mPPDBackend);
}

void CpuTuningWidget::onAdvancedToggled(bool on)
{
    mAdvancedPanel->setVisible(on);
}

void CpuTuningWidget::onApplyClicked()
{
    if (mPPDBackend)
        return;

    const quint64 minKHz = static_cast<quint64>(mSldMin->value());
    const quint64 maxKHz = static_cast<quint64>(mSldMax->value());
    const QString wholeGov = mCmbGovernor->currentText();
    const bool turboWanted = mChkTurbo->isChecked();
    const bool turboSupported = (mCurrent.turbo != CpuTuning::Turbo::Unsupported);

    // Assemble per-core overrides only if Advanced is on AND at least one
    // combo differs from the whole-cpu selection.
    QList<QPair<int, QString>> perCore;
    if (mChkAdvanced->isChecked() && !mCoreGovernorCombos.isEmpty()) {
        for (int i = 0; i < mCurrent.cores.size() && i < mCoreGovernorCombos.size(); ++i) {
            const QString chosen = mCoreGovernorCombos.at(i)->currentText();
            if (chosen != wholeGov)
                perCore.append(qMakePair(mCurrent.cores.at(i).index, chosen));
        }
    }

    mBtnApply->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblLoading->show();
    mLblResult->clear();
    const bool persist = mChkPersist->isChecked();

    QThreadPool::globalInstance()->start(
        [this, minKHz, maxKHz, wholeGov, turboWanted, turboSupported, perCore, persist]() {
        bool ok = true;

        // Governor (whole-CPU) first so per-core overrides on top make sense.
        if (!wholeGov.isEmpty())
            ok = CpuTuning::writeGovernor(-1, wholeGov) && ok;

        if (ok && !perCore.isEmpty())
            ok = CpuTuning::writePerCoreGovernors(perCore) && ok;

        if (ok)
            ok = CpuTuning::writeFreqRange(minKHz, maxKHz) && ok;

        if (ok && turboSupported)
            ok = CpuTuning::writeTurbo(turboWanted) && ok;

        if (ok && persist) {
            SettingManager *sm = SettingManager::ins();
            sm->setCpuTuningPersist(true);
            sm->setCpuTuningTurboOn(turboWanted);
            sm->setCpuTuningMinFreqKHz(static_cast<int>(minKHz));
            sm->setCpuTuningMaxFreqKHz(static_cast<int>(maxKHz));
        } else if (!persist) {
            SettingManager::ins()->setCpuTuningPersist(false);
        }

        QMetaObject::invokeMethod(this, [this, ok]() {
            mLblLoading->hide();
            mBtnRefresh->setEnabled(true);
            if (ok) {
                mLblResult->setText(tr("✓ Applied"));
                refresh();
            } else {
                mLblResult->setText(tr("⚠ Apply failed — did you cancel the password prompt?"));
                mBtnApply->setEnabled(true);
            }
            refreshThemeColors();
        }, Qt::QueuedConnection);
    });
}

void CpuTuningWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg     = sv->value("@cardBg").toString();
    const QString border     = sv->value("@borderColor").toString();
    const QString secondary  = sv->value("@color04").toString();
    const QString warnCol    = sv->value("@warningColor").toString();
    const QString successCol = sv->value("@successColor").toString();

    mCard->setStyleSheet(QString(
        "QFrame#cpuTuningCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}").arg(cardBg, border));
    mLblDriver->setStyleSheet(QString("color: %1;").arg(secondary));
    mLblConflict->setStyleSheet(QString("color: %1;").arg(warnCol));
    mLblPPDNotice->setStyleSheet(QString("color: %1;").arg(warnCol));

    const QString resultText = mLblResult->text();
    if (resultText.startsWith(QStringLiteral("✓")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(successCol));
    else if (resultText.startsWith(QStringLiteral("⚠")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
    else
        mLblResult->setStyleSheet(QString());
}

#endif // Q_OS_LINUX
