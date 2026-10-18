#pragma once
#include <QtWidgets>
#include <cmath>

// Shared by the compositor integration and the context-menu fallback.
class RadialWheel : public QWidget {
    static constexpr double pi = 3.14159265358979323846;
    QVariantAnimation reveal;
    QTimer frames;
    QVector<qreal> emphasis;
    qreal opening = 1;
    void hoverAt(QPointF local) {
        hovered = hitTest(local);
        if (!frames.isActive()) frames.start();
        update();
    }
public:
    static constexpr int extent = 360;
    static constexpr int orbit = 119;
    QStringList formats;
    QString message = "Reading files…";
    int hovered = -1;
    std::function<void(QString)> chosen;
    RadialWheel() : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setMouseTracking(true);
        setFixedSize(extent, extent);
        setWindowTitle("Metroid format wheel");
        setObjectName("metroid-wheel");
        reveal.setDuration(180);
        reveal.setStartValue(0.0); reveal.setEndValue(1.0);
        reveal.setEasingCurve(QEasingCurve::OutCubic);
        connect(&reveal, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { opening=v.toReal(); update(); });
        frames.setInterval(16);
        connect(&frames, &QTimer::timeout, this, [this] {
            emphasis.resize(formats.size());
            bool moving=false;
            for (int i=0; i<emphasis.size(); ++i) {
                const qreal target=i==hovered ? 1 : 0;
                emphasis[i]+=(target-emphasis[i])*0.28;
                if (std::abs(target-emphasis[i])<0.005) emphasis[i]=target;
                else moving=true;
            }
            update();
            if (!moving) frames.stop();
        });
    }
    void openAt(QPoint point) {
        QPoint topLeft = point - QPoint(width()/2,height()/2);
        if (auto *screen = QGuiApplication::screenAt(point)) {
            const QRect area = screen->availableGeometry();
            topLeft.setX(qBound(area.left(),topLeft.x(),qMax(area.left(),area.right()-width()+1)));
            topLeft.setY(qBound(area.top(),topLeft.y(),qMax(area.top(),area.bottom()-height()+1)));
        }
        move(topLeft); hovered = -1; emphasis.fill(0,formats.size());
        opening=0; show(); raise(); reveal.start();
    }
    int hitTest(QPointF local) const {
        if (formats.isEmpty()) return -1;
        const QPointF d = local - QPointF(extent/2,extent/2);
        const double r = std::hypot(d.x(),d.y());
        if (r < 77 || r > 161) return -1;
        double a = std::atan2(d.y(),d.x()) + pi/2;
        if (a < 0) a += 2*pi;
        return qMin(int(a/(2*pi)*formats.size()),int(formats.size())-1);
    }
    void trackGlobal(QPointF pos) { hoverAt(pos-QPointF(x(),y())); }
    QString selected() const { return hovered>=0 && hovered<formats.size() ? formats[hovered] : QString(); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.translate(extent/2,extent/2);
        const qreal scale=0.88+0.12*opening;
        p.scale(scale,scale); p.setOpacity(opening);
        // Soft painted shadow keeps the overlay independent of compositor effects.
        QRadialGradient shadow(QPointF(0,5),177);
        shadow.setColorAt(0.42,QColor(0,0,0,100));
        shadow.setColorAt(0.88,QColor(0,0,0,45)); shadow.setColorAt(1,Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(shadow); p.drawEllipse(QPointF(0,5),177,177);
        const int n=formats.size();
        for (int i=0;i<n;++i) {
            const qreal t=i<emphasis.size()?emphasis[i]:0;
            const double sweep=360.0/n, gap=2.0;
            QPainterPath sector;
            sector.arcMoveTo(QRectF(-159,-159,318,318),90-i*sweep-gap/2);
            sector.arcTo(QRectF(-159,-159,318,318),90-i*sweep-gap/2,-sweep+gap);
            sector.arcTo(QRectF(-79,-79,158,158),90-(i+1)*sweep+gap/2,sweep-gap);
            sector.closeSubpath();
            QLinearGradient fill(0,-159,0,159);
            fill.setColorAt(0,QColor(40+int(105*t),39+int(3*t),43+int(10*t),245));
            fill.setColorAt(1,QColor(23+int(80*t),23,27+int(6*t),245));
            p.setBrush(fill); p.setPen(QPen(QColor(255,255,255,28+int(55*t)),1)); p.drawPath(sector);
            const double a=-pi/2+(i+0.5)*2*pi/n;
            const QPointF c(orbit*std::cos(a),orbit*std::sin(a));
            p.save(); p.translate(c); p.scale(1+0.05*t,1+0.05*t);
            // Small document glyph drawn as vectors, consistent across platforms.
            p.setPen(QPen(QColor(220+int(35*t),170+int(65*t),175+int(60*t)),1.4)); p.setBrush(Qt::NoBrush);
            QPainterPath icon; icon.moveTo(-8,-25); icon.lineTo(3,-25); icon.lineTo(9,-19); icon.lineTo(9,-5); icon.lineTo(-8,-5); icon.closeSubpath();
            icon.moveTo(3,-25); icon.lineTo(3,-19); icon.lineTo(9,-19); p.drawPath(icon);
            p.setPen(QColor(245,243,246)); p.setFont(QFont("Sans Serif",10,QFont::DemiBold));
            p.drawText(QRectF(-40,2,80,24),Qt::AlignCenter,formats[i].toUpper()); p.restore();
        }
        p.setBrush(QColor(25,24,28,245)); p.setPen(QPen(QColor(255,255,255,32),1)); p.drawEllipse(QPointF(0,0),70,70);
        p.setPen(QColor("#ef777e")); p.setFont(QFont("Sans Serif",8,QFont::DemiBold));
        p.drawText(QRectF(-65,-37,130,20),Qt::AlignCenter,hovered<0?"METROID":"CONVERT TO");
        p.setPen(QColor("#faf8fa")); p.setFont(QFont("Sans Serif",hovered<0?11:16,QFont::DemiBold));
        p.drawText(QRectF(-68,-13,136,28),Qt::AlignCenter,formats.isEmpty()?"Please wait":hovered<0?"Choose format":selected().toUpper());
        p.setPen(QColor("#aaa5ae")); p.setFont(QFont("Sans Serif",8));
        p.drawText(QRectF(-61,20,122,32),Qt::AlignCenter|Qt::TextWordWrap,formats.isEmpty()?message:hovered<0?"Center to cancel":"Release to convert");
    }
    void hideEvent(QHideEvent *event) override { reveal.stop(); frames.stop(); QWidget::hideEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { hoverAt(event->position()); }
    void mouseReleaseEvent(QMouseEvent *event) override { hoverAt(event->position()); if (chosen) chosen(selected()); hide(); }
};
