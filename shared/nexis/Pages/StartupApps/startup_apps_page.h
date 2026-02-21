#ifndef STARTUPAPPSPAGE_H
#define STARTUPAPPSPAGE_H

#include <QWidget>
#include <QDebug>
#include <QSharedPointer>
#include <QAbstractItemModel>

#include "startup_app.h"
#include "startup_app_edit.h"

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

private:
    Ui::StartupAppsPage *ui;

private:
    QSharedPointer<StartupAppEdit> mStartupAppEdit;
    StartupService *mStartupService;
};

#endif // STARTUPAPPSPAGE_H
