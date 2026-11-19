#include "dev_artifacts_dialog.h"
#include "Common/dialog_buttons.h"
#include "Managers/cleaner_service.h"
#include "Managers/setting_manager.h"

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <Utils/format_util.h>

namespace {

enum Column { ColProject, ColType, ColSize, ColIdle, ColFolder };

constexpr int kRoleProjectIndex = Qt::UserRole;

} // namespace

DevArtifactsDialog::DevArtifactsDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("DevArtifactsDialog"));
    setWindowTitle(tr("Developer Projects"));
    setMinimumSize(760, 520);

    buildUi();

    connect(&mScanWatcher, &QFutureWatcher<DevArtifacts::ScanResult>::finished,
            this, &DevArtifactsDialog::onScanFinished);
    connect(&mTrashWatcher, &QFutureWatcher<QList<DevArtifacts::TrashOutcome>>::finished,
            this, &DevArtifactsDialog::onTrashFinished);
}

DevArtifactsDialog::~DevArtifactsDialog()
{
    mCancelScan.storeRelaxed(1);
    mScanWatcher.waitForFinished();
    mTrashWatcher.waitForFinished();
}

void DevArtifactsDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(DialogButtons::dialogSpacing());
    root->setContentsMargins(DialogButtons::dialogMargins());

    auto *intro = new QLabel(
        tr("Finds code projects you have not touched for a while and offers their "
           "rebuildable folders (node_modules, virtual environments, build output) "
           "for removal. Age counts from the project's own files and git activity, "
           "not from the folder itself. Projects with a running program inside, or "
           "with a git operation in progress, are never listed. Everything goes to "
           "the Trash."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *rootsRow = new QHBoxLayout();
    mRootList = new QListWidget(this);
    mRootList->setObjectName(QStringLiteral("devArtifactRoots"));
    mRootList->setMaximumHeight(84);
    mRootList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const QString &dir : SettingManager::ins()->getDevArtifactRoots())
        mRootList->addItem(dir);
    rootsRow->addWidget(mRootList, 1);

    auto *rootButtons = new QVBoxLayout();
    mBtnAddRoot = new QPushButton(tr("Add Folder…"), this);
    mBtnAddRoot->setCursor(Qt::PointingHandCursor);
    connect(mBtnAddRoot, &QPushButton::clicked, this, &DevArtifactsDialog::onAddRoot);
    rootButtons->addWidget(mBtnAddRoot);
    mBtnRemoveRoot = new QPushButton(tr("Remove"), this);
    mBtnRemoveRoot->setCursor(Qt::PointingHandCursor);
    connect(mBtnRemoveRoot, &QPushButton::clicked, this, &DevArtifactsDialog::onRemoveRoot);
    rootButtons->addWidget(mBtnRemoveRoot);
    rootButtons->addStretch();
    rootsRow->addLayout(rootButtons);
    root->addLayout(rootsRow);

    auto *scanRow = new QHBoxLayout();
    scanRow->addWidget(new QLabel(tr("Only show projects idle for at least"), this));
    mSpinIdleDays = new QSpinBox(this);
    mSpinIdleDays->setRange(0, 3650);
    mSpinIdleDays->setValue(30);
    mSpinIdleDays->setSuffix(tr(" days"));
    connect(mSpinIdleDays, &QSpinBox::valueChanged, this, &DevArtifactsDialog::populateResults);
    scanRow->addWidget(mSpinIdleDays);
    scanRow->addStretch();
    mBtnScan = new QPushButton(tr("Find Projects"), this);
    mBtnScan->setCursor(Qt::PointingHandCursor);
    connect(mBtnScan, &QPushButton::clicked, this, &DevArtifactsDialog::onScan);
    scanRow->addWidget(mBtnScan);
    mBtnCancelScan = new QPushButton(tr("Stop"), this);
    mBtnCancelScan->setCursor(Qt::PointingHandCursor);
    mBtnCancelScan->setEnabled(false);
    connect(mBtnCancelScan, &QPushButton::clicked, this, [this] { mCancelScan.storeRelaxed(1); });
    scanRow->addWidget(mBtnCancelScan);
    root->addLayout(scanRow);

    mBusy = new QProgressBar(this);
    mBusy->setRange(0, 0);
    mBusy->setTextVisible(false);
    mBusy->setMaximumHeight(6);
    mBusy->hide();
    root->addWidget(mBusy);

    mTree = new QTreeWidget(this);
    mTree->setObjectName(QStringLiteral("devArtifactTree"));
    mTree->setColumnCount(5);
    mTree->setHeaderLabels({tr("Project"), tr("Type"), tr("Size"), tr("Idle"), tr("Folder")});
    mTree->setRootIsDecorated(true);
    mTree->setUniformRowHeights(true);
    mTree->header()->setSectionResizeMode(ColProject, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColSize, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColIdle, QHeaderView::ResizeToContents);
    mTree->header()->setStretchLastSection(true);
    connect(mTree, &QTreeWidget::itemChanged, this, &DevArtifactsDialog::updateSelection);
    root->addWidget(mTree, 1);

    mLblStatus = new QLabel(tr("Add the folders where you keep your code, then choose Find Projects."), this);
    mLblStatus->setObjectName(QStringLiteral("lblDevArtifactStatus"));
    mLblStatus->setWordWrap(true);
    root->addWidget(mLblStatus);

    mLblSelection = new QLabel(this);
    mLblSelection->setObjectName(QStringLiteral("lblDevArtifactSelection"));
    root->addWidget(mLblSelection);

    DialogButtons::Row row = DialogButtons::build(this, tr("Move to Trash…"),
                                                  DialogButtons::Confirm::Danger, tr("Close"));
    mBtnTrash = row.confirm;
    mBtnTrash->setObjectName(QStringLiteral("btnDevArtifactTrash"));
    mBtnTrash->setEnabled(false);
    connect(mBtnTrash, &QPushButton::clicked, this, &DevArtifactsDialog::onTrash);
    root->addWidget(row.box);

    updateSelection();
}

