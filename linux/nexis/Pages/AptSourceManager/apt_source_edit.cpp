#include "apt_source_edit.h"
#include "ui_apt_source_edit.h"

#include <QDebug>

APTSourceEdit::~APTSourceEdit()
{
    delete ui;
}

APTSourcePtr APTSourceEdit::selectedAptSource = nullptr;

APTSourceEdit::APTSourceEdit(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::APTSourceEdit)
{
    ui->setupUi(this);

    init();
}

void APTSourceEdit::init()
{
    ui->lblErrorMsg->hide();
}

void APTSourceEdit::show()
{
    clearElements();

    // example deb822 stanza or legacy:
    //   deb [arch=amd64 signed-by=/etc/apt/keyrings/example.asc] \
    //       http://packages.microsoft.com/repos/vscode stable main

    // set values to elements
    ui->radioBinary->setChecked(! selectedAptSource->isSource);
    ui->radioSource->setChecked(selectedAptSource->isSource);
    ui->txtOptions->setText(selectedAptSource->options);
    ui->txtUri->setText(selectedAptSource->uri);
    ui->txtSuites->setText(selectedAptSource->suites);
    ui->txtComponents->setText(selectedAptSource->components);
    // SSO-3728 / FW-01: surface Signed-By + Architectures so the user can
    // edit a deb822 entry's keyring path and arch list without hand-editing
    // /etc/apt/sources.list.d/*.sources.
    ui->txtSignedBy->setText(selectedAptSource->signedByPath);
    ui->txtArchitectures->setText(selectedAptSource->architectures);
    // Options are only meaningful in legacy .list format; hide for deb822
    // since Signed-By / Architectures replace the [arch=..., signed-by=...]
    // brackets.
    const bool isDeb822 = selectedAptSource->format == APTSource::Deb822;
    ui->txtOptions->setVisible(!isDeb822);

    QDialog::show();
}

void APTSourceEdit::clearElements()
{
    ui->lblErrorMsg->hide();
    ui->txtOptions->clear();
    ui->txtUri->clear();
    ui->txtSuites->clear();
    ui->txtComponents->clear();
    ui->txtSignedBy->clear();
    ui->txtArchitectures->clear();
}

void APTSourceEdit::on_btnSave_clicked()
{
    if (!ui->txtUri->text().isEmpty() && !ui->txtSuites->text().isEmpty()) {
        APTSourcePtr updatedAptSource(new APTSource(*selectedAptSource));
        updatedAptSource->isSource = ui->radioSource->isChecked();
        updatedAptSource->options = ui->txtOptions->text();
        updatedAptSource->uri = ui->txtUri->text();
        updatedAptSource->suites = ui->txtSuites->text();
        updatedAptSource->components = ui->txtComponents->text();
        updatedAptSource->signedByPath = ui->txtSignedBy->text().trimmed();
        updatedAptSource->architectures = ui->txtArchitectures->text().trimmed();

        ToolManager::ins()->changeAPTSource(selectedAptSource, updatedAptSource);

        emit saved();
        close();
    } else {
        ui->lblErrorMsg->show();
    }
}

void APTSourceEdit::on_btnCancel_clicked()
{
    close();
}
