#ifndef STARTUPAPPSPAGE_H
#define STARTUPAPPSPAGE_H

#include <QWidget>
#include <QDebug>
#include <QLabel>
#include <QList>
#include <QListWidgetItem>
#include <QSharedPointer>
#include <QAbstractItemModel>

#include "startup_app.h"
#include "startup_app_edit.h"

#ifdef Q_OS_MACOS
class QPushButton;
#endif

class StartupService;

namespace Ui {
    class StartupAppsPage;
}

class StartupAppsPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartupAppsPage(QWidget *parent = nullptr,
                             StartupService *startupService = nullptr);
    ~StartupAppsPage();

public slots:
    void loadApps();

private slots:
    void init();
    void openStartupAppEdit(const QString filePath = QString());
    void setAppCount();
    void filterStartupApps(const QString &text);
#ifdef Q_OS_MACOS
    void onRepairBtmClicked();
#endif

private:
    Ui::StartupAppsPage *ui;

    QSharedPointer<StartupAppEdit> mStartupAppEdit;
    StartupService *mStartupService;

    struct SectionGroup {
        QListWidgetItem *headerItem = nullptr;
        QList<QListWidgetItem *> appItems;
        bool isBtmGroup = false;
    };
    QList<SectionGroup> mSectionGroups;
#ifdef Q_OS_MACOS
    QPushButton *mBtnRepairBtm = nullptr;
    int mBtmRowCount = 0;
#endif

    void addSectionHeader(const QString &title, bool isBtmGroup = false);
};

#endif // STARTUPAPPSPAGE_H
