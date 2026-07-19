#include "docker_page.h"
#include "ui_docker_page.h"
#include "utilities.h"
#include "dpi.h"
#include "Services/docker_service.h"
#include <Utils/format_util.h>
#include <QMessageBox>

DockerPage::DockerPage(QWidget *parent, DockerService *dockerService) :
    QWidget(parent),
    ui(new Ui::DockerPage),
    mDockerService(dockerService ? dockerService : DockerService::ins()),
    mDaemonRunning(false),
    mContainersLoaded(false),
    mVolumesLoaded(false)
{
    ui->setupUi(this);
    init();
}

DockerPage::~DockerPage()
{
    delete ui;
}

void DockerPage::init()
{
    setWindowTitle(tr("Docker"));

    ui->treeWidgetImages->header()->setFixedHeight(Dpi::scale(30));
    ui->treeWidgetImages->setHeaderLabels({tr("Image"), tr("Size"), tr("Created")});
    ui->treeWidgetImages->header()->setStretchLastSection(true);
    ui->treeWidgetImages->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeWidgetImages->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    // DS §7 / Design Anchor data-table convention: right-align the tabular Size column.
    ui->treeWidgetImages->headerItem()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

    // DS §2/§7: single elevated container around the Images tab tree; rows
    // inside stay flat (no per-row shadow). Containers/Volumes tabs are out
    // of scope (SSO-15097) and keep their existing self-contained tree look.
    ui->imagesContainer->setAttribute(Qt::WA_StyledBackground, true);
    ui->imagesContainer->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(ui->imagesContainer, 90, 26);

    ui->treeWidgetContainers->header()->setFixedHeight(Dpi::scale(30));
    ui->treeWidgetContainers->setHeaderLabels({tr("Container"), tr("Image"), tr("Status"), tr("Ports")});
    ui->treeWidgetContainers->header()->setStretchLastSection(true);
    ui->treeWidgetContainers->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->treeWidgetVolumes->header()->setFixedHeight(Dpi::scale(30));
    ui->treeWidgetVolumes->setHeaderLabels({tr("Volume"), tr("Driver"), tr("Mount Point")});
    ui->treeWidgetVolumes->header()->setStretchLastSection(true);
    ui->treeWidgetVolumes->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->cmbContainerFilter->addItems({tr("All"), tr("Running"), tr("Exited"), tr("Paused")});

    ui->stackedWidget->setCurrentIndex(0);

    ui->btnStartContainer->setVisible(false);
    ui->btnStopContainer->setVisible(false);

    Utilities::addDropShadow({ui->txtSearch, ui->btnRefresh, ui->btnRemoveSelected, ui->btnPrune}, 40);

    connect(mDockerService, &DockerService::daemonChecked,
            this, &DockerPage::onDaemonChecked);
    connect(mDockerService, &DockerService::imagesFetched,
            this, &DockerPage::onImagesLoaded);
    connect(mDockerService, &DockerService::containersFetched,
            this, &DockerPage::onContainersLoaded);
    connect(mDockerService, &DockerService::volumesFetched,
            this, &DockerPage::onVolumesLoaded);

    mDockerService->checkDaemon();
}

void DockerPage::onDaemonChecked(bool running, const QString &version)
{
    mDaemonRunning = running;
    setDaemonStatus(running, version);

    if (running) {
        ui->stackedWidget->setCurrentIndex(1);
        mDockerService->fetchImages();
    } else {
        ui->stackedWidget->setCurrentIndex(2);
    }
}

void DockerPage::setDaemonStatus(bool running, const QString &version)
{
    if (running) {
        QString label = tr("Docker daemon: Running");
        if (!version.isEmpty())
            label += QString(" (v%1)").arg(version);
        ui->lblDockerStatus->setText(label);
        ui->lblDockerStatus->setProperty("accessibleName", "success");
    } else {
        ui->lblDockerStatus->setText(tr("Docker daemon: Not running"));
        ui->lblDockerStatus->setProperty("accessibleName", "danger");
    }
    ui->lblDockerStatus->style()->unpolish(ui->lblDockerStatus);
    ui->lblDockerStatus->style()->polish(ui->lblDockerStatus);

    ui->btnRemoveSelected->setEnabled(running);
    ui->btnPrune->setEnabled(running);
}

void DockerPage::setStatusMessage(const QString &msg)
{
    ui->lblItemCount->setText(msg);
}

void DockerPage::onImagesLoaded(QList<DockerImage> images)
{
    mImages = images;
    buildImagesTree();

    int dangling = 0;
    qint64 danglingBytes = 0;
    for (const DockerImage &img : mImages) {
        if (img.isDangling) {
            dangling++;
            danglingBytes += img.sizeBytes;
        }
    }

    QString status = tr("%1 images").arg(mImages.size());
    if (dangling > 0)
        status += tr(" (%1 dangling, %2 reclaimable)")
                    .arg(dangling)
                    .arg(FormatUtil::formatBytes(danglingBytes));
    setStatusMessage(status);
}

