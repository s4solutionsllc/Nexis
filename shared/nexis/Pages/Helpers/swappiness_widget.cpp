#include "swappiness_widget.h"

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/command_util.h>
#include <Utils/file_util.h>
#include <Utils/format_util.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// Where Nexis persists swappiness across reboots.
const QString kSysctlConfPath =
    QStringLiteral("/etc/sysctl.d/99-nexis-swappiness.conf");

// Presets per FR-81.
constexpr int kPresetDesktop     = 60;
constexpr int kPresetPerformance = 10;
constexpr int kPresetLowRam      = 80;

quint64 parseMeminfoKib(const QString &content, const QString &key)
{
    static const QRegularExpression ws(R"(\s+)");
    for (const QString &line : content.split('\n')) {
        if (!line.startsWith(key))
            continue;
        const QStringList parts = line.split(ws, Qt::SkipEmptyParts);
        if (parts.size() < 2)
            return 0;
        bool ok = false;
        const quint64 kib = parts.at(1).toULongLong(&ok);
        return ok ? kib * 1024ULL : 0;
    }
    return 0;
}

int extractSwappinessFromSysctlConf(const QString &content)
{
    // Expected single-line: "vm.swappiness = N"
    static const QRegularExpression re(R"(^\s*vm\.swappiness\s*=\s*(\d+)\s*$)",
                                        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch m = re.match(content);
    if (!m.hasMatch())
        return -1;
    bool ok = false;
    const int v = m.captured(1).toInt(&ok);
    return ok ? v : -1;
}

} // namespace

SwappinessWidget::SwappinessWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &SwappinessWidget::statusFetched,
            this, &SwappinessWidget::onStatusFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &SwappinessWidget::refreshThemeColors);
    refreshThemeColors();
}

void SwappinessWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void SwappinessWidget::refresh()
{
    mLoaded = true;
    mLblLoading->show();
    mBtnApply->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this]() {
        SwappinessStatus s = fetchStatus();
        emit statusFetched(s);
    });
}

SwappinessStatus SwappinessWidget::fetchStatus()
{
    SwappinessStatus s;

#ifdef Q_OS_LINUX
    const QString raw = FileUtil::readStringFromFile("/proc/sys/vm/swappiness").trimmed();
    if (raw.isEmpty()) {
        s.errorMsg = tr("/proc/sys/vm/swappiness is not readable on this system.");
        return s;
    }
    bool ok = false;
    s.currentValue = raw.toInt(&ok);
    if (!ok || s.currentValue < 0) {
        s.errorMsg = tr("Failed to parse swappiness value \"%1\".").arg(raw);
        return s;
    }
    s.available = true;

    const QString meminfo = FileUtil::readStringFromFile("/proc/meminfo");
    const quint64 total = parseMeminfoKib(meminfo, QStringLiteral("SwapTotal:"));
    const quint64 free  = parseMeminfoKib(meminfo, QStringLiteral("SwapFree:"));
    s.swapTotalBytes = total;
    s.swapUsedBytes  = (total >= free) ? (total - free) : 0;

    const QString swaps = FileUtil::readStringFromFile("/proc/swaps");
    const QStringList swapLines = swaps.split('\n', Qt::SkipEmptyParts);
    // First line is the header.
    s.swapBackendCount = qMax(0, swapLines.size() - 1);

    if (QFileInfo::exists(kSysctlConfPath)) {
        s.persisted = true;
        const QString conf = FileUtil::readStringFromFile(kSysctlConfPath);
        s.persistedValue = extractSwappinessFromSysctlConf(conf);
    }
#else
    s.errorMsg = tr("Swappiness tuning is a Linux-only feature.");
#endif

    return s;
}

bool SwappinessWidget::applySwappiness(int value, bool persist)
{
#ifdef Q_OS_LINUX
    // Temporary change — verify via re-read.
    CommandUtil::sudoExec("sysctl",
        {QStringLiteral("-w"), QStringLiteral("vm.swappiness=%1").arg(value)});

    const QString afterRaw =
        FileUtil::readStringFromFile("/proc/sys/vm/swappiness").trimmed();
    bool ok = false;
    const int after = afterRaw.toInt(&ok);
    if (!ok || after != value)
        return false;

    // Optional persistence.
    if (persist) {
        const QByteArray body =
            QStringLiteral("# Written by Nexis (FR-81).\nvm.swappiness = %1\n")
                .arg(value).toUtf8();
        if (!FileUtil::writeRootFile(kSysctlConfPath, body))
            return false;
    } else if (QFileInfo::exists(kSysctlConfPath)) {
        // User turned persistence off — remove the file.
        CommandUtil::sudoExec("rm", {kSysctlConfPath});
        if (QFileInfo::exists(kSysctlConfPath))
            return false;
    }
    return true;
#else
    Q_UNUSED(value)
    Q_UNUSED(persist)
    return false;
#endif
}

void SwappinessWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    // Header
    mLblTitle = new QLabel(tr("Swappiness"), this);
    mLblTitle->setObjectName("swappinessTitle");
    QFont titleFont = mLblTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    mLblTitle->setFont(titleFont);
    root->addWidget(mLblTitle);

    auto *intro = new QLabel(
        tr("Controls how eagerly the kernel moves idle memory pages to swap. "
           "Lower = keep more in RAM (interactive); higher = swap sooner "
           "(helpful on low-RAM systems)."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    // Detail card
    mDetailWidget = new QFrame(this);
    mDetailWidget->setObjectName("swappinessCard");
    auto *card = new QVBoxLayout(mDetailWidget);
    card->setContentsMargins(16, 16, 16, 16);
    card->setSpacing(10);

    mLblCurrent = new QLabel(mDetailWidget);
    mLblCurrent->setObjectName("swappinessCurrent");
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
        b->setFocusPolicy(Qt::NoFocus);
        presetRow->addWidget(b);
        return b;
    };
    mBtnDesktop     = mkBtn(tr("Desktop (60)"));
    mBtnPerformance = mkBtn(tr("Performance (10)"));
    mBtnLowRam      = mkBtn(tr("Low-RAM (80)"));
    mBtnCustom      = mkBtn(tr("Custom…"));
    mPresetGroup->addButton(mBtnDesktop, kPresetDesktop);
    mPresetGroup->addButton(mBtnPerformance, kPresetPerformance);
    mPresetGroup->addButton(mBtnLowRam, kPresetLowRam);
    mPresetGroup->addButton(mBtnCustom, -1);
    presetRow->addStretch();
    card->addLayout(presetRow);

    connect(mPresetGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &SwappinessWidget::onPresetClicked);

    // Custom slider row
    auto *sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(12);
    mSldCustom = new QSlider(Qt::Horizontal, mDetailWidget);
    mSldCustom->setRange(0, 100);
    mSldCustom->setTickInterval(10);
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
        mLblCustomValue->setText(QString::number(v));
    });

    // Persistence + usage footer
    mChkPersist = new QCheckBox(tr("Persist across reboots (writes %1)")
                                    .arg(kSysctlConfPath), mDetailWidget);
    mChkPersist->setCursor(Qt::PointingHandCursor);
    mChkPersist->setFocusPolicy(Qt::NoFocus);
    card->addWidget(mChkPersist);

    mLblSwapUsage = new QLabel(mDetailWidget);
    mLblSwapUsage->setObjectName("swappinessFooter");
    card->addWidget(mLblSwapUsage);

    root->addWidget(mDetailWidget);

    // Not-available label (shown when /proc/sys/vm/swappiness isn't readable)
    mLblNotAvail = new QLabel(this);
    mLblNotAvail->setWordWrap(true);
    mLblNotAvail->hide();
    root->addWidget(mLblNotAvail);

    // Controls row: Apply + Refresh + Loading + Result
    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);
    mBtnApply = new QPushButton(tr("Apply"), this);
    mBtnApply->setObjectName("swappinessApply");
    mBtnApply->setAccessibleName("primary");
    mBtnApply->setCursor(Qt::PointingHandCursor);
    mBtnApply->setFocusPolicy(Qt::NoFocus);
    mBtnApply->setEnabled(false);
    connect(mBtnApply, &QPushButton::clicked, this, &SwappinessWidget::onApplyClicked);
    actions->addWidget(mBtnApply);

    mBtnRefresh = new QPushButton(tr("Refresh"), this);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    mBtnRefresh->setFocusPolicy(Qt::NoFocus);
    connect(mBtnRefresh, &QPushButton::clicked, this, &SwappinessWidget::refresh);
    actions->addWidget(mBtnRefresh);

    mLblLoading = new QLabel(tr("Loading…"), this);
    mLblLoading->hide();
    actions->addWidget(mLblLoading);

    actions->addStretch();

    mLblResult = new QLabel(this);
    mLblResult->setObjectName("swappinessResult");
    actions->addWidget(mLblResult);

    root->addLayout(actions);
    root->addStretch();
}

