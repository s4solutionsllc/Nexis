#include "ai_models_dialog.h"
#include "Common/dialog_buttons.h"

#include <Utils/format_util.h>

#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int ModelIndexRole = Qt::UserRole + 1;
constexpr int ColName = 0;
constexpr int ColTool = 1;
constexpr int ColFrees = 2;
constexpr int ColLastUsed = 3;
constexpr int ColWarning = 4;
}

AiModelsDialog::AiModelsDialog(const QList<AiModelStore::Model> &models,
                               const AiModelStore::Environment &env,
                               QWidget *parent)
    : QDialog(parent),
      mEnv(env),
      mModels(models)
{
    setWindowTitle(tr("Local AI Models"));
    setObjectName("aiModelsDialog");
    setMinimumSize(860, 520);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(DialogButtons::dialogSpacing());
    mainLayout->setContentsMargins(DialogButtons::dialogMargins());

    auto *lblTitle = new QLabel(tr("Local AI Models"), this);
    lblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(lblTitle);

    auto *lblIntro = new QLabel(
        tr("Models downloaded by Ollama, Hugging Face, LM Studio and llama.cpp. You pick what "
           "to remove: nothing here is ticked for you and it is never part of a scheduled clean. "
           "\"Frees\" is what is really given back to disk; data two models share is counted "
           "once, and only when you tick every model that uses it. A removed model has to be "
           "downloaded again."),
        this);
    lblIntro->setWordWrap(true);
    mainLayout->addWidget(lblIntro);

    mTree = new QTreeWidget(this);
    mTree->setObjectName("aiModelTree");
    mTree->setRootIsDecorated(false);
    mTree->setAlternatingRowColors(true);
    mTree->setUniformRowHeights(true);
    mTree->setColumnCount(5);
    mTree->setHeaderLabels({tr("Model"), tr("Tool"), tr("Frees"), tr("Last used"), tr("Heads-up")});
    mTree->header()->setStretchLastSection(false);
    mTree->header()->setSectionResizeMode(ColName, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColTool, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColFrees, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColLastUsed, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(ColWarning, QHeaderView::Stretch);
    mainLayout->addWidget(mTree, 1);

    mLblSummary = new QLabel(this);
    mLblSummary->setObjectName("lblAiModelSummary");
    mainLayout->addWidget(mLblSummary);

    DialogButtons::Row buttons = DialogButtons::build(this, tr("Remove Selected"),
                                                      DialogButtons::Confirm::Danger, tr("Close"));
    mBtnRemove = buttons.confirm;
    mBtnRemove->setObjectName("btnRemoveAiModels");
    mainLayout->addWidget(buttons.box);

    connect(mBtnRemove, &QPushButton::clicked, this, &AiModelsDialog::onRemoveClicked);
    connect(mTree, &QTreeWidget::itemChanged, this, &AiModelsDialog::updateSelection);

    populate();
}

void AiModelsDialog::populate()
{
    mTree->blockSignals(true);
    mTree->clear();
    if (mModels.isEmpty()) {
        auto *empty = new QTreeWidgetItem(mTree);
        empty->setText(ColName, tr("No downloaded models were found."));
        empty->setFlags(Qt::NoItemFlags);
    }
    for (int i = 0; i < mModels.size(); ++i) {
        const AiModelStore::Model &model = mModels.at(i);
        auto *item = new QTreeWidgetItem(mTree);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(ColName, Qt::Unchecked);
        item->setData(ColName, ModelIndexRole, i);
        item->setText(ColName, model.name);
        item->setToolTip(ColName, model.topLevelPaths.join(QLatin1Char('\n')));
        item->setText(ColTool, AiModelStore::toolDisplayName(model.tool));
        item->setText(ColFrees, FormatUtil::formatBytes(model.exclusiveBytes()));
        item->setTextAlignment(ColFrees, Qt::AlignRight | Qt::AlignVCenter);
        item->setText(ColLastUsed, model.lastUsed.isValid()
                                       ? QLocale().toString(model.lastUsed.date(), QLocale::ShortFormat)
                                       : QString());
        const QString warning = warningFor(model, AiModelStore::removesWithToolCommand(model, mEnv));
        item->setText(ColWarning, warning);
        item->setToolTip(ColWarning, warning);
    }
    mTree->blockSignals(false);
    updateSelection();
}

QList<AiModelStore::Model> AiModelsDialog::selectedModels() const
{
    QList<AiModelStore::Model> selected;
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *item = mTree->topLevelItem(i);
        if (item->checkState(ColName) == Qt::Checked)
            selected.append(mModels.at(item->data(ColName, ModelIndexRole).toInt()));
    }
    return selected;
}

