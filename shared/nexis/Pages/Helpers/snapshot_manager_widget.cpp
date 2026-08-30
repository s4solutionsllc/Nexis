#include "snapshot_manager_widget.h"

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/command_util.h>
#include <Utils/format_util.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStorageInfo>
#include <QThreadPool>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// tmutil's snapshot identifiers embed a sortable timestamp, e.g.
// "com.apple.TimeMachine.2024-01-15-120000.local". `deletelocalsnapshots`
// takes just the "2024-01-15-120000" token, not the full identifier.
QString displaySnapshotDate(const QString &dateToken)
{
    const QDateTime dt = QDateTime::fromString(dateToken, QStringLiteral("yyyy-MM-dd-HHmmss"));
    return dt.isValid() ? QLocale().toString(dt, QLocale::ShortFormat) : dateToken;
}

} // namespace

SnapshotManagerWidget::SnapshotManagerWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(this, &SnapshotManagerWidget::stateFetched,
            this, &SnapshotManagerWidget::onStateFetched);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &SnapshotManagerWidget::refreshThemeColors);
    refreshThemeColors();
}

void SnapshotManagerWidget::loadIfNeeded()
{
    if (!mLoaded)
        refresh();
}

void SnapshotManagerWidget::refresh()
{
    mLoaded = true;
    setBusy(true);
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this]() {
        const State state = fetchState();
        emit stateFetched(state);
    });
}

// ── Pure, testable parsing ──────────────────────────────────────────────────

QList<SnapshotManagerWidget::SnapshotEntry>
SnapshotManagerWidget::parseListLocalSnapshots(const QString &output)
{
    QList<SnapshotEntry> entries;

    // Matches the snapshot identifier anywhere in the line so an optional
    // "Snapshots for disk /:" (or "...volume group containing...") header
    // line, blank lines, or trailing noise are all safely ignored.
    static const QRegularExpression re(
        QStringLiteral(R"(com\.apple\.TimeMachine\.(\d{4}-\d{2}-\d{2}-\d{6})(?:\.local)?)"));

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QRegularExpressionMatch m = re.match(rawLine.trimmed());
        if (!m.hasMatch())
            continue;

        SnapshotEntry entry;
        entry.dateToken = m.captured(1);
        entry.timestamp = QDateTime::fromString(entry.dateToken, QStringLiteral("yyyy-MM-dd-HHmmss"));
        entries.append(entry);
    }

    // Newest first. The token format sorts lexicographically identically to
    // chronological order, so a plain string compare is sufficient.
    std::sort(entries.begin(), entries.end(),
              [](const SnapshotEntry &a, const SnapshotEntry &b) {
                  return a.dateToken > b.dateToken;
              });

    return entries;
}

// ── State fetch (worker thread) ─────────────────────────────────────────────

SnapshotManagerWidget::State SnapshotManagerWidget::fetchState() const
{
    State state;

    if (!CommandUtil::isExecutable("tmutil")) {
        state.errorMsg = tr("tmutil is not available on this system.");
        return state;
    }
    state.available = true;

    const ExecResult r = CommandUtil::execWithStatus("tmutil", {"listlocalsnapshots", "/"}, 15000);
    if (!r.ok() && r.output.isEmpty()) {
        state.errorMsg = r.error.isEmpty() ? tr("Failed to list local snapshots.") : r.error;
    } else {
        state.snapshots = parseListLocalSnapshots(r.output);
    }

    // APFS counts purgeable space (e.g. local snapshots) as available in
    // statfs()'s free-block count, so QStorageInfo already reports the same
    // "will grow when you delete a snapshot" figure Finder shows.
    const QStorageInfo storage(QStringLiteral("/"));
    if (storage.isValid())
        state.availableBytes = storage.bytesAvailable();

    return state;
}

// ── UI ───────────────────────────────────────────────────────────────────────

void SnapshotManagerWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("Local Snapshots"), this);
    QFont titleFont = mLblTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    mLblTitle->setFont(titleFont);
    root->addWidget(mLblTitle);

    auto *intro = new QLabel(
        tr("APFS keeps local Time Machine snapshots on disk as purgeable space. "
           "Deleting old snapshots reclaims that space immediately."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    mCard = new QFrame(this);
    mCard->setObjectName("snapshotManagerCard");
    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 16, 16, 16);
    card->setSpacing(10);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    mLblAvailable = new QLabel(mCard);
    mLblAvailable->setObjectName("snapshotAvailableSpace");
    topRow->addWidget(mLblAvailable);
    topRow->addStretch();

    mBtnCreate = new QPushButton(tr("Create Snapshot"), mCard);
    mBtnCreate->setCursor(Qt::PointingHandCursor);
    connect(mBtnCreate, &QPushButton::clicked, this, &SnapshotManagerWidget::onCreateClicked);
    topRow->addWidget(mBtnCreate);

    mBtnRefresh = new QPushButton(tr("Refresh"), mCard);
    mBtnRefresh->setCursor(Qt::PointingHandCursor);
    connect(mBtnRefresh, &QPushButton::clicked, this, &SnapshotManagerWidget::refresh);
    topRow->addWidget(mBtnRefresh);

    card->addLayout(topRow);

    mLblLoading = new QLabel(tr("Loading…"), mCard);
    mLblLoading->hide();
    card->addWidget(mLblLoading);

    mLblResult = new QLabel(mCard);
    mLblResult->setObjectName("snapshotResult");
    mLblResult->setWordWrap(true);
    card->addWidget(mLblResult);

    mLblEmpty = new QLabel(tr("No local snapshots."), mCard);
    mLblEmpty->hide();
    card->addWidget(mLblEmpty);

    // QScrollArea-in-programmatic-widget theme workaround (CLAUDE.md Qt/QSS
    // gotchas): the viewport otherwise renders with the system palette
    // instead of the QSS theme.
    auto *scrollArea = new QScrollArea(mCard);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea{background-color:transparent;}"));

    mListContainer = new QWidget(scrollArea);
    mListContainer->setStyleSheet(QStringLiteral("background-color:transparent;"));
    mListLayout = new QVBoxLayout(mListContainer);
    mListLayout->setContentsMargins(0, 0, 0, 0);
    mListLayout->setSpacing(6);
    mListLayout->addStretch();

    scrollArea->setWidget(mListContainer);
    card->addWidget(scrollArea, 1);

    root->addWidget(mCard, 1);
}

void SnapshotManagerWidget::setBusy(bool busy)
{
    mBtnCreate->setEnabled(!busy);
    mBtnRefresh->setEnabled(!busy);
    mLblLoading->setVisible(busy);
    for (QPushButton *btn : mListContainer->findChildren<QPushButton *>())
        btn->setEnabled(!busy);
}

void SnapshotManagerWidget::onStateFetched(State state)
{
    setBusy(false);
    renderState(state);
}

void SnapshotManagerWidget::renderState(const State &state)
{
    mCurrent = state;

    if (!state.available) {
        mCard->hide();
        mLblResult->setText(state.errorMsg);
        refreshThemeColors();
        return;
    }
    mCard->show();

    mLblAvailable->setText(state.availableBytes >= 0
        ? tr("Available disk space: %1")
              .arg(FormatUtil::formatBytes(static_cast<quint64>(state.availableBytes)))
        : tr("Available disk space: unknown"));

    if (!state.errorMsg.isEmpty())
        mLblResult->setText(tr("⚠ %1").arg(state.errorMsg));

    rebuildSnapshotRows();
    refreshThemeColors();
}