void SwappinessWidget::onStatusFetched(SwappinessStatus status)
{
    mCurrent = status;
    mLblLoading->hide();
    mBtnRefresh->setEnabled(true);
    renderStatus(status);
}

void SwappinessWidget::renderStatus(const SwappinessStatus &s)
{
    if (!s.available) {
        mDetailWidget->hide();
        mLblNotAvail->setText(
            s.errorMsg.isEmpty() ? tr("Swappiness tuning isn't available.")
                                 : s.errorMsg);
        mLblNotAvail->show();
        mBtnApply->setEnabled(false);
        return;
    }

    mDetailWidget->show();
    mLblNotAvail->hide();

    mLblCurrent->setText(tr("Current value: <b>%1</b>%2")
        .arg(s.currentValue)
        .arg(s.persisted
                ? tr("  (persisted at %1)").arg(s.persistedValue)
                : QString()));

    // Swap usage footer.
    if (s.swapTotalBytes > 0) {
        mLblSwapUsage->setText(tr("Swap: %1 of %2 used  ·  %3 backend(s)")
            .arg(FormatUtil::formatBytes(s.swapUsedBytes),
                 FormatUtil::formatBytes(s.swapTotalBytes))
            .arg(s.swapBackendCount));
    } else {
        mLblSwapUsage->setText(tr("No active swap backend."));
    }

    // Select the matching preset button.
    setPreset(s.currentValue);

    mChkPersist->setChecked(s.persisted);
    mBtnApply->setEnabled(true);
}

void SwappinessWidget::setPreset(int value)
{
    int id = -1;   // Custom by default
    if (value == kPresetDesktop)     id = kPresetDesktop;
    else if (value == kPresetPerformance) id = kPresetPerformance;
    else if (value == kPresetLowRam) id = kPresetLowRam;

    if (id >= 0) {
        mPresetGroup->button(id)->setChecked(true);
        mSldCustom->hide();
        mLblCustomValue->hide();
    } else {
        mBtnCustom->setChecked(true);
        mSldCustom->setValue(value);
        mLblCustomValue->setText(QString::number(value));
        mSldCustom->show();
        mLblCustomValue->show();
    }
}

void SwappinessWidget::onPresetClicked(int preset)
{
    if (preset < 0) {
        mSldCustom->setValue(mCurrent.currentValue >= 0 ? mCurrent.currentValue
                                                        : kPresetDesktop);
        mLblCustomValue->setText(QString::number(mSldCustom->value()));
        mSldCustom->show();
        mLblCustomValue->show();
    } else {
        mSldCustom->hide();
        mLblCustomValue->hide();
    }
}

int SwappinessWidget::selectedValue() const
{
    const int id = mPresetGroup->checkedId();
    if (id >= 0)
        return id;
    return mSldCustom->value();
}

void SwappinessWidget::onApplyClicked()
{
    const int value = selectedValue();
    if (value < 0 || value > 100)
        return;

    mBtnApply->setEnabled(false);
    mBtnRefresh->setEnabled(false);
    mLblLoading->show();
    mLblResult->clear();

    const bool persist = mChkPersist->isChecked();

    QThreadPool::globalInstance()->start([this, value, persist]() {
        const bool ok = applySwappiness(value, persist);
        QMetaObject::invokeMethod(this, [this, value, ok]() {
            mLblLoading->hide();
            mBtnRefresh->setEnabled(true);
            if (ok) {
                mLblResult->setText(tr("✓ Applied swappiness = %1").arg(value));
                refresh();   // reload from disk to confirm.
            } else {
                mLblResult->setText(tr("⚠ Apply failed — did you cancel the password prompt?"));
                mBtnApply->setEnabled(true);
            }
        }, Qt::QueuedConnection);
    });
}

void SwappinessWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg     = sv->value("@cardBg").toString();
    const QString border     = sv->value("@borderColor").toString();
    const QString secondary  = sv->value("@color04").toString();
    const QString successCol = sv->value("@successColor").toString();
    const QString warnCol    = sv->value("@warningColor").toString();

    mDetailWidget->setStyleSheet(QString(
        "QFrame#swappinessCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}").arg(cardBg, border));

    mLblSwapUsage->setStyleSheet(QString("color: %1;").arg(secondary));

    // Result label — colour depends on whether it starts with ✓ or ⚠.
    const QString resultText = mLblResult->text();
    if (resultText.startsWith(QStringLiteral("✓")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(successCol));
    else if (resultText.startsWith(QStringLiteral("⚠")))
        mLblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
    else
        mLblResult->setStyleSheet(QString());
}
