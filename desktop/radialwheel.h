#pragma once
#include <QtWidgets>
#include <cmath>

// Shared by the compositor integration and the context-menu fallback.
class RadialWheel : public QWidget {
public:
    QStringList formats;
    QString message = "Reading files…";
    int hovered = -1;
    std::function<void(QString)> chosen;
    RadialWheel() : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setMouseTracking(true);
        setFixedSize(440, 440);
        setWindowTitle("Metroid format wheel");
        setObjectName("metroid-wheel");
    }
    void openAt(QPoint point) {
        QPoint topLeft = point - QPoint(width()/2,height()/2);
        if (auto *screen = QGuiApplication::screenAt(point)) {
            const QRect area = screen->availableGeometry();
            topLeft.setX(qBound(area.left(),topLeft.x(),qMax(area.left(),area.right()-width()+1)));
            topLeft.setY(qBound(area.top(),topLeft.y(),qMax(area.top(),area.bottom()-height()+1)));
        }
        move(topLeft); hovered = -1; show(); raise(); update();
    }
    int hitTest(QPointF local) const {
        if (formats.isEmpty()) return -1;
        const QPointF d = local - QPointF(220,220);
        const double r = std::hypot(d.x(),d.y());
        if (r < 78 || r > 202) return -1;
        double a = std::atan2(d.y(),d.x()) + 3.14159265358979323846/2;
        if (a < 0) a += 2*3.14159265358979323846;
        return qMin(int(a/(2*3.14159265358979323846)*formats.size()),int(formats.size())-1);
    }
    void trackGlobal(QPointF pos) { hovered=hitTest(pos-QPointF(x(),y())); update(); }
    QString selected() const { return hovered>=0 && hovered<formats.size() ? formats[hovered] : QString(); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#413b3b"),1)); p.setBrush(QColor(8,8,8,245)); p.drawEllipse(QPointF(220,220),207,207);
        const int n=formats.size();
        for (int i=0;i<n;++i) {
            const double sweep=360.0/n;
            QPainterPath sector; sector.moveTo(220,220); sector.arcTo(QRectF(20,20,400,400),90-i*sweep,-sweep); sector.closeSubpath();
            p.setBrush(QColor(i==hovered ? "#b91624" : "#161414")); p.setPen(QPen(QColor("#773034"),1)); p.drawPath(sector);
            const double a=-3.14159265358979323846/2+(i+0.5)*2*3.14159265358979323846/n;
            const QPointF c(220+140*std::cos(a),220+140*std::sin(a));
            p.setPen(QColor("#f2eeee")); p.setFont(QFont("DejaVu Sans",11,QFont::Bold));
            p.drawText(QRectF(c.x()-47,c.y()-20,94,40),Qt::AlignCenter,formats[i].toUpper());
        }
        p.setPen(QPen(QColor("#ff5454"),2)); p.setBrush(QColor("#080808")); p.drawEllipse(QPointF(220,220),77,77);
        p.setPen(QColor("#ff5454")); p.setFont(QFont("DejaVu Sans",11,QFont::Bold)); p.drawText(QRect(150,179,140,30),Qt::AlignCenter,"METROID");
        p.setPen(QColor("#bdb3b3")); p.setFont(QFont("DejaVu Sans",9));
        p.drawText(QRect(152,210,136,48),Qt::AlignCenter|Qt::TextWordWrap,formats.isEmpty()?message:"Release to convert\nCenter to cancel");
    }
    void mouseMoveEvent(QMouseEvent *event) override { hovered=hitTest(event->position()); update(); }
    void mouseReleaseEvent(QMouseEvent *event) override { hovered=hitTest(event->position()); if (chosen) chosen(selected()); hide(); }
};
