// A real Wayland drag source for isolated compositor integration tests.
#include <QtWidgets>
class Source : public QWidget {
    QPoint origin;
    QStringList paths;
public:
    explicit Source(QStringList files) : paths(std::move(files)) { setWindowTitle("Lorenz isolated drag test"); setAcceptDrops(true); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.fillRect(rect(),QColor("#202530"));
        p.setPen(Qt::white); p.setFont(QFont("DejaVu Sans",20)); p.drawText(QRect(40,40,850,100),"Lorenz interaction test — isolated compositor");
        p.setBrush(QColor("#ff5454")); p.setPen(Qt::NoPen); p.drawRoundedRect(QRect(160,160,100,100),10,10);
        p.setPen(Qt::white); p.setFont(QFont("DejaVu Sans",12)); p.drawText(QRect(100,280,300,60),Qt::AlignCenter,"sample.png\nDrag, tap Shift, release on WebP");
    }
    void mousePressEvent(QMouseEvent *event) override { origin=event->pos(); }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!(event->buttons()&Qt::LeftButton) || (event->pos()-origin).manhattanLength()<20) return;
        auto *mime=new QMimeData; QList<QUrl> urls; for (const auto &file:paths) urls << QUrl::fromLocalFile(file); mime->setUrls(urls);
        QDrag drag(this); drag.setMimeData(mime); QPixmap icon(32,32); icon.fill(QColor("#ff5454")); drag.setPixmap(icon);
        qInfo()<<"DRAG_STARTED";
        const auto result=drag.exec(Qt::CopyAction|Qt::MoveAction,Qt::CopyAction);
        qInfo()<<"DRAG_FINISHED"<<int(result);
    }
    void dragEnterEvent(QDragEnterEvent *event) override { event->acceptProposedAction(); }
    void dropEvent(QDropEvent *event) override { qInfo()<<"NORMAL_DROP"; event->acceptProposedAction(); }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv); Source source(app.arguments().mid(1)); source.showFullScreen(); return app.exec();
}
