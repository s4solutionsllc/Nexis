#ifndef SIDEBAR_SECTION_H
#define SIDEBAR_SECTION_H

#include <QString>
#include <QList>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

class QPushButton;
class QWidget;
class QVBoxLayout;

// One collapsible group in the sidebar nav (e.g. "MANAGE"). MONITOR is the
// only headerless section — always expanded, no toggle. Also the single
// source of truth for tray menu grouping (SSO-23896): the tray derives its
// structure from this, so adding a sidebar page never needs a tray-side edit.
struct SidebarSection {
    // Stable, untranslated key for persisted state; `name` is display text.
    QString id;
    QString name;
    QPushButton *header = nullptr;
    QWidget *container = nullptr;
    QVBoxLayout *containerLayout = nullptr;
    QList<QPushButton*> buttons;
    bool collapsed = false;
    bool headerless = false;
};

// Persistence of the collapsed/expanded state, kept free of widgets so it can
// be unit-tested. State is keyed on SidebarSection::id; keys written by older
// builds (the translated header text) are accepted once and reported via
// `migrated` so the caller can re-save under stable ids.
namespace SidebarSectionState {

struct Key { QString id; QString name; bool headerless = false; };

inline QString toJson(const QList<Key> &keys, const QHash<QString, bool> &collapsedById)
{
    QJsonObject obj;
    for (const Key &key : keys) {
        if (!key.headerless)
            obj[key.id] = collapsedById.value(key.id, false);
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// `inheritsFrom` maps a section id introduced by a regrouping to the id it was
// split out of, so a group the user had collapsed stays collapsed.
inline QHash<QString, bool> fromJson(const QString &json, const QList<Key> &keys, bool *migrated = nullptr,
                                     const QHash<QString, QString> &inheritsFrom = {})
{
    QHash<QString, bool> result;
    if (migrated)
        *migrated = false;
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    for (const Key &key : keys) {
        if (key.headerless)
            continue;
        if (obj.contains(key.id)) {
            result.insert(key.id, obj.value(key.id).toBool());
        } else if (obj.contains(key.name)) {
            result.insert(key.id, obj.value(key.name).toBool());
            if (migrated)
                *migrated = true;
        }
    }
    for (auto it = inheritsFrom.constBegin(); it != inheritsFrom.constEnd(); ++it) {
        if (result.contains(it.key()) || !obj.contains(it.value()))
            continue;
        result.insert(it.key(), obj.value(it.value()).toBool());
        if (migrated)
            *migrated = true;
    }
    return result;
}

} // namespace SidebarSectionState

#endif // SIDEBAR_SECTION_H
