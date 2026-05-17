#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <QApplication>
#include <QMap>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTranslator>
#include <QSystemTrayIcon>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#include "Utils/file_util.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"

class AppManager
{

public:
    static AppManager *ins();

    QMap<QString, QString> getLanguageList() const;
    void loadLanguageList();

    void updateStylesheet();
    QString getStylesheetFileContent() const;

    // SSO-381: swap the four SVG indicator URLs in a QSS string for their
    // PNG siblings. Called from updateStylesheet() only when the Qt SVG
    // image plugin is missing (e.g. Linux Mint 22's default seed). Exposed
    // here so the substitution can be unit-tested without a QApplication.
    static QString applyIndicatorPngFallback(const QString &qss);

    QSettings *getStyleValues() const;

    QSystemTrayIcon *getTrayIcon();
    void updateTrayIcon();

    QString resolveThemeName() const;

private:
    static AppManager *instance;
    AppManager();

private:
    QTranslator mTranslator;
    QSystemTrayIcon *mTrayIcon;

    QSettings *mStyleValues;

    QMap<QString, QString> mLanguageList;
    QString mStylesheetFileContent;

    SettingManager *mSettingManager;
};

#endif // APP_MANAGER_H
