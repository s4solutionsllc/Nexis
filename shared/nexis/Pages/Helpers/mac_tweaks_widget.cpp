#include "mac_tweaks_widget.h"

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/mac_os_version_util.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThreadPool>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString formatValue(const MacTweakDef &tweak, const QVariant &value)
{
    switch (tweak.type) {
    case MacDefaultsValueType::Bool:
        return value.toBool() ? QObject::tr("On") : QObject::tr("Off");
    case MacDefaultsValueType::Int:
        return QString::number(value.toInt());
    case MacDefaultsValueType::String:
        for (const MacTweakOption &opt : tweak.options) {
            if (opt.value == value)
                return opt.label;
        }
        return value.toString();
    }
    return value.toString();
}

} // namespace

MacTweaksWidget::MacTweaksWidget(QWidget *parent)
    : QWidget(parent)
{
    mOsVersion = MacOsVersionUtil::current();
    buildUI();
    connect(this, &MacTweaksWidget::allStatusesFetched,
            this, &MacTweaksWidget::onAllStatusesFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &MacTweaksWidget::refreshThemeColors);
    refreshThemeColors();
}

void MacTweaksWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void MacTweaksWidget::refresh()
{
    mLoaded = true;
    mLblLoading->show();

    QThreadPool::globalInstance()->start([this]() {
        QMap<QString, MacDefaultsReadResult> statuses;
        for (const MacTweakDef &tweak : MacTweaksCatalog::all())
            statuses.insert(tweak.id, MacTweaksCatalog::readCurrent(tweak));
        emit allStatusesFetched(statuses);
    });
}

void MacTweaksWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    QLabel *title = new QLabel(tr("Tweaks"), this);
    title->setObjectName("macTweaksTitle");
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    QLabel *intro = new QLabel(
        tr("Hidden macOS preferences for Finder, Dock, screenshots, animations, "
           "and the login window. Changes are applied immediately via `defaults` "
           "and verified by reading the value back."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    mSearchBox = new QLineEdit(this);
    mSearchBox->setObjectName("macTweaksSearch");
    mSearchBox->setPlaceholderText(tr("Search tweaks by name or category…"));
    mSearchBox->setClearButtonEnabled(true);
    connect(mSearchBox, &QLineEdit::textChanged, this, &MacTweaksWidget::onFilterTextChanged);
    root->addWidget(mSearchBox);

    mLblLoading = new QLabel(tr("Loading…"), this);
    mLblLoading->hide();
    root->addWidget(mLblLoading);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea{background-color:transparent;}"));

    auto *listWidget = new QWidget(scrollArea);
    listWidget->setStyleSheet(QStringLiteral("background-color:transparent;"));
    mListLayout = new QVBoxLayout(listWidget);
    mListLayout->setContentsMargins(0, 0, 0, 0);
    mListLayout->setSpacing(16);

    for (const QString &category : MacTweaksCatalog::categories()) {
        QLabel *header = new QLabel(category, listWidget);
        header->setObjectName("macTweaksCategoryHeader");
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        mListLayout->addWidget(header);
        mCategoryHeaders.insert(category, header);

        for (const MacTweakDef &tweak : MacTweaksCatalog::all()) {
            if (tweak.category != category)
                continue;
            mListLayout->addWidget(buildRow(tweak));
            mCategoryOrder[category] << tweak.id;
        }
    }
    mListLayout->addStretch();

    scrollArea->setWidget(listWidget);
    root->addWidget(scrollArea, 1);
}

QFrame *MacTweaksWidget::buildRow(const MacTweakDef &tweak)
{
    RowWidgets row;
    row.def = tweak;
    row.supported = MacTweaksCatalog::isSupported(tweak, mOsVersion);

    row.card = new QFrame(this);
    row.card->setObjectName("macTweaksRow");
    auto *cardLayout = new QVBoxLayout(row.card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(4);

    auto *topRow = new QHBoxLayout();
    QLabel *lblName = new QLabel(tweak.name, row.card);
    QFont nameFont = lblName->font();
    nameFont.setBold(true);
    lblName->setFont(nameFont);
    topRow->addWidget(lblName);
    topRow->addStretch();

    switch (tweak.type) {
    case MacDefaultsValueType::Bool:
        row.chk = new QCheckBox(row.card);
        row.chk->setCursor(Qt::PointingHandCursor);
        connect(row.chk, &QCheckBox::toggled, this, [this, id = tweak.id](bool checked) {
            const MacTweakDef *def = MacTweaksCatalog::findById(id);
            if (!def)
                return;
            const MacTweakDef captured = *def;
            const QVariant next = checked ? captured.enabledValue : captured.disabledValue;
            runWrite(id, [captured, next]() {
                return MacDefaultsTool::writeValue(captured.domain, captured.key, captured.type,
                                                    next, captured.requiresSudo, captured.killApps);
            });
        });
        topRow->addWidget(row.chk);
        break;
    case MacDefaultsValueType::Int:
        row.spin = new QSpinBox(row.card);
        // Range matches dock.tile_size, the only Int-typed tweak today.
        row.spin->setRange(16, 128);
        connect(row.spin, &QSpinBox::editingFinished, this, [this, id = tweak.id]() {
            RowWidgets &r = mRows[id];
            runWrite(id, [captured = r.def, value = r.spin->value()]() {
                return MacDefaultsTool::writeValue(captured.domain, captured.key, captured.type,
                                                    value, captured.requiresSudo, captured.killApps);
            });
        });
        topRow->addWidget(row.spin);
        break;
    case MacDefaultsValueType::String:
        if (!tweak.options.isEmpty()) {
            row.combo = new QComboBox(row.card);
            for (const MacTweakOption &opt : tweak.options)
                row.combo->addItem(opt.label, opt.value);
            connect(row.combo, &QComboBox::activated, this, [this, id = tweak.id](int index) {
                RowWidgets &r = mRows[id];
                const QVariant value = r.combo->itemData(index);
                runWrite(id, [captured = r.def, value]() {
                    return MacDefaultsTool::writeValue(captured.domain, captured.key, captured.type,
                                                        value, captured.requiresSudo, captured.killApps);
                });
            });
            topRow->addWidget(row.combo);
        } else {
            row.edit = new QLineEdit(row.card);
            row.edit->setMinimumWidth(180);
            connect(row.edit, &QLineEdit::editingFinished, this, [this, id = tweak.id]() {
                RowWidgets &r = mRows[id];
                const QVariant value = r.edit->text();
                runWrite(id, [captured = r.def, value]() {
                    return MacDefaultsTool::writeValue(captured.domain, captured.key, captured.type,
                                                        value, captured.requiresSudo, captured.killApps);
                });
            });
            topRow->addWidget(row.edit);
        }
        break;
    }

    row.btnRevert = new QToolButton(row.card);
    row.btnRevert->setText(tr("Revert"));
    row.btnRevert->setAutoRaise(true);
    row.btnRevert->setCursor(Qt::PointingHandCursor);
    row.btnRevert->setToolTip(tr("Delete the override and fall back to the system default."));
    connect(row.btnRevert, &QToolButton::clicked, this, [this, id = tweak.id]() {
        RowWidgets &r = mRows[id];
        runWrite(id, [captured = r.def]() {
            return MacTweaksCatalog::resetToDefault(captured);
        });
    });
    topRow->addWidget(row.btnRevert);
    cardLayout->addLayout(topRow);

    QLabel *lblDesc = new QLabel(tweak.description, row.card);
    lblDesc->setObjectName("macTweaksDescription");
    lblDesc->setWordWrap(true);
    cardLayout->addWidget(lblDesc);

    row.lblCurrent = new QLabel(row.card);
    row.lblCurrent->setObjectName("macTweaksCurrent");
    cardLayout->addWidget(row.lblCurrent);

    row.lblResult = new QLabel(row.card);
    row.lblResult->setObjectName("macTweaksResult");
    cardLayout->addWidget(row.lblResult);

    row.lblGated = new QLabel(row.card);
    row.lblGated->setObjectName("macTweaksGated");
    row.lblGated->setWordWrap(true);
    cardLayout->addWidget(row.lblGated);

    if (!row.supported) {
        row.lblGated->setText(tr("Requires macOS %1 or later (this system reports %2).")
                                   .arg(tweak.minOsVersion.toString(), mOsVersion.toString()));
        row.lblGated->show();
        if (row.chk) row.chk->setEnabled(false);
        if (row.combo) row.combo->setEnabled(false);
        if (row.spin) row.spin->setEnabled(false);
        if (row.edit) row.edit->setEnabled(false);
        row.btnRevert->setEnabled(false);
    } else {
        row.lblGated->hide();
    }

    mRows.insert(tweak.id, row);
    return row.card;
}

void MacTweaksWidget::onAllStatusesFetched(QMap<QString, MacDefaultsReadResult> statuses)
{
    mLblLoading->hide();
    for (auto it = statuses.constBegin(); it != statuses.constEnd(); ++it) {
        auto rowIt = mRows.find(it.key());
        if (rowIt == mRows.end())
            continue;
        renderRow(rowIt.value(), it.value());
    }
}

void MacTweaksWidget::renderRow(RowWidgets &row, const MacDefaultsReadResult &read)
{
    const QVariant effective = MacTweaksCatalog::effectiveValue(row.def, read);

    row.lblCurrent->setText(tr("Current: <b>%1</b>%2")
        .arg(formatValue(row.def, effective),
             read.found ? QString() : tr("  (system default)")));

    if (!row.supported)
        return;

    if (row.chk) {
        QSignalBlocker blocker(row.chk);
        row.chk->setChecked(effective == row.def.enabledValue);
    } else if (row.combo) {
        QSignalBlocker blocker(row.combo);
        const int idx = row.combo->findData(effective);
        row.combo->setCurrentIndex(idx >= 0 ? idx : 0);
    } else if (row.spin) {
        QSignalBlocker blocker(row.spin);
        row.spin->setValue(effective.toInt());
    } else if (row.edit) {
        QSignalBlocker blocker(row.edit);
        row.edit->setText(effective.toString());
    }
}

void MacTweaksWidget::setRowBusy(RowWidgets &row, bool busy)
{
    if (row.chk) row.chk->setEnabled(!busy);
    if (row.combo) row.combo->setEnabled(!busy);
    if (row.spin) row.spin->setEnabled(!busy);
    if (row.edit) row.edit->setEnabled(!busy);
    row.btnRevert->setEnabled(!busy);
    if (busy)
        row.lblResult->clear();
}

void MacTweaksWidget::runWrite(const QString &id, std::function<MacDefaultsWriteResult()> op)
{
    auto it = mRows.find(id);
    if (it == mRows.end())
        return;
    setRowBusy(it.value(), true);

    QThreadPool::globalInstance()->start([this, id, op]() {
        const MacDefaultsWriteResult writeRes = op();
        QMetaObject::invokeMethod(this, [this, id, writeRes]() {
            onRowWriteFinished(id, writeRes);
        }, Qt::QueuedConnection);
    });
}

void MacTweaksWidget::onRowWriteFinished(const QString &id, MacDefaultsWriteResult writeRes)
{
    auto it = mRows.find(id);
    if (it == mRows.end())
        return;
    RowWidgets &row = it.value();
    setRowBusy(row, false);

    if (writeRes.ok) {
        row.lblResult->setText(tr("✓ Applied"));
        const MacDefaultsReadResult read = MacTweaksCatalog::readCurrent(row.def);
        renderRow(row, read);
    } else {
        row.lblResult->setText(tr("⚠ %1").arg(
            writeRes.errorMsg.isEmpty() ? tr("Apply failed.") : writeRes.errorMsg));
    }
}

void MacTweaksWidget::onFilterTextChanged(const QString &text)
{
    applyFilter(text);
}

void MacTweaksWidget::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();

    for (auto category : mCategoryOrder.keys()) {
        bool anyVisible = false;
        for (const QString &id : mCategoryOrder.value(category)) {
            auto it = mRows.find(id);
            if (it == mRows.end())
                continue;
            const RowWidgets &row = it.value();
            const bool matches = needle.isEmpty()
                || row.def.name.contains(needle, Qt::CaseInsensitive)
                || row.def.category.contains(needle, Qt::CaseInsensitive)
                || row.def.description.contains(needle, Qt::CaseInsensitive);
            row.card->setVisible(matches);
            anyVisible = anyVisible || matches;
        }
        if (QLabel *header = mCategoryHeaders.value(category, nullptr))
            header->setVisible(anyVisible);
    }
}

void MacTweaksWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg    = sv->value("@cardBg").toString();
    const QString border    = sv->value("@borderColor").toString();
    const QString secondary = sv->value("@color04").toString();
    const QString tertiary  = sv->value("@tertiaryText").toString();
    const QString successCol = sv->value("@successColor").toString();
    const QString warnCol    = sv->value("@warningColor").toString();

    setStyleSheet(QString(
        "QFrame#macTweaksRow {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel#macTweaksDescription { color: %3; }"
        "QLabel#macTweaksCurrent { color: %4; }"
        "QLabel#macTweaksGated { color: %5; }"
    ).arg(cardBg, border, secondary, tertiary, warnCol));

    for (auto &row : mRows) {
        const QString resultText = row.lblResult->text();
        if (resultText.startsWith(QStringLiteral("✓")))
            row.lblResult->setStyleSheet(QString("color: %1;").arg(successCol));
        else if (resultText.startsWith(QStringLiteral("⚠")))
            row.lblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
        else
            row.lblResult->setStyleSheet(QString());
    }
}
