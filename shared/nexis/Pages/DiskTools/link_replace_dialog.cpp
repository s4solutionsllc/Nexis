#include "link_replace_dialog.h"

#include <QButtonGroup>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Common/dialog_buttons.h"
#include <Utils/format_util.h>

LinkReplaceDialog::LinkReplaceDialog(DuplicateFinderService *service, const QStringList &selected,
                                     const QList<DuplicateGroup> &groups, QWidget *parent)
    : QDialog(parent)
    , mService(service)
    , mSelected(selected)
    , mGroups(groups)
{
    setObjectName(QStringLiteral("LinkReplaceDialog"));
    setWindowTitle(tr("Replace Duplicates with Links"));
    setMinimumSize(640, 420);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(DialogButtons::dialogSpacing());
    root->setContentsMargins(DialogButtons::dialogMargins());

    auto *intro = new QLabel(
        tr("Each ticked file is replaced by a link to the file kept in its group, so every path "
           "still opens the same content while the extra copy stops using disk space. Nothing "
           "changes until you confirm."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    mRadioHard = new QRadioButton(
        tr("Hard links — the paths become one file; it is deleted only when the last one is"), this);
    mRadioHard->setObjectName(QStringLiteral("radioHardLink"));
    mRadioSymbolic = new QRadioButton(
        tr("Symbolic links — relative shortcuts to the kept file; they break if it is moved or deleted"),
        this);
    mRadioSymbolic->setObjectName(QStringLiteral("radioSymbolicLink"));
    mRadioHard->setChecked(true);
    auto *group = new QButtonGroup(this);
    group->addButton(mRadioHard);
    group->addButton(mRadioSymbolic);
    connect(group, &QButtonGroup::buttonClicked, this, &LinkReplaceDialog::rebuildPlan);
    root->addWidget(mRadioHard);
    root->addWidget(mRadioSymbolic);

    mTree = new QTreeWidget(this);
    mTree->setObjectName(QStringLiteral("treeLinkPreview"));
    mTree->setFrameShape(QFrame::NoFrame);
    mTree->setRootIsDecorated(false);
    mTree->setAlternatingRowColors(true);
    mTree->setHeaderLabels({tr("Duplicate"), tr("Becomes a link to"), tr("Result")});
    mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    root->addWidget(mTree, 1);

    mLblSummary = new QLabel(this);
    mLblSummary->setObjectName(QStringLiteral("lblLinkSummary"));
    mLblSummary->setWordWrap(true);
    root->addWidget(mLblSummary);

    auto *note = new QLabel(
        tr("Before each replacement the two files are compared again, and a file that changed "
           "since the scan is left alone. A hard link makes the path share the kept file's "
           "permissions and modification time."), this);
    note->setWordWrap(true);
    root->addWidget(note);

    DialogButtons::Row row = DialogButtons::build(this, tr("Replace with Links"),
                                                  DialogButtons::Confirm::Danger, tr("Cancel"));
    mBtnReplace = row.confirm;
    mBtnReplace->setObjectName(QStringLiteral("btnLinkReplaceConfirm"));
    connect(mBtnReplace, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(row.box);

    rebuildPlan();
}

QString LinkReplaceDialog::statusText(LinkReplaceEntry::Status status)
{
    using Status = LinkReplaceEntry::Status;
    switch (status) {
    case Status::Ready:               return tr("Will be replaced");
    case Status::KeptAsOriginal:      return tr("Kept as the original");
    case Status::SkipNotInGroup:      return tr("Skipped — not part of an identical-files group");
    case Status::SkipMissing:         return tr("Skipped — file is missing or not a regular file");
    case Status::SkipExcluded:        return tr("Skipped — protected by your exclusions");
    case Status::SkipAlreadyLinked:   return tr("Skipped — already the same file");
    case Status::SkipCrossFilesystem: return tr("Skipped — kept file is on a different drive");
    }
    return {};
}

void LinkReplaceDialog::rebuildPlan()
{
    const LinkType type = mRadioSymbolic->isChecked() ? LinkType::Symbolic : LinkType::Hard;
    mPlan = mService->previewLinkReplace(mSelected, mGroups, type);

    mTree->clear();
    int ready = 0;
    int skipped = 0;
    quint64 reclaim = 0;
    for (const LinkReplaceEntry &e : mPlan) {
        auto *item = new QTreeWidgetItem(mTree);
        item->setText(0, QDir::toNativeSeparators(e.duplicatePath));
        item->setText(1, e.keeperPath.isEmpty() ? QString() : QDir::toNativeSeparators(e.keeperPath));
        item->setText(2, statusText(e.status));
        item->setToolTip(0, e.duplicatePath);
        item->setToolTip(1, e.keeperPath);
        if (e.isReady()) {
            ++ready;
            reclaim += e.size;
        } else if (e.status != LinkReplaceEntry::Status::KeptAsOriginal) {
            ++skipped;
        }
    }

    mLblSummary->setText(skipped > 0
        ? tr("%1 files will be replaced, freeing %2 · %3 skipped")
              .arg(ready).arg(FormatUtil::formatBytes(reclaim)).arg(skipped)
        : tr("%1 files will be replaced, freeing %2")
              .arg(ready).arg(FormatUtil::formatBytes(reclaim)));
    mBtnReplace->setEnabled(ready > 0);
}