void DockerPage::onContainersLoaded(QList<DockerContainer> containers)
{
    mContainers = containers;
    mContainersLoaded = true;
    buildContainersTree();

    int running = 0;
    for (const DockerContainer &c : mContainers) {
        if (c.state == "running")
            running++;
    }
    setStatusMessage(tr("%1 containers (%2 running)").arg(mContainers.size()).arg(running));
}

void DockerPage::onVolumesLoaded(QList<DockerVolume> volumes)
{
    mVolumes = volumes;
    mVolumesLoaded = true;
    buildVolumesTree();

    int unused = 0;
    for (const DockerVolume &v : mVolumes) {
        if (!v.isUsed)
            unused++;
    }
    setStatusMessage(tr("%1 volumes (%2 unused)").arg(mVolumes.size()).arg(unused));
}

void DockerPage::buildImagesTree()
{
    ui->treeWidgetImages->blockSignals(true);
    ui->treeWidgetImages->clear();

    QList<DockerImage> inUse, dangling, other;
    for (const DockerImage &img : mImages) {
        if (img.isDangling) dangling.append(img);
        else if (img.isUsed) inUse.append(img);
        else other.append(img);
    }

    auto addSection = [&](const QString &title, const QList<DockerImage> &items) {
        if (items.isEmpty()) return;
        auto *section = new QTreeWidgetItem(ui->treeWidgetImages);
        section->setText(0, QString("%1 (%2)").arg(title).arg(items.size()));
        section->setFlags(Qt::ItemIsEnabled);
        QFont f = section->font(0);
        f.setBold(true);
        section->setFont(0, f);

        for (const DockerImage &img : items) {
            auto *item = new QTreeWidgetItem(section);
            QString display = img.isDangling ? QString("<none>:<none>") : QString("%1:%2").arg(img.repository, img.tag);
            item->setText(0, display);
            item->setText(1, img.size);
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            item->setText(2, img.createdAt);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, img.id);
        }
        section->setExpanded(true);
    };

    addSection(tr("In Use"), inUse);
    addSection(tr("Dangling"), dangling);
    addSection(tr("Other"), other);

    ui->treeWidgetImages->blockSignals(false);
}

void DockerPage::buildContainersTree()
{
    ui->treeWidgetContainers->blockSignals(true);
    ui->treeWidgetContainers->clear();

    QString filter = ui->cmbContainerFilter->currentText().toLower();

    QMap<QString, QList<DockerContainer>> grouped;
    for (const DockerContainer &c : mContainers) {
        if (filter != tr("all").toLower() && c.state != filter)
            continue;
        QString key = c.state.left(1).toUpper() + c.state.mid(1);
        grouped[key].append(c);
    }

    QStringList order = {"Running", "Exited", "Paused", "Created"};
    for (const QString &state : order) {
        if (!grouped.contains(state))
            continue;
        const QList<DockerContainer> &items = grouped[state];

        auto *section = new QTreeWidgetItem(ui->treeWidgetContainers);
        section->setText(0, QString("%1 (%2)").arg(state).arg(items.size()));
        section->setFlags(Qt::ItemIsEnabled);
        QFont f = section->font(0);
        f.setBold(true);
        section->setFont(0, f);

        for (const DockerContainer &c : items) {
            auto *item = new QTreeWidgetItem(section);
            item->setText(0, c.name);
            item->setText(1, c.image);
            item->setText(2, c.status);
            item->setText(3, c.ports);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, c.id);
            item->setData(0, Qt::UserRole + 1, c.state);
        }
        section->setExpanded(true);
    }

    ui->treeWidgetContainers->blockSignals(false);
}

void DockerPage::buildVolumesTree()
{
    ui->treeWidgetVolumes->blockSignals(true);
    ui->treeWidgetVolumes->clear();

    QList<DockerVolume> used, unused;
    for (const DockerVolume &v : mVolumes) {
        if (v.isUsed) used.append(v);
        else unused.append(v);
    }

    auto addSection = [&](const QString &title, const QList<DockerVolume> &items) {
        if (items.isEmpty()) return;
        auto *section = new QTreeWidgetItem(ui->treeWidgetVolumes);
        section->setText(0, QString("%1 (%2)").arg(title).arg(items.size()));
        section->setFlags(Qt::ItemIsEnabled);
        QFont f = section->font(0);
        f.setBold(true);
        section->setFont(0, f);

        for (const DockerVolume &v : items) {
            auto *item = new QTreeWidgetItem(section);
            item->setText(0, v.name);
            item->setText(1, v.driver);
            item->setText(2, v.mountpoint);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, Qt::UserRole, v.name);
        }
        section->setExpanded(true);
    };

    addSection(tr("In Use"), used);
    addSection(tr("Unused"), unused);

    ui->treeWidgetVolumes->blockSignals(false);
}

QStringList DockerPage::getSelectedImageIds()
{
    QStringList ids;
    for (int i = 0; i < ui->treeWidgetImages->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetImages->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                ids << item->data(0, Qt::UserRole).toString();
        }
    }
    return ids;
}

