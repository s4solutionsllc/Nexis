#ifndef EXCLUSION_MANAGER_DIALOG_H
#define EXCLUSION_MANAGER_DIALOG_H

#include <QDialog>
#include <Managers/cleaner_service.h>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QToolButton;
class AppManager;

class ExclusionManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExclusionManagerDialog(QWidget *parent = nullptr,
                                    AppManager *appManager = nullptr);

private slots:
    void onAddFile();
    void onAddFolder();
    void onRemoveSelected();

private:
    void buildUI();
    void refreshList();

    AppManager *mAppManager;
    QLabel *mLblTitle;
    QLabel *mLblNotice;
    QTreeWidget *mTree;
    QToolButton *mBtnAddFile;
    QToolButton *mBtnAddFolder;
    QToolButton *mBtnRemove;
    QPushButton *mBtnClose;
};

#endif // EXCLUSION_MANAGER_DIALOG_H
