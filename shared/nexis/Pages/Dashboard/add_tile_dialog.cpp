#include "add_tile_dialog.h"
#include "dashboard_layout_util.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

AddTileDialog::AddTileDialog(const QList<QPair<QString, QString>> &typeOptions,
                             const QHash<QString, QList<QPair<QString, QString>>> &inputsByType,
                             QWidget *parent)
    : QDialog(parent)
    , mTypeList(new QListWidget(this))
    , mInputList(new QListWidget(this))
    , mInputsByType(inputsByType)
{
    setWindowTitle(tr("Add Tile"));
    setModal(true);

    auto *typeLabel = new QLabel(tr("Tile type"), this);
    for (const auto &t : typeOptions) {
        auto *item = new QListWidgetItem(t.second, mTypeList);
        item->setData(Qt::UserRole, t.first); // store the type key
    }

    auto *inputLabel = new QLabel(tr("Input"), this);

    auto *typeCol = new QVBoxLayout;
    typeCol->addWidget(typeLabel);
    typeCol->addWidget(mTypeList);

    auto *inputCol = new QVBoxLayout;
    inputCol->addWidget(inputLabel);
    inputCol->addWidget(mInputList);

    auto *lists = new QHBoxLayout;
    lists->addLayout(typeCol);
    lists->addLayout(inputCol);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mOkButton = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addLayout(lists);
    root->addWidget(buttons);

    connect(mTypeList, &QListWidget::currentRowChanged, this, &AddTileDialog::onTypeChanged);
    connect(mInputList, &QListWidget::currentRowChanged, this, [this] { updateOkEnabled(); });

    if (mTypeList->count() > 0)
        mTypeList->setCurrentRow(0);
    else
        onTypeChanged(); // hide the (empty) input column

    updateOkEnabled();
}

void AddTileDialog::onTypeChanged()
{
    mInputList->clear();

    const QString type = chosenType();
    const bool multi = !type.isEmpty()
                       && DashboardLayout::isMultiInstanceType(type)
                       && mInputsByType.contains(type);

    if (!multi) {
        mInputList->hide();
        return;
    }

    const auto inputs = mInputsByType.value(type);
    for (const auto &p : inputs) {
        auto *item = new QListWidgetItem(p.second, mInputList);
        item->setData(Qt::UserRole, p.first); // store the input key
    }
    mInputList->show();
    if (mInputList->count() > 0)
        mInputList->setCurrentRow(0);

    updateOkEnabled();
}

void AddTileDialog::updateOkEnabled()
{
    if (!mOkButton)
        return;

    const QString type = chosenType();
    if (type.isEmpty()) {
        mOkButton->setEnabled(false);
        return;
    }

    const bool inputBound = DashboardLayout::isMultiInstanceType(type)
                            && mInputsByType.contains(type);
    if (!inputBound) {
        mOkButton->setEnabled(true); // singleton: always addable
        return;
    }

    // Input-bound: addable only when an available input is selected.
    mOkButton->setEnabled(mInputList->count() > 0 && mInputList->currentRow() >= 0);
}

QString AddTileDialog::chosenType() const
{
    QListWidgetItem *item = mTypeList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString AddTileDialog::chosenInput() const
{
    if (mInputList->isHidden())
        return QString();
    QListWidgetItem *item = mInputList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}
