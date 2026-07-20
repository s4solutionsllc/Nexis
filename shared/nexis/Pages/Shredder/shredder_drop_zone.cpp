#include "shredder_drop_zone.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStyle>
#include <QUrl>

ShredderDropZone::ShredderDropZone(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("shredderDropZone");
    setAcceptDrops(true);
    setProperty("dragActive", false);
}

void ShredderDropZone::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setDragActive(true);
    }
}

void ShredderDropZone::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    setDragActive(false);
}

void ShredderDropZone::dropEvent(QDropEvent *event)
{
    setDragActive(false);

    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList paths;
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }

    if (!paths.isEmpty()) {
        event->acceptProposedAction();
        emit pathsDropped(paths);
    }
}

void ShredderDropZone::setDragActive(bool active)
{
    setProperty("dragActive", active);
    style()->unpolish(this);
    style()->polish(this);
}
