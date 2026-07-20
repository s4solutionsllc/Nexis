#include "shred_confirm_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <Utils/format_util.h>

ShredConfirmDialog::ShredConfirmDialog(int itemCount, quint64 totalBytes, QWidget *parent)
    : QDialog(parent)
{
    setObjectName("shredConfirmDialog");
    setWindowTitle(tr("Confirm Shred"));
    setMinimumWidth(420);
    buildUI(itemCount, totalBytes);
}

void ShredConfirmDialog::buildUI(int itemCount, quint64 totalBytes)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *lblTitle = new QLabel(
        tr("Shred %n item(s) (%1)?", "", itemCount).arg(FormatUtil::formatBytes(totalBytes)), this);
    lblTitle->setProperty("accessibleName", "dialog-title");
    lblTitle->setWordWrap(true);
    layout->addWidget(lblTitle);

    // Design Anchor: confirmation dialogs are one sentence maximum — the
    // SSD/copy-on-write caveat lives in the shredder view itself, not here.
    auto *lblBody = new QLabel(
        tr("This overwrites and permanently deletes the selected files — this cannot be undone."),
        this);
    lblBody->setWordWrap(true);
    layout->addWidget(lblBody);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();

    auto *btnCancel = new QPushButton(tr("Cancel"), this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(btnCancel);

    auto *btnShred = new QPushButton(tr("Shred"), this);
    btnShred->setCursor(Qt::PointingHandCursor);
    btnShred->setProperty("accessibleName", "danger");
    btnShred->setDefault(true);
    connect(btnShred, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(btnShred);

    layout->addLayout(buttons);
}