QStringList DockerPage::getSelectedContainerIds()
{
    QStringList ids;
    for (int i = 0; i < ui->treeWidgetContainers->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetContainers->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                ids << item->data(0, Qt::UserRole).toString();
        }
    }
    return ids;
}

QStringList DockerPage::getSelectedVolumeNames()
{
    QStringList names;
    for (int i = 0; i < ui->treeWidgetVolumes->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = ui->treeWidgetVolumes->topLevelItem(i);
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            if (item->checkState(0) == Qt::Checked)
                names << item->data(0, Qt::UserRole).toString();
        }
    }
    return names;
}

void DockerPage::on_txtSearch_textChanged(const QString &text)
{
    int tabIdx = ui->tabWidget->currentIndex();
    QTreeWidget *tree = nullptr;
    if (tabIdx == 0) tree = ui->treeWidgetImages;
    else if (tabIdx == 1) tree = ui->treeWidgetContainers;
    else if (tabIdx == 2) tree = ui->treeWidgetVolumes;
    if (!tree) return;

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *section = tree->topLevelItem(i);
        int visibleChildren = 0;
        for (int j = 0; j < section->childCount(); ++j) {
            QTreeWidgetItem *item = section->child(j);
            bool match = text.isEmpty();
            if (!match) {
                for (int col = 0; col < item->columnCount(); ++col) {
                    if (item->text(col).contains(text, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            item->setHidden(!match);
            if (match) visibleChildren++;
        }
        section->setHidden(visibleChildren == 0 && !text.isEmpty());
    }
}

void DockerPage::on_tabWidget_currentChanged(int index)
{
    ui->btnStartContainer->setVisible(index == 1);
    ui->btnStopContainer->setVisible(index == 1);
    ui->cmbContainerFilter->setVisible(index == 1);

    if (index == 1 && !mContainersLoaded && mDaemonRunning) {
        mDockerService->fetchContainers();
    } else if (index == 2 && !mVolumesLoaded && mDaemonRunning) {
        mDockerService->fetchVolumes();
    }

    on_txtSearch_textChanged(ui->txtSearch->text());
}

void DockerPage::on_cmbContainerFilter_currentIndexChanged(int /*index*/)
{
    if (mContainersLoaded)
        buildContainersTree();
}

void DockerPage::on_btnRefresh_clicked()
{
    mDockerService->checkDaemon();
    if (mDaemonRunning) {
        int tab = ui->tabWidget->currentIndex();
        if (tab == 0)
            mDockerService->fetchImages();
        else if (tab == 1) {
            mContainersLoaded = false;
            mDockerService->fetchContainers();
        } else if (tab == 2) {
            mVolumesLoaded = false;
            mDockerService->fetchVolumes();
        }
    }
}

void DockerPage::on_btnRemoveSelected_clicked()
{
    int tab = ui->tabWidget->currentIndex();

    if (tab == 0) {
        QStringList ids = getSelectedImageIds();
        if (ids.isEmpty()) return;
        if (QMessageBox::warning(this, tr("Remove Images"),
                tr("Remove %1 selected image(s)? This cannot be undone.").arg(ids.size()),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        mDockerService->removeImages(ids);
    } else if (tab == 1) {
        QStringList ids = getSelectedContainerIds();
        if (ids.isEmpty()) return;
        if (QMessageBox::warning(this, tr("Remove Containers"),
                tr("Remove %1 selected container(s)? This cannot be undone.").arg(ids.size()),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        mDockerService->removeContainers(ids);
    } else if (tab == 2) {
        QStringList names = getSelectedVolumeNames();
        if (names.isEmpty()) return;
        if (QMessageBox::warning(this, tr("Remove Volumes"),
                tr("Remove %1 selected volume(s)? All data in these volumes will be permanently lost.").arg(names.size()),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        mDockerService->removeVolumes(names);
    }
}

void DockerPage::on_btnPrune_clicked()
{
    int tab = ui->tabWidget->currentIndex();

    if (tab == 0) {
        if (QMessageBox::warning(this, tr("Prune Images"),
                tr("Remove all dangling (unused) images? This cannot be undone."),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        mDockerService->pruneImages();
    } else if (tab == 1) {
        if (QMessageBox::warning(this, tr("Prune Containers"),
                tr("Remove all stopped containers? This cannot be undone."),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        mDockerService->pruneContainers();
    } else if (tab == 2) {
        if (QMessageBox::warning(this, tr("Prune Volumes"),
                tr("Remove all unused volumes? All data in these volumes will be permanently lost."),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        mDockerService->pruneVolumes();
    }
}

void DockerPage::on_btnStartContainer_clicked()
{
    QStringList ids = getSelectedContainerIds();
    if (ids.isEmpty()) return;
    mDockerService->startContainers(ids);
}

void DockerPage::on_btnStopContainer_clicked()
{
    QStringList ids = getSelectedContainerIds();
    if (ids.isEmpty()) return;
    mDockerService->stopContainers(ids);
}
