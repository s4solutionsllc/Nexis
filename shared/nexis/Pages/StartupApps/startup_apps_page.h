#ifndef STARTUPAPPSPAGE_H
#define STARTUPAPPSPAGE_H

#include <QWidget>
#include <QDebug>
#include <QLabel>
#include <QList>
#include <QListWidgetItem>
#include <QResizeEvent>
#include <QSet>
#include <QSharedPointer>
#include <QAbstractItemModel>

#include <Utils/persistence_watch.h>

#include "startup_app.h"
#include "startup_app_edit.h"

#ifdef Q_OS_MACOS
class QPushButton;
#endif

class QFrame;
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
    void onStartupAppSaved();
    void onAcknowledgeClicked();
#ifdef Q_OS_MACOS
    void onRepairBtmClicked();
#endif

#ifdef Q_OS_MACOS
protected:
    // SSO-25047: BtmRow elides name/subtext to the list viewport width
    // (setAvailableWidth), so a viewport-width change needs to re-elide and
    // re-apply each row's QListWidgetItem sizeHint.
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
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

    // SSO-27149: persistence watch. mItems is what the last load displayed
    // (login items + cron), mFlaggedKeys the subset new/changed since the
    // acknowledged baseline.
    QFrame *mPersistenceBanner = nullptr;
    QLabel *mPersistenceBannerText = nullptr;
    QList<StartupAppData> mItems;
    QSet<QString> mFlaggedKeys;
    void updatePersistenceBanner(const PersistenceDiff &diff);
    QList<StartupAppData> collectItems() const;
#ifdef Q_OS_MACOS
    QPushButton *mBtnRepairBtm = nullptr;
    int mBtmRowCount = 0;
#endif

    void addSectionHeader(const QString &title, bool isBtmGroup = false);
#ifdef Q_OS_MACOS
    void reflowBtmRows();
#endif
};

#endif // STARTUPAPPSPAGE_H
