#ifndef ADD_TILE_DIALOG_H
#define ADD_TILE_DIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

class QListWidget;

// GH#191: the "Add tile" palette. Lets the user pick a tile type and, for
// multi-instance types (temp/fan/gpu/disk/network), bind the new tile to one of
// that type's still-available detected inputs. The set of available inputs is
// computed by the caller (DashboardPage) and passed in.
class AddTileDialog : public QDialog
{
    Q_OBJECT
public:
    // typeOptions: (typeKey, displayName) pairs to offer.
    // inputsByType: typeKey -> list of (inputKey, label) still AVAILABLE (not
    // already placed on the dashboard).
    AddTileDialog(const QList<QPair<QString, QString>> &typeOptions,
                  const QHash<QString, QList<QPair<QString, QString>>> &inputsByType,
                  QWidget *parent = nullptr);

    // The chosen tile type key, or empty if nothing is selected.
    QString chosenType() const;
    // The chosen input key. Empty for single-instance types or when the input
    // list is hidden / has no selection.
    QString chosenInput() const;

private slots:
    void onTypeChanged();

private:
    QListWidget *mTypeList;
    QListWidget *mInputList;
    QHash<QString, QList<QPair<QString, QString>>> mInputsByType;
};

#endif // ADD_TILE_DIALOG_H