QStringList DevArtifactsDialog::roots() const
{
    QStringList list;
    for (int i = 0; i < mRootList->count(); ++i)
        list.append(mRootList->item(i)->text());
    return list;
}

void DevArtifactsDialog::saveRoots() const
{
    SettingManager::ins()->setDevArtifactRoots(roots());
}

void DevArtifactsDialog::onAddRoot()
{
    const QString start = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add a folder to search"), start);
    if (dir.isEmpty())
        return;

    const QString reason = DevArtifacts::rootRefusalReason(dir);
    if (!reason.isEmpty()) {
        QMessageBox::warning(this, tr("Folder not allowed"),
            tr("Nexis will not search %1 because it is %2. Choose the folder that holds your "
               "projects instead.").arg(QDir::toNativeSeparators(dir), reason));
        return;
    }

    const QString resolved = QFileInfo(dir).canonicalFilePath();
    for (const QString &existing : roots()) {
        if (QFileInfo(existing).canonicalFilePath() == resolved)
            return;
    }
    mRootList->addItem(dir);
    saveRoots();
}

void DevArtifactsDialog::onRemoveRoot()
{
    qDeleteAll(mRootList->selectedItems());
    saveRoots();
}

void DevArtifactsDialog::setBusy(bool busy, const QString &status)
{
    mBusy->setVisible(busy);
    mBtnScan->setEnabled(!busy);
    mBtnAddRoot->setEnabled(!busy);
    mBtnRemoveRoot->setEnabled(!busy);
    mTree->setEnabled(!busy);
    if (!status.isEmpty())
        mLblStatus->setText(status);
    updateSelection();
}

void DevArtifactsDialog::onScan()
{
    if (mScanWatcher.isRunning() || mTrashWatcher.isRunning())
        return;
    if (mRootList->count() == 0) {
        mLblStatus->setText(tr("Add at least one folder to search first."));
        return;
    }

    DevArtifacts::ScanOptions options;
    options.roots = roots();
    const auto exclusions = CleanerService::ins()->loadExclusions();
    options.isExcluded = [exclusions](const QString &path) {
        return CleanerService::isExcluded(path, exclusions);
    };

    mCancelScan.storeRelaxed(0);
    mResult = {};
    mTree->clear();
    setBusy(true, tr("Searching…"));
    mBtnCancelScan->setEnabled(true);

    const QAtomicInt *cancel = &mCancelScan;
    mScanWatcher.setFuture(QtConcurrent::run([options, cancel]() {
        const DevArtifacts::HeldPathsProbe probe = DevArtifacts::HeldPathsProbe::fromSystem();
        return DevArtifacts::scan(options, probe, cancel);
    }));
}

