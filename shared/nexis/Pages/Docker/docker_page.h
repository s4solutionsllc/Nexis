#ifndef DOCKERPAGE_H
#define DOCKERPAGE_H

#include <QWidget>
#include <QTreeWidgetItem>

#include <Tools/docker_tool.h>

class DockerService;

namespace Ui {
    class DockerPage;
}

class DockerPage : public QWidget
{
    Q_OBJECT

public:
    explicit DockerPage(QWidget *parent = nullptr,
                        DockerService *dockerService = nullptr);
    ~DockerPage();

private:
    void init();

    void buildImagesTree();
    void buildContainersTree();
    void buildVolumesTree();

    QStringList getSelectedImageIds();
    QStringList getSelectedContainerIds();
    QStringList getSelectedVolumeNames();

    void setDaemonStatus(bool running, const QString &version);
    void setStatusMessage(const QString &msg);

private slots:
    void onDaemonChecked(bool running, const QString &version);
    void onImagesLoaded(QList<DockerImage> images);
    void onContainersLoaded(QList<DockerContainer> containers);
    void onVolumesLoaded(QList<DockerVolume> volumes);

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
    DockerService *mDockerService;

    QList<DockerImage> mImages;
    QList<DockerContainer> mContainers;
    QList<DockerVolume> mVolumes;

    bool mDaemonRunning;
    bool mContainersLoaded;
    bool mVolumesLoaded;
};

#endif // DOCKERPAGE_H
