#include "stacer_import_dialog.h"
#include "ui_stacer_import_dialog.h"

#include <QAbstractButton>
#include <QListWidgetItem>

StacerImportDialog::StacerImportDialog(const StacerImportResult &result, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StacerImportDialog)
{
    ui->setupUi(this);
    setModal(true);
    populate(result);

    // Wire Apply button to accept() so the caller can detect acceptance.
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *btn) {
        if (ui->buttonBox->buttonRole(btn) == QDialogButtonBox::ApplyRole)
            accept();
    });
}

StacerImportDialog::~StacerImportDialog()
{
    delete ui;
}

void StacerImportDialog::populate(const StacerImportResult &result)
{
    const int total = result.willChange.size()
                    + result.alreadyMatch.size()
                    + result.noEquivalent.size();

    const int imported = result.willChange.size();
    const int noEq     = result.noEquivalent.size();

    ui->lblSummary->setText(
        tr("%1 of %2 settings will be imported; %3 have no Nexis equivalent.")
            .arg(imported).arg(total).arg(noEq));

    if (result.willChange.isEmpty()) {
        auto *item = new QListWidgetItem(tr("(no settings differ from current values)"));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        ui->listWillChange->addItem(item);
    } else {
        for (const StacerMappedEntry &entry : result.willChange) {
            const QString text = QString("%1: %2 → %3")
                .arg(entry.label, entry.fromValue.isEmpty() ? tr("(not set)") : entry.fromValue,
                     entry.toValue);
            ui->listWillChange->addItem(text);
        }
    }

    if (noEq > 0) {
        ui->lblNotImported->setText(
            tr("%1 setting(s) not imported (no Nexis equivalent): %2.")
                .arg(noEq)
                .arg(result.noEquivalent.join(", ")));
    } else {
        ui->lblNotImported->hide();
    }

    // Disable Apply if nothing will change.
    if (result.willChange.isEmpty()) {
        for (QAbstractButton *btn : ui->buttonBox->buttons()) {
            if (ui->buttonBox->buttonRole(btn) == QDialogButtonBox::ApplyRole)
                btn->setEnabled(false);
        }
    }
}
