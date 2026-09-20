#ifndef DIALOG_BUTTONS_H
#define DIALOG_BUTTONS_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QMargins>
#include <QPushButton>

// One button row for every dialog, so the same meaning always has the same
// word in the same place:
//   - "Cancel" abandons a pending action; "Close" dismisses a view with
//     nothing pending. Confirm buttons are verbs naming the action — never
//     "OK", never "Skip".
//   - Order comes from QDialogButtonBox roles, i.e. the platform's own
//     convention (macOS/GNOME: dismiss left of confirm; KDE: reversed).
//   - A destructive confirm is styled as danger and is never the default
//     button: Enter must not delete, shred or wipe anything.
namespace DialogButtons {

enum class Confirm { Primary, Danger };

struct Row {
    QDialogButtonBox *box = nullptr;
    QPushButton *confirm = nullptr;   // null when the dialog only dismisses
    QPushButton *dismiss = nullptr;
};

inline QMargins dialogMargins() { return QMargins(20, 16, 20, 16); }
inline int dialogSpacing() { return 12; }

// confirmText may be empty for informational dialogs. The dismiss button is
// wired to QDialog::reject unless wireReject is false (dialogs whose dismiss
// button doubles as "Stop" while work is running); the caller connects
// `confirm->clicked`.
inline Row build(QDialog *dialog, const QString &confirmText, Confirm kind, const QString &dismissText,
                 bool wireReject = true)
{
    Row row;
    row.box = new QDialogButtonBox(dialog);

    row.dismiss = row.box->addButton(dismissText, QDialogButtonBox::RejectRole);
    row.dismiss->setCursor(Qt::PointingHandCursor);
    row.dismiss->setAutoDefault(false);
    if (wireReject)
        QObject::connect(row.box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    if (!confirmText.isEmpty()) {
        row.confirm = row.box->addButton(confirmText, QDialogButtonBox::AcceptRole);
        row.confirm->setCursor(Qt::PointingHandCursor);
        // Styling hook only (QPushButton[variant=...] in style.qss); it used to
        // be accessibleName, which made screen readers announce "danger".
        row.confirm->setProperty("variant", kind == Confirm::Danger ? "danger" : "primary");
        row.confirm->setAutoDefault(false);
    }

    const bool confirmIsDefault = row.confirm && kind == Confirm::Primary;
    (confirmIsDefault ? row.confirm : row.dismiss)->setDefault(true);
    return row;
}

// A secondary destructive action inside an editor (e.g. "Delete" next to
// Cancel/Save). Placed by the platform on the far side of the row.
inline QPushButton *addDestructive(Row &row, const QString &text)
{
    QPushButton *button = row.box->addButton(text, QDialogButtonBox::DestructiveRole);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(false);
    return button;
}

} // namespace DialogButtons

#endif // DIALOG_BUTTONS_H