void SnapshotManagerWidget::rebuildSnapshotRows()
{
    QLayoutItem *item;
    while ((item = mListLayout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    mLblEmpty->setVisible(mCurrent.snapshots.isEmpty());

    for (const SnapshotEntry &entry : std::as_const(mCurrent.snapshots)) {
        auto *row = new QFrame(mListContainer);
        row->setObjectName("snapshotRow");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 8, 10, 8);
        rowLayout->setSpacing(8);

        auto *lbl = new QLabel(displaySnapshotDate(entry.dateToken), row);
        rowLayout->addWidget(lbl);
        rowLayout->addStretch();

        auto *btnDelete = new QPushButton(tr("Delete"), row);
        btnDelete->setCursor(Qt::PointingHandCursor);
        const QString dateToken = entry.dateToken;
        connect(btnDelete, &QPushButton::clicked, this,
                [this, dateToken] { onDeleteClicked(dateToken); });
        rowLayout->addWidget(btnDelete);

        mListLayout->addWidget(row);
    }

    mListLayout->addStretch();
}

void SnapshotManagerWidget::onCreateClicked()
{
    setBusy(true);
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this]() {
        const ExecResult r = CommandUtil::execWithStatus("tmutil", {"localsnapshot"}, 60000);
        const bool ok = r.ok();
        const QString errorMsg = r.error.isEmpty() ? r.output : r.error;

        QMetaObject::invokeMethod(this, [this, ok, errorMsg]() {
            if (ok) {
                mLblResult->setText(tr("✓ Snapshot created"));
                refresh();
            } else {
                setBusy(false);
                mLblResult->setText(tr("⚠ Failed to create snapshot: %1")
                    .arg(errorMsg.isEmpty() ? tr("unknown error") : errorMsg));
                refreshThemeColors();
            }
        }, Qt::QueuedConnection);
    });
}

void SnapshotManagerWidget::onDeleteClicked(const QString &dateToken)
{
    if (QMessageBox::question(this, tr("Delete Snapshot"),
            tr("Delete the local snapshot from %1?\n\n"
               "This reclaims the purgeable disk space it holds and cannot be undone.")
                .arg(displaySnapshotDate(dateToken)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    setBusy(true);
    mLblResult->clear();

    QThreadPool::globalInstance()->start([this, dateToken]() {
        const ExecResult r = CommandUtil::execWithStatus(
            "tmutil", {"deletelocalsnapshots", dateToken}, 60000);
        const bool ok = r.ok();
        const QString errorMsg = r.error.isEmpty() ? r.output : r.error;

        QMetaObject::invokeMethod(this, [this, ok, errorMsg]() {
            if (ok) {
                mLblResult->setText(tr("✓ Snapshot deleted"));
                refresh();
            } else {
                setBusy(false);
                mLblResult->setText(tr("⚠ Failed to delete snapshot: %1")
                    .arg(errorMsg.isEmpty() ? tr("unknown error") : errorMsg));
                refreshThemeColors();
            }
        }, Qt::QueuedConnection);
    });
}

void SnapshotManagerWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    const QString cardBg     = sv->value("@cardBg").toString();
    const QString border     = sv->value("@borderColor").toString();
    const QString secondary  = sv->value("@color04").toString();
    const QString successCol = sv->value("@successColor").toString();
    const QString warnCol    = sv->value("@warningColor").toString();

    if (mCard) {
        mCard->setStyleSheet(QString(
            "QFrame#snapshotManagerCard {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "}").arg(cardBg, border));
    }

    if (mLblAvailable)
        mLblAvailable->setStyleSheet(QString("color: %1;").arg(secondary));
    if (mLblEmpty)
        mLblEmpty->setStyleSheet(QString("color: %1;").arg(secondary));

    if (mListContainer) {
        const QString rowCss = QString(
            "QFrame#snapshotRow {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 6px;"
            "}").arg(cardBg, border);
        for (QFrame *row : mListContainer->findChildren<QFrame *>())
            row->setStyleSheet(rowCss);
    }

    if (mLblResult) {
        const QString resultText = mLblResult->text();
        if (resultText.startsWith(QStringLiteral("✓")))
            mLblResult->setStyleSheet(QString("color: %1;").arg(successCol));
        else if (resultText.startsWith(QStringLiteral("⚠")))
            mLblResult->setStyleSheet(QString("color: %1;").arg(warnCol));
        else
            mLblResult->setStyleSheet(QString());
    }
}
