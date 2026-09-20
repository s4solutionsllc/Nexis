#include "shred_confirm_dialog.h"
#include "Common/dialog_buttons.h"

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
    layout->setContentsMargins(DialogButtons::dialogMargins());
    layout->setSpacing(DialogButtons::dialogSpacing());

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

    DialogButtons::Row row = DialogButtons::build(this, tr("Shred"), DialogButtons::Confirm::Danger, tr("Cancel"));
    connect(row.confirm, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(row.box);
}
