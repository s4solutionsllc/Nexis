#include "btm_reset_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

BtmResetDialog::BtmResetDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("BtmResetDialog"));
    setWindowTitle(tr("Repair Background Task Management"));
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(20, 20, 20, 20);

    auto *header = new QLabel(tr("Reset the Background Task Management database?"), this);
    header->setObjectName(QStringLiteral("lblBtmResetHeader"));
    header->setWordWrap(true);

    auto *body = new QLabel(this);
    body->setObjectName(QStringLiteral("lblBtmResetBody"));
    body->setWordWrap(true);
    body->setText(tr(
        "Running <code>sudo sfltool resetbtm</code> clears the macOS Background "
        "Task Management database. This is the standard repair when Login Items "
        "fail to launch or <code>backgroundtaskmanagementd</code> is stuck at "
        "high CPU.\n\n"
        "Side effects:\n"
        "• Every Login Item, Launch Agent and Launch Daemon will re-prompt the "
        "user for permission via Notification Center on next login.\n"
        "• Items you previously disabled will be re-enabled at their default "
        "state and may need to be disabled again.\n"
        "• MDM-managed items are re-evaluated against the current profile.\n"
        "• Daemons may briefly fail to start while the system rebuilds state.\n\n"
        "Type <b>RESET</b> below to enable the repair button."));

    auto *prompt = new QLabel(tr("Confirmation:"), this);
    mConfirmEdit = new QLineEdit(this);
    mConfirmEdit->setObjectName(QStringLiteral("txtBtmResetConfirm"));
    mConfirmEdit->setPlaceholderText(tr("Type RESET to confirm"));

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    mResetBtn = btns->addButton(tr("Run sfltool resetbtm"),
                                QDialogButtonBox::DestructiveRole);
    mResetBtn->setObjectName(QStringLiteral("btnBtmResetRun"));
    mResetBtn->setEnabled(false);
    mResetBtn->setAutoDefault(false);
    btns->button(QDialogButtonBox::Cancel)->setDefault(true);

    connect(mConfirmEdit, &QLineEdit::textChanged,
            this, &BtmResetDialog::onConfirmTextChanged);
    connect(mResetBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(header);
    root->addWidget(body);
    root->addSpacing(6);
    root->addWidget(prompt);
    root->addWidget(mConfirmEdit);
    root->addWidget(btns);
}

void BtmResetDialog::onConfirmTextChanged(const QString &text)
{
    mResetBtn->setEnabled(text.trimmed() == QLatin1String(kConfirmToken));
}
