#ifndef APTSourceRepositoryItem_H
#define APTSourceRepositoryItem_H

#include <QWidget>
#include <QResizeEvent>
#include "Managers/tool_manager.h"
#include <Tools/repo_health_types.h>
class QLabel;

namespace Ui {
class APTSourceRepositoryItem;
}

class APTSourceRepositoryItem : public QWidget
{
    Q_OBJECT

public:
    explicit APTSourceRepositoryItem(APTSourcePtr aptSource, QWidget *parent = 0);
    ~APTSourceRepositoryItem();

public:
    APTSourcePtr aptSource() const;
    void setHealthResult(const RepoHealthResult &result);

private:
    void updateStatusIndicator(RepoHealthResult::Status status);
    void refreshThemeColors();

private slots:
    void on_checkAptSource_clicked(bool checked);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void init();
    void elideDescription();

private:
    Ui::APTSourceRepositoryItem *ui;

    APTSourcePtr mAptSource;
    QLabel *mStatusDot = nullptr;
    QLabel *mLblDescription = nullptr;
    RepoHealthResult::Status mCurrentStatus = RepoHealthResult::Unknown;
    QString mFullDescription;
};

#endif