QString AiModelsDialog::warningFor(const AiModelStore::Model &model, bool usesToolCommand)
{
    QString text = tr("Re-download: %1.").arg(FormatUtil::formatBytes(model.totalBytes()));
    switch (model.tool) {
    case AiModelStore::Tool::Ollama:
        text += usesToolCommand
            ? tr(" Removed with `ollama rm`, so Ollama's list stays correct.")
            : tr(" Ollama was not found, so its files go to the Trash; Ollama may keep listing the "
                 "model until it is restarted.");
        break;
    case AiModelStore::Tool::HuggingFace:
        text += tr(" Every cached version goes to the Trash. Projects that link into this cache "
                   "stop working until the next download.");
        break;
    case AiModelStore::Tool::LmStudio:
        text += tr(" The folder goes to the Trash; LM Studio may keep listing the model until it "
                   "is restarted.");
        break;
    case AiModelStore::Tool::LlamaCpp:
        text += tr(" The files go to the Trash; llama.cpp downloads the model again on next use.");
        break;
    }
    if (model.sharedBytes() > 0) {
        text += tr(" %1 is shared with another model and stays unless that one is removed too.")
                    .arg(FormatUtil::formatBytes(model.sharedBytes()));
    }
    return text;
}

QString AiModelsDialog::summaryFor(const QList<AiModelStore::Model> &selection)
{
    if (selection.isEmpty())
        return tr("Nothing selected.");
    return tr("%n model(s) selected, frees %1", nullptr, selection.size())
        .arg(FormatUtil::formatBytes(AiModelStore::reclaimableBytes(selection)));
}

void AiModelsDialog::updateSelection()
{
    const QList<AiModelStore::Model> selection = selectedModels();
    mLblSummary->setText(summaryFor(selection));
    mBtnRemove->setEnabled(!selection.isEmpty());
}

void AiModelsDialog::onRemoveClicked()
{
    const QList<AiModelStore::Model> selection = selectedModels();
    if (selection.isEmpty())
        return;

    QStringList lines;
    for (const AiModelStore::Model &model : selection) {
        lines << QStringLiteral("• %1 (%2): %3")
                     .arg(model.name, AiModelStore::toolDisplayName(model.tool),
                          warningFor(model, AiModelStore::removesWithToolCommand(model, mEnv)));
    }
    QMessageBox confirm(QMessageBox::Warning, tr("Remove AI Models"),
                        tr("Remove %n model(s) and free about %1?", nullptr, selection.size())
                            .arg(FormatUtil::formatBytes(AiModelStore::reclaimableBytes(selection))),
                        QMessageBox::NoButton, this);
    confirm.setInformativeText(lines.join(QLatin1Char('\n')));
    QPushButton *remove = confirm.addButton(tr("Remove"), QMessageBox::DestructiveRole);
    confirm.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirm.setDefaultButton(static_cast<QPushButton *>(nullptr));
    confirm.exec();
    if (confirm.clickedButton() != remove)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<AiModelStore::RemoveOutcome> outcomes = AiModelStore::remove(selection, mEnv);
    mModels = AiModelStore::scan(mEnv);
    QApplication::restoreOverrideCursor();

    quint64 freed = 0;
    int removed = 0;
    QStringList problems;
    for (const AiModelStore::RemoveOutcome &outcome : outcomes) {
        freed += outcome.freedBytes;
        if (outcome.status == AiModelStore::RemoveOutcome::Status::Removed)
            ++removed;
        else
            problems << QStringLiteral("%1: %2").arg(outcome.name, outcome.message);
    }
    populate();

    QString summary = tr("Removed %n model(s), freed %1.", nullptr, removed).arg(FormatUtil::formatBytes(freed));
    if (problems.isEmpty()) {
        QMessageBox::information(this, tr("Remove AI Models"), summary);
    } else {
        QMessageBox::warning(this, tr("Remove AI Models"),
                             summary + QStringLiteral("\n\n") + problems.join(QLatin1Char('\n')));
    }
}
