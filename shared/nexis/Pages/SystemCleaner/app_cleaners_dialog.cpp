#include "app_cleaners_dialog.h"
#include "Common/dialog_buttons.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int kIdRole = Qt::UserRole;

}

AppCleanersDialog::AppCleanersDialog(const QList<CleanerML::Cleaner> &cleaners, const QSet<QString> &runningIds,
                                     QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("App Cleaners"));
    setObjectName("appCleanersDialog");
    setMinimumSize(620, 560);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(DialogButtons::dialogSpacing());
    mainLayout->setContentsMargins(DialogButtons::dialogMargins());

    auto *lblTitle = new QLabel(tr("App Cleaners"), this);
    lblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(lblTitle);

    auto *lblIntro = new QLabel(
        tr("Per-application cleanup rules for apps that have left data on this computer. Tick what "
           "to clean; you review every file before anything is deleted. An app that is open "
           "cannot be cleaned until you close it."),
        this);
    lblIntro->setWordWrap(true);
    mainLayout->addWidget(lblIntro);

    mEditFilter = new QLineEdit(this);
    mEditFilter->setObjectName("editAppCleanerFilter");
    mEditFilter->setPlaceholderText(tr("Filter apps…"));
    mEditFilter->setClearButtonEnabled(true);
    mainLayout->addWidget(mEditFilter);

    mTree = new QTreeWidget(this);
    mTree->setObjectName("appCleanersTree");
    mTree->setHeaderLabels({tr("App / option"), tr("What it removes")});
    mTree->setAlternatingRowColors(true);
    mTree->setSelectionMode(QAbstractItemView::NoSelection);
    mTree->header()->setStretchLastSection(true);
    mTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    for (const CleanerML::Cleaner &cleaner : cleaners) {
        const bool running = runningIds.contains(cleaner.id);
        auto *appItem = new QTreeWidgetItem(mTree);
        appItem->setText(0, cleaner.label.isEmpty() ? cleaner.id : cleaner.label);
        appItem->setText(1, running ? tr("Open — close it to clean") : cleaner.description);
        appItem->setData(0, kIdRole, cleaner.id);
        appItem->setFlags(Qt::ItemIsEnabled);
        QFont bold = appItem->font(0);
        bold.setBold(true);
        appItem->setFont(0, bold);

        for (const CleanerML::Option &option : cleaner.options) {
            auto *optionItem = new QTreeWidgetItem(appItem);
            optionItem->setText(0, option.label.isEmpty() ? option.id : option.label);
            optionItem->setText(1, option.warning.isEmpty()
                                       ? option.description
                                       : option.description + QStringLiteral(" ⚠ ") + option.warning);
            optionItem->setToolTip(1, optionItem->text(1));
            optionItem->setData(0, kIdRole, option.id);
            optionItem->setCheckState(0, Qt::Unchecked);
            optionItem->setFlags(running ? Qt::NoItemFlags : (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
        }
    }
    mTree->expandAll();
    mainLayout->addWidget(mTree, 1);

    DialogButtons::Row buttons = DialogButtons::build(this, tr("Review…"),
                                                      DialogButtons::Confirm::Primary, tr("Cancel"));
    mBtnReview = buttons.confirm;
    mBtnReview->setObjectName("btnReviewAppCleaners");
    mainLayout->addWidget(buttons.box);

    connect(mBtnReview, &QPushButton::clicked, this, &QDialog::accept);
    connect(mEditFilter, &QLineEdit::textChanged, this, &AppCleanersDialog::applyFilter);
    connect(mTree, &QTreeWidget::itemChanged, this, &AppCleanersDialog::updateReviewEnabled);
    updateReviewEnabled();
}

void AppCleanersDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *appItem = mTree->topLevelItem(i);
        appItem->setHidden(!needle.isEmpty() && !appItem->text(0).contains(needle, Qt::CaseInsensitive));
    }
}

void AppCleanersDialog::updateReviewEnabled()
{
    mBtnReview->setEnabled(!selection().isEmpty());
}

QHash<QString, QSet<QString>> AppCleanersDialog::selection() const
{
    QHash<QString, QSet<QString>> selected;
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *appItem = mTree->topLevelItem(i);
        QSet<QString> options;
        for (int j = 0; j < appItem->childCount(); ++j) {
            const QTreeWidgetItem *optionItem = appItem->child(j);
            if (optionItem->checkState(0) == Qt::Checked)
                options.insert(optionItem->data(0, kIdRole).toString());
        }
        if (!options.isEmpty())
            selected.insert(appItem->data(0, kIdRole).toString(), options);
    }
    return selected;
}
