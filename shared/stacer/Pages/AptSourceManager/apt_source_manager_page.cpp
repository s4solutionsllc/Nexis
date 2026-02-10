#include "apt_source_manager_page.h"
#include "ui_apt_source_manager_page.h"
#include <QDebug>
#include "utilities.h"
#include "Managers/tool_manager.h"

APTSourceManagerPage::~APTSourceManagerPage()
{
    delete ui;
}

APTSourcePtr APTSourceManagerPage::selectedAptSource = nullptr;

APTSourceManagerPage::APTSourceManagerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::APTSourceManagerPage)
{
    ui->setupUi(this);

    init();
}

void APTSourceManagerPage::init()
{
#ifdef Q_OS_MAC
    // macOS: Homebrew packages instead of APT repositories
    ui->txtAptSource->setPlaceholderText(tr("example %1")
                                         .arg("'package-name'"));
    // Hide controls that don't apply to Homebrew packages
    ui->btnEditAptSource->hide();
    ui->checkEnableSource->hide();
    ui->lblAptSourceSelectInfo->setText(tr("Select to uninstall."));
#else
    ui->txtAptSource->setPlaceholderText(tr("example %1")
                                         .arg("'deb http://archive.ubuntu.com/ubuntu xenial main'"));
#endif

    loadAptSources();

    on_btnCancel_clicked();

    QList<QWidget*> widgets = {
        ui->btnAddAPTSourceRepository, ui->btnCancel, ui->btnDeleteAptSource,
        ui->txtSearchAptSource, ui->txtSearchAptSource
    };
#ifndef Q_OS_MAC
    widgets.append(ui->btnEditAptSource);
#endif
    Utilities::addDropShadow(widgets, 40);
}

void APTSourceManagerPage::loadAptSources()
{
    ui->listWidgetAptSources->clear();

    QList<APTSourcePtr> aptSourceList = ToolManager::ins()->getSourceList();

    for (APTSourcePtr &aptSource: aptSourceList) {

        QListWidgetItem *listItem = new QListWidgetItem(ui->listWidgetAptSources);
        // Store searchable text: name + uri + description
        QString searchData = aptSource->source + " " + aptSource->uri + " " + aptSource->components;
        listItem->setData(5, searchData);

        APTSourceRepositoryItem *aptSourceItem = new APTSourceRepositoryItem(aptSource, ui->listWidgetAptSources);

        listItem->setSizeHint(aptSourceItem->sizeHint() + QSize(0, 1));

        ui->listWidgetAptSources->setItemWidget(listItem, aptSourceItem);
    }

    ui->notFoundWidget->setVisible(aptSourceList.isEmpty());

#ifdef Q_OS_MAC
    ui->lblAptSourceTitle->setText(tr("Homebrew Packages (%1)")
                                   .arg(aptSourceList.count()));
    if (aptSourceList.isEmpty())
        ui->lblNotFound->setText(tr("No Homebrew Packages Found"));
#else
    ui->lblAptSourceTitle->setText(tr("APT Repositories (%1)")
                                   .arg(aptSourceList.count()));
#endif
}

void APTSourceManagerPage::on_btnAddAPTSourceRepository_clicked(bool checked)
{
    if (checked) {
        ui->btnAddAPTSourceRepository->setText(tr("Save"));
        changeElementsVisible(checked);
    } else {
        QString aptSourceRepository = ui->txtAptSource->text().trimmed();

        if (! aptSourceRepository.isEmpty()) {
            ToolManager::ins()->addAPTRepository(aptSourceRepository, ui->checkEnableSource->isChecked());

            ui->txtAptSource->clear();
            ui->checkEnableSource->setChecked(false);
            on_btnCancel_clicked();
            loadAptSources();
        }
    }
}

void APTSourceManagerPage::on_btnCancel_clicked()
{
    ui->btnAddAPTSourceRepository->setChecked(false);
    changeElementsVisible(false);
#ifdef Q_OS_MAC
    ui->btnAddAPTSourceRepository->setText(tr("Install"));
#else
    ui->btnAddAPTSourceRepository->setText(tr("Add Repository"));
#endif
}

void APTSourceManagerPage::changeElementsVisible(const bool checked)
{
    ui->txtAptSource->setVisible(checked);
    ui->btnCancel->setVisible(checked);
    ui->btnDeleteAptSource->setVisible(!checked);
    ui->bottomSectionHorizontalSpacer->changeSize(0, 0, checked ? QSizePolicy::Minimum : QSizePolicy::Expanding);
#ifdef Q_OS_MAC
    // Homebrew packages can't be edited or toggled — keep these hidden
    ui->checkEnableSource->setVisible(false);
    ui->btnEditAptSource->setVisible(false);
    ui->btnDeleteAptSource->setText(tr("Uninstall"));
#else
    ui->checkEnableSource->setVisible(checked);
    ui->btnEditAptSource->setVisible(!checked);
#endif
}

void APTSourceManagerPage::on_listWidgetAptSources_itemClicked(QListWidgetItem *item)
{
    QWidget *widget = ui->listWidgetAptSources->itemWidget(item);
    if (widget) {
        APTSourceRepositoryItem *aptSourceItem = dynamic_cast<APTSourceRepositoryItem*>(widget);
        if (aptSourceItem) {
            selectedAptSource = aptSourceItem->aptSource();
        }
    } else {
        selectedAptSource.clear();
    }
}

void APTSourceManagerPage::on_listWidgetAptSources_itemDoubleClicked(QListWidgetItem *item)
{
    on_listWidgetAptSources_itemClicked(item);
#ifndef Q_OS_MAC
    // Edit is not applicable for Homebrew packages
    on_btnEditAptSource_clicked();
#endif
}

void APTSourceManagerPage::on_btnDeleteAptSource_clicked()
{
    if (! selectedAptSource.isNull()) {
        ToolManager::ins()->removeAPTSource(selectedAptSource);
        loadAptSources();
    }
}

void APTSourceManagerPage::on_txtSearchAptSource_textChanged(const QString &val)
{
    for (int i = 0; i < ui->listWidgetAptSources->count(); ++i) {
        QListWidgetItem *item = ui->listWidgetAptSources->item(i);
        if (item) {
            bool isContain = item->data(5).toString().contains(val, Qt::CaseInsensitive);
            item->setHidden(! isContain);
        }
    }
}

void APTSourceManagerPage::on_btnEditAptSource_clicked()
{
    if (! selectedAptSource.isNull()) {
        if (mAptSourceEditDialog.isNull()) {
            mAptSourceEditDialog = QSharedPointer<APTSourceEdit>(new APTSourceEdit(this));
            connect(mAptSourceEditDialog.data(), &APTSourceEdit::saved, this, &APTSourceManagerPage::loadAptSources);
        }
        APTSourceEdit::selectedAptSource = selectedAptSource;
        mAptSourceEditDialog->show();
    }
}
