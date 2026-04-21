#include "process_prefs_manager.h"

#include "setting_manager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ProcessPrefsManager *ProcessPrefsManager::instance = nullptr;

ProcessPrefsManager *ProcessPrefsManager::ins()
{
    if (!instance)
        instance = new ProcessPrefsManager;
    return instance;
}

ProcessPrefsManager::ProcessPrefsManager()
{
    load();
}

void ProcessPrefsManager::load()
{
    SettingManager *sm = SettingManager::ins();

    const QJsonArray names = QJsonDocument::fromJson(
        sm->getProcessPinnedNames().toUtf8()).array();
    for (const QJsonValue &v : names) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty())
            mPinnedNames.insert(s);
    }

    const QJsonArray ts = QJsonDocument::fromJson(
        sm->getProcessThresholds().toUtf8()).array();
    for (const QJsonValue &v : ts) {
        const QJsonObject obj = v.toObject();
        Threshold t;
        t.name = obj.value("name").toString();
        t.cpuPercent = obj.value("cpuPercent").toInt(0);
        t.memoryBytes = static_cast<qint64>(
            obj.value("memoryBytes").toVariant().toLongLong());
        if (!t.name.isEmpty())
            mThresholds.append(t);
    }
}

void ProcessPrefsManager::saveNames()
{
    QJsonArray arr;
    for (const QString &s : mPinnedNames)
        arr.append(s);
    SettingManager::ins()->setProcessPinnedNames(
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ProcessPrefsManager::saveThresholds()
{
    QJsonArray arr;
    for (const Threshold &t : mThresholds) {
        QJsonObject obj;
        obj.insert("name", t.name);
        obj.insert("cpuPercent", t.cpuPercent);
        obj.insert("memoryBytes", QJsonValue::fromVariant(
            QVariant::fromValue<qint64>(t.memoryBytes)));
        arr.append(obj);
    }
    SettingManager::ins()->setProcessThresholds(
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QStringList ProcessPrefsManager::pinnedNames() const
{
    return QStringList(mPinnedNames.values());
}

bool ProcessPrefsManager::isPinned(const QString &name) const
{
    return mPinnedNames.contains(name);
}

void ProcessPrefsManager::setPinned(const QString &name, bool pinned)
{
    if (name.isEmpty())
        return;
    const bool was = mPinnedNames.contains(name);
    if (pinned == was)
        return;
    if (pinned)
        mPinnedNames.insert(name);
    else
        mPinnedNames.remove(name);
    saveNames();
    emit changed();
}

QList<ProcessPrefsManager::Threshold> ProcessPrefsManager::thresholds() const
{
    return mThresholds;
}

ProcessPrefsManager::Threshold ProcessPrefsManager::threshold(const QString &name) const
{
    for (const Threshold &t : mThresholds)
        if (t.name == name)
            return t;
    return Threshold{};
}

bool ProcessPrefsManager::hasThreshold(const QString &name) const
{
    for (const Threshold &t : mThresholds)
        if (t.name == name)
            return true;
    return false;
}

void ProcessPrefsManager::setThreshold(const Threshold &t)
{
    if (t.name.isEmpty())
        return;
    // Empty threshold (both fields 0) == removal.
    if (t.cpuPercent <= 0 && t.memoryBytes <= 0) {
        removeThreshold(t.name);
        return;
    }
    bool found = false;
    for (int i = 0; i < mThresholds.size(); ++i) {
        if (mThresholds.at(i).name == t.name) {
            mThresholds[i] = t;
            found = true;
            break;
        }
    }
    if (!found)
        mThresholds.append(t);
    saveThresholds();
    emit changed();
}

void ProcessPrefsManager::removeThreshold(const QString &name)
{
    bool removed = false;
    for (int i = mThresholds.size() - 1; i >= 0; --i) {
        if (mThresholds.at(i).name == name) {
            mThresholds.removeAt(i);
            removed = true;
        }
    }
    if (removed) {
        saveThresholds();
        emit changed();
    }
}
