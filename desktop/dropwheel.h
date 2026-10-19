#pragma once
#include "radialwheel.h"

// The wheel as a drop target. It always answers "copy", so a source asked to
// move (Shift) never deletes the originals after the drop.
class DropWheel : public RadialWheel {
public:
    std::function<void(QStringList)> filesEntered;
    std::function<void(QString)> droppedOn;
    DropWheel() { setAcceptDrops(true); }
    static constexpr int maxFiles = 128;
    static QStringList localFiles(const QMimeData *data) {
        QStringList files;
        for (const QUrl &url : data->urls()) {
            if (!url.isLocalFile()) continue;
            files << QFileInfo(url.toLocalFile()).absoluteFilePath();
            if (files.size() == maxFiles) break;
        }
        return files;
    }
    void paintEvent(QPaintEvent *event) override {
        // Windows routes a drag only over painted pixels: an almost invisible
        // square makes the whole wheel a target, even while it fades in.
        { QPainter p(this); p.fillRect(rect(), QColor(0, 0, 0, 1)); }
        RadialWheel::paintEvent(event);
    }
    void dragEnterEvent(QDragEnterEvent *event) override {
        const QStringList files = localFiles(event->mimeData());
        if (files.isEmpty() || closing() || !acceptCopy(event)) { event->ignore(); return; }
        if (filesEntered) filesEntered(files);
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        if (closing() || !acceptCopy(event)) { event->ignore(); return; }
        const QPointF local = event->position();
        // No click is possible while dragging: pointing at a tab switches wheels.
        if (const int tab = tabAt(local); tab >= 0 && tab != mode && modeRequested) modeRequested(Mode(tab));
        trackGlobal(mapToGlobal(local));
        if (selected().isEmpty()) event->ignore(QRect(local.toPoint(), QSize(1, 1)));
    }
    void dropEvent(QDropEvent *event) override {
        trackGlobal(mapToGlobal(event->position()));
        const QString id = selected();
        if (id.isEmpty() || closing() || !acceptCopy(event)) { event->ignore(); return; }
        if (droppedOn) droppedOn(id);
        dismiss();
    }
private:
    bool closing() const { return testAttribute(Qt::WA_TransparentForMouseEvents); }
    static bool acceptCopy(QDropEvent *event) {
        const Qt::DropActions possible = event->possibleActions();
        const Qt::DropAction action = possible & Qt::CopyAction ? Qt::CopyAction
            : possible & Qt::LinkAction ? Qt::LinkAction : Qt::IgnoreAction;
        if (action == Qt::IgnoreAction) return false;
        event->setDropAction(action);
        event->accept();
        return true;
    }
};
