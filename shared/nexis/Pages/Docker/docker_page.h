#ifndef DOCKERPAGE_H
#define DOCKERPAGE_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QtConcurrent>

#include "Managers/tool_manager.h"

namespace Ui {
    class DockerPage;
}

class DockerPage : public QWidget
{
    Q_OBJECT

public:
    explicit DockerPage(QWidget *parent = nullptr);
    ~DockerPage();

signals:
    void imagesLoadedS();
    void containersLoadedS();
    void volumesLoadedS();
    void daemonCheckedS(bool running, const QString &version);

private:
    void init();

    void fetchImages();
    void fetchContainers();
    void fetchVolumes();

    void buildImagesTree();
    void buildContainersTree();
    void buildVolumesTree();

    QStringList getSelectedImageIds();
    QStringList getSelectedContainerIds();
    QStringList getSelectedVolumeNames();

    void setDaemonStatus(bool running, const QString &version);
    void setStatusMessage(const QString &msg);
    void refreshCurrentTab();

private slots:
    void onImagesLoaded();
    void onContainersLoaded();
    void onVolumesLoaded();
    void onDaemonChecked(bool running, const QString &version);

    void on_txtSearch_textChanged(const QString &text);
    void on_tabWidget_currentChanged(int index);
    void on_cmbContainerFilter_currentIndexChanged(int index);

    void on_btnRefresh_clicked();
    void on_btnRemoveSelected_clicked();
    void on_btnPrune_clicked();
    void on_btnStartContainer_clicked();
    void on_btnStopContainer_clicked();

private:
    Ui::DockerPage *ui;

    QList<DockerImage> mImages;
    QList<DockerContainer> mContainers;
    QList<DockerVolume> mVolumes;

    QFuture<void> mFetchImagesFuture;
    QFuture<void> mFetchContainersFuture;
    QFuture<void> mFetchVolumesFuture;

    bool mDaemonRunning;
    bool mContainersLoaded;
    bool mVolumesLoaded;
};

#endif // DOCKERPAGE_H