void DevArtifactsDialog::onScanFinished()
{
    mResult = mScanWatcher.result();
    mBtnCancelScan->setEnabled(false);
    setBusy(false);
    populateResults();
}

void DevArtifactsDialog::populateResults()
{
    mTree->blockSignals(true);
    mTree->clear();

    const qint64 minIdle = mSpinIdleDays->value();
    const QDateTime now = QDateTime::currentDateTime();
    quint64 listedBytes = 0;
    int listed = 0;

    for (int i = 0; i < mResult.projects.size(); ++i) {
        const DevArtifacts::Project &project = mResult.projects.at(i);
        const qint64 idle = project.idleDays(now);
        if (idle < minIdle)
            continue;

        auto *item = new QTreeWidgetItem(mTree);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(ColProject, Qt::Unchecked);
        item->setText(ColProject, project.name);
        item->setText(ColType, project.kinds.join(QStringLiteral(", ")));
        item->setText(ColSize, FormatUtil::formatBytes(project.totalBytes()));
        item->setText(ColIdle, tr("%n day(s)", nullptr, static_cast<int>(idle)));
        item->setText(ColFolder, QDir::toNativeSeparators(project.root));
        item->setToolTip(ColProject, tr("Last activity: %1")
                                         .arg(QLocale().toString(project.lastActivity, QLocale::ShortFormat)));
        item->setData(ColProject, kRoleProjectIndex, i);

        for (const DevArtifacts::Artifact &artifact : project.artifacts) {
            auto *child = new QTreeWidgetItem(item);
            child->setText(ColProject, QFileInfo(artifact.path).fileName());
            child->setText(ColSize, FormatUtil::formatBytes(artifact.bytes));
            child->setText(ColFolder, QDir::toNativeSeparators(artifact.path));
        }
        listedBytes += project.totalBytes();
        ++listed;
    }
    mTree->blockSignals(false);

    QStringList notes;
    if (mResult.probeUnavailable) {
        notes << tr("Could not check which projects are in use, so nothing is listed.");
    } else {
        if (mResult.skippedInUse > 0)
            notes << tr("%n project(s) in use were skipped.", nullptr, mResult.skippedInUse);
        if (mResult.skippedGitOperation > 0)
            notes << tr("%n project(s) with a git operation in progress were skipped.", nullptr,
                        mResult.skippedGitOperation);
        if (mResult.skippedUnverifiable > 0)
            notes << tr("%n project(s) too large to check were skipped.", nullptr,
                        mResult.skippedUnverifiable);
    }
    for (const QString &refused : mResult.refusedRoots)
        notes << tr("Not searched: %1").arg(refused);
    if (mResult.cancelled)
        notes << tr("Search stopped early.");

    QString status;
    if (!mResult.probeUnavailable)
        status = listed > 0
            ? tr("%n project(s), %1 can be freed.", nullptr, listed).arg(FormatUtil::formatBytes(listedBytes))
            : tr("No stale projects found.");
    mLblStatus->setText((QStringList{status} + notes).join(QLatin1Char(' ')).trimmed());
    updateSelection();
}

void DevArtifactsDialog::selectionTotals(int &projects, quint64 &bytes) const
{
    projects = 0;
    bytes = 0;
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *item = mTree->topLevelItem(i);
        if (item->checkState(ColProject) != Qt::Checked)
            continue;
        ++projects;
        bytes += mResult.projects.at(item->data(ColProject, kRoleProjectIndex).toInt()).totalBytes();
    }
}

void DevArtifactsDialog::updateSelection()
{
    int projects = 0;
    quint64 bytes = 0;
    selectionTotals(projects, bytes);
    mLblSelection->setText(projects > 0
        ? tr("%n project(s) selected, %1", nullptr, projects).arg(FormatUtil::formatBytes(bytes))
        : QString());
    mBtnTrash->setEnabled(projects > 0 && !mScanWatcher.isRunning() && !mTrashWatcher.isRunning());
}

QList<DevArtifacts::TrashRequest> DevArtifactsDialog::checkedRequests() const
{
    QList<DevArtifacts::TrashRequest> requests;
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *item = mTree->topLevelItem(i);
        if (item->checkState(ColProject) != Qt::Checked)
            continue;
        const DevArtifacts::Project &project =
            mResult.projects.at(item->data(ColProject, kRoleProjectIndex).toInt());
        for (const DevArtifacts::Artifact &artifact : project.artifacts)
            requests.append({project.root, artifact.path, project.lastActivity});
    }
    return requests;
}

