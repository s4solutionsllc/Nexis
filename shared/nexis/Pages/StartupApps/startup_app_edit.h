#ifndef STARTUP_APP_EDIT_H
#define STARTUP_APP_EDIT_H

#include <QDialog>
#include <QScreen>
#include <QRegularExpression>

#include "Managers/app_manager.h"

// Linux .desktop file regex patterns (not used on macOS)
#ifndef Q_OS_MACOS
#define NAME_REG QRegularExpression("^Name=.*")
#define COMMENT_REG QRegularExpression("^Comment=.*")
#define EXEC_REG QRegularExpression("^Exec=.*")
#define GNOME_ENABLED_REG QRegularExpression("^X-GNOME-Autostart-enabled=.*")
#define HIDDEN_REG QRegularExpression("^Hidden=.*")
#define DELAY_REG QRegularExpression("^X-GNOME-Autostart-Delay=.*")
#endif

namespace Ui {
    class StartupAppEdit;
}

class StartupAppEdit : public QDialog
{
    Q_OBJECT

public:
    explicit StartupAppEdit(QWidget *parent = 0);
    ~StartupAppEdit();

public:
    static QString selectedFilePath;

signals:
    void startupAppAdded();

public slots:
    void show();

private slots:
    void init();
    bool isValid();
    void on_btnSave_clicked();
    void changeDesktopValue(QStringList &lines, const QRegularExpression &reg, const QString &text);
#ifdef Q_OS_MACOS
    QString buildPlistContent();
#endif

private:
    Ui::StartupAppEdit *ui;

private:
    QString mNewAppTemplate;
    QString mAutostartPath;
};

#endif // STARTUP_APP_EDIT_H
