#ifndef SHREDDER_DROP_ZONE_H
#define SHREDDER_DROP_ZONE_H

#include <QFrame>
#include <QStringList>

// SSO-15381: drag-and-drop target for the File Shredder. Accepts files and
// folders dropped from the OS file manager and forwards their local paths
// via pathsDropped(). Drag-over feedback is driven by the "dragActive"
// dynamic property (QSS-styled, BUG-56 re-polish discipline) rather than a
// custom paintEvent.
class ShredderDropZone : public QFrame
{
    Q_OBJECT

public:
    explicit ShredderDropZone(QWidget *parent = nullptr);

signals:
    void pathsDropped(const QStringList &paths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setDragActive(bool active);
};

#endif // SHREDDER_DROP_ZONE_H