void DevArtifactsDialog::onTrash()
{
    const QList<DevArtifacts::TrashRequest> requests = checkedRequests();
    if (requests.isEmpty() || mScanWatcher.isRunning() || mTrashWatcher.isRunning())
        return;

    int projects = 0;
    quint64 bytes = 0;
    selectionTotals(projects, bytes);

    const auto answer = QMessageBox::question(this, tr("Move to Trash"),
        tr("Move the rebuildable folders of %n project(s) (%1) to the Trash?\n\n"
           "Your source files are not touched. Reinstall dependencies or rebuild to get these "
           "folders back. Space is only freed once the Trash is emptied.", nullptr, projects)
            .arg(FormatUtil::formatBytes(bytes)),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    const auto exclusions = CleanerService::ins()->loadExclusions();
    setBusy(true, tr("Moving to the Trash…"));
    mTrashWatcher.setFuture(QtConcurrent::run([requests, exclusions]() {
        // A fresh process snapshot: the listing may be minutes old.
        const DevArtifacts::HeldPathsProbe probe = DevArtifacts::HeldPathsProbe::fromSystem();
        return DevArtifacts::trashArtifacts(
            requests, DevArtifacts::RunContext::Interactive, probe,
            [](const QString &path) { return QFile::moveToTrash(path); },
            [exclusions](const QString &path) { return CleanerService::isExcluded(path, exclusions); });
    }));
}

void DevArtifactsDialog::onTrashFinished()
{
    const QList<DevArtifacts::TrashOutcome> outcomes = mTrashWatcher.result();

    QHash<QString, quint64> bytesByPath;
    for (const DevArtifacts::Project &project : std::as_const(mResult.projects)) {
        for (const DevArtifacts::Artifact &artifact : project.artifacts)
            bytesByPath.insert(artifact.path, artifact.bytes);
    }

    int trashed = 0;
    quint64 freed = 0;
    QStringList leftAlone;
    QSet<QString> gone;
    for (const DevArtifacts::TrashOutcome &outcome : outcomes) {
        if (outcome.trashed) {
            ++trashed;
            freed += bytesByPath.value(outcome.request.artifactPath);
            gone.insert(outcome.request.artifactPath);
        } else {
            leftAlone << QStringLiteral("%1 — %2")
                             .arg(QDir::toNativeSeparators(outcome.request.artifactPath), outcome.reason);
        }
    }

    for (int i = mResult.projects.size() - 1; i >= 0; --i) {
        DevArtifacts::Project &project = mResult.projects[i];
        project.artifacts.erase(
            std::remove_if(project.artifacts.begin(), project.artifacts.end(),
                           [&gone](const DevArtifacts::Artifact &a) { return gone.contains(a.path); }),
            project.artifacts.end());
        if (project.artifacts.isEmpty())
            mResult.projects.removeAt(i);
    }

    setBusy(false);
    populateResults();

    QString summary = tr("Moved %n folder(s) (%1) to the Trash.", nullptr, trashed)
                          .arg(FormatUtil::formatBytes(freed));
    if (!leftAlone.isEmpty())
        summary += QLatin1Char(' ') + tr("%n folder(s) left alone.", nullptr, static_cast<int>(leftAlone.size()));
    mLblStatus->setText(summary);

    if (!leftAlone.isEmpty()) {
        QMessageBox box(QMessageBox::Information, tr("Some folders were left alone"),
                        tr("These folders were not moved, because something changed since the search "
                           "or a safety check failed:"), QMessageBox::Close, this);
        box.setDetailedText(leftAlone.join(QLatin1Char('\n')));
        box.exec();
    }
}

void DevArtifactsDialog::reject()
{
    if (mTrashWatcher.isRunning())
        return;
    mCancelScan.storeRelaxed(1);
    QDialog::reject();
}

void DevArtifactsDialog::closeEvent(QCloseEvent *event)
{
    if (mTrashWatcher.isRunning()) {
        event->ignore();
        return;
    }
    mCancelScan.storeRelaxed(1);
    QDialog::closeEvent(event);
}
