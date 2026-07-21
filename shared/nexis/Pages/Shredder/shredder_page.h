#ifndef SHREDDER_PAGE_H
#define SHREDDER_PAGE_H

#include <QWidget>
#include <QStringList>

#include "Services/file_shredder_service.h"

class QLabel;
class QListWidget;
class QPushButton;
class QProgressBar;
class ShredderDropZone;

// SSO-15381: Secure File Shredder — drag-and-drop (+ file-picker fallback)
// target that stages files/folders, previews the exact item count + total
// size before anything is touched, gates the destructive action behind an
// explicit confirm dialog, and shows live progress while
// FileShredderService overwrites-then-unlinks the selection.
class ShredderPage : public QWidget
{
    Q_OBJECT

public:
    explicit ShredderPage(QWidget *parent = nullptr,
                          FileShredderService *shredderService = nullptr);
    ~ShredderPage() override;

private slots:
    void onAddFilesClicked();
    void onAddFolderClicked();
    void onShredClicked();
    void onPreviewReady(const ShredPlan &plan);
    void onShredProgress(int current, int total, quint64 bytesDone,
                         quint64 bytesTotal, const QString &currentPath);
    void onItemFailed(const QString &path, const QString &reason);
    void onShredFinished(int itemsShredded, int itemsFailed, quint64 bytesFreed);

private:
    void buildUi();
    void addPaths(const QStringList &paths);
    void removePath(const QString &path);
    void refreshStagedList();
    void requestPreview();
    void setBusy(bool busy);
    QString metaTextFor(const QString &path) const;

    static QWidget *makeElevatedContainer(QWidget *parent);

    FileShredderService *mShredderService;

    QStringList mStagedPaths;
    ShredPlan mLastPlan;
    QStringList mFailureLog;
    bool mPreviewInFlight = false;
    bool mPreviewPending = false;

    ShredderDropZone *mDropZone = nullptr;
    QListWidget *mListStaged = nullptr;
    QPushButton *mBtnAddFiles = nullptr;
    QPushButton *mBtnAddFolder = nullptr;
    QPushButton *mBtnShredSelected = nullptr;
    QLabel *mLblFooterTotal = nullptr;
    QLabel *mLblDisclosure = nullptr;
    QProgressBar *mProgressBar = nullptr;
    QLabel *mLblProgressStatus = nullptr;
};

#endif // SHREDDER_PAGE_H
