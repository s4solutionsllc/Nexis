#ifndef SHRED_CONFIRM_DIALOG_H
#define SHRED_CONFIRM_DIALOG_H

#include <QDialog>
#include <QString>

// SSO-15381: destructive-action gate for the File Shredder. Per the Nexis
// Design Anchor — one sentence maximum in the dialog body, destructive
// (red-accent) confirm button, title carries the item count + total size so
// the user never confirms a delete without seeing what it costs.
class ShredConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShredConfirmDialog(int itemCount, quint64 totalBytes, QWidget *parent = nullptr);

private:
    void buildUI(int itemCount, quint64 totalBytes);
};

#endif // SHRED_CONFIRM_DIALOG_H
