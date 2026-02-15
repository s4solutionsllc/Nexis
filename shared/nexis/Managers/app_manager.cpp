#include "app_manager.h"
#include <QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

AppManager *AppManager::instance = nullptr;

AppManager *AppManager::ins()
{
    if (! instance) {
        instance = new AppManager;
    }

    return instance;
}

AppManager::AppManager()
    : mStyleValues(nullptr)
{
    mSettingManager = SettingManager::ins();

    mTrayIcon = new QSystemTrayIcon(QIcon(":/static/tray-icon.svg"));

    loadLanguageList();

    if (mTranslator.load(QString("nexis_%1").arg(mSettingManager->getLanguage()), qApp->applicationDirPath() + "/translations")) {
        qApp->installTranslator(&mTranslator);
        (mSettingManager->getLanguage() == "ar") ? qApp->setLayoutDirection(Qt::RightToLeft) : qApp->setLayoutDirection(Qt::LeftToRight);
    }

    // Live-switch when the system color scheme changes (Qt 6.5+)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                     qApp, [this](Qt::ColorScheme) {
                         if (mSettingManager->getColorScheme() == "auto") {
                             updateStylesheet();
                         }
                     });
#endif
}

QSystemTrayIcon *AppManager::getTrayIcon()
{
    return mTrayIcon;
}

QSettings *AppManager::getStyleValues() const
{
    return mStyleValues;
}

void AppManager::loadLanguageList()
{
    QByteArray languagesJson = FileUtil::readStringFromFile(":/static/languages.json").toUtf8();
    QJsonArray languages = QJsonDocument::fromJson(languagesJson).array();

    for (int i = 0; i < languages.count(); ++i) {

        QJsonObject ob = languages.at(i).toObject();

        mLanguageList.insert(ob["value"].toString(), ob["text"].toString());
    }
}

QMap<QString, QString> AppManager::getLanguageList() const
{
    return mLanguageList;
}

QString AppManager::resolveThemeName() const
{
    QString scheme = mSettingManager->getColorScheme();

    if (scheme == "light") return "light";
    if (scheme == "dark")  return "default";

    // "auto" – detect system preference
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    Qt::ColorScheme sys = QGuiApplication::styleHints()->colorScheme();
    if (sys == Qt::ColorScheme::Light) return "light";
#endif
    return "default";
}

void AppManager::updateStylesheet()
{
    QString themeName = resolveThemeName();

    // Color values come from the theme-specific folder
    delete mStyleValues;
    mStyleValues = new QSettings(
        QString(":/static/themes/%1/style/values.ini").arg(themeName),
        QSettings::IniFormat);

    // Single QSS template – always from "default" which uses @token placeholders
    mStylesheetFileContent = FileUtil::readStringFromFile(
        QStringLiteral(":/static/themes/default/style/style.qss"));

    // Replace @tokens with values
    for (const QString &key : mStyleValues->allKeys()) {
        mStylesheetFileContent.replace(key, mStyleValues->value(key).toString());
    }

    qApp->setStyleSheet(mStylesheetFileContent);

    emit SignalMapper::ins()->sigChangedAppTheme();
}

QString AppManager::getStylesheetFileContent() const
{
    return mStylesheetFileContent;
}
