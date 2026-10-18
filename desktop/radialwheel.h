#pragma once
#include <QtWidgets>
#include <cmath>

// One native wheel for compositor gestures and file-context actions.
class RadialWheel : public QWidget {
    static constexpr double pi = 3.14159265358979323846;
    QVariantAnimation reveal;
    QTimer frames;
    QElapsedTimer clock;
    QVector<qreal> emphasis;
    qreal opening = 1;
    qreal phase = 0;
    bool closing = false;
    bool loading = false;
    QPointF lastPointer{-1000,-1000};
    static QColor blend(QColor a, QColor b, qreal t) {
        return QColor::fromRgbF(a.redF()+(b.redF()-a.redF())*t,
            a.greenF()+(b.greenF()-a.greenF())*t,
            a.blueF()+(b.blueF()-a.blueF())*t,
            a.alphaF()+(b.alphaF()-a.alphaF())*t);
    }
    static QFont font(int pixels, QFont::Weight weight=QFont::Normal) {
        QFont f=QApplication::font(); f.setPixelSize(pixels); f.setWeight(weight); return f;
    }
    static QPointF polar(qreal radius, qreal degrees) {
        const qreal a=degrees*pi/180;
        return {radius*std::cos(a),-radius*std::sin(a)};
    }
    static QPainterPath petal(qreal start, qreal sweep) {
        // Rounded annular sectors; all corners are softened without a mask.
        constexpr qreal outer=160, inner=84, corner=7;
        const qreal end=start-sweep;
        QPainterPath path;
        path.moveTo(polar(outer,start-3));
        path.arcTo(QRectF(-outer,-outer,2*outer,2*outer),start-3,-sweep+6);
        path.quadTo(polar(outer,end),polar(outer-corner,end));
        path.lineTo(polar(inner+corner,end));
        path.quadTo(polar(inner,end),polar(inner,end+5));
        path.arcTo(QRectF(-inner,-inner,2*inner,2*inner),end+5,sweep-10);
        path.quadTo(polar(inner,start),polar(inner+corner,start));
        path.lineTo(polar(outer-corner,start));
        path.quadTo(polar(outer,start),polar(outer,start-3));
        path.closeSubpath(); return path;
    }
    static void glyph(QPainter &p, const QString &type) {
        p.setBrush(Qt::NoBrush);
        if (QStringList{"png","jpg","webp","avif","bmp","tiff","gif","heic"}.contains(type)) {
            p.drawRoundedRect(QRectF(-10,-9,20,18),4,4);
            p.drawEllipse(QPointF(4,-3),2,2);
            QPainterPath hills; hills.moveTo(-9,6); hills.lineTo(-3,-1); hills.lineTo(2,5); hills.lineTo(5,2); hills.lineTo(9,6); p.drawPath(hills);
        } else if (QStringList{"mp3","m4a","wav","flac","ogg","opus","aiff","wma"}.contains(type)) {
            p.drawLine(QPointF(-3,6),QPointF(-3,-7)); p.drawLine(QPointF(-3,-7),QPointF(8,-10));
            p.drawLine(QPointF(8,-10),QPointF(8,3)); p.drawEllipse(QPointF(-6,7),3,2.5); p.drawEllipse(QPointF(5,4),3,2.5);
        } else if (QStringList{"mp4","mov","mkv","webm","avi"}.contains(type)) {
            p.drawRoundedRect(QRectF(-11,-8,22,16),4,4);
            QPainterPath play; play.moveTo(-2,-4); play.lineTo(4,0); play.lineTo(-2,4); play.closeSubpath(); p.drawPath(play);
        } else {
            QPainterPath page; page.moveTo(-7,-10); page.lineTo(2,-10); page.lineTo(8,-4); page.lineTo(8,10); page.lineTo(-7,10); page.closeSubpath();
            page.moveTo(2,-10); page.lineTo(2,-4); page.lineTo(8,-4); p.drawPath(page);
            p.drawLine(QPointF(-3,1),QPointF(4,1)); p.drawLine(QPointF(-3,5),QPointF(2,5));
        }
    }
    void animate() { if (!frames.isActive()) { clock.restart(); frames.start(); } }
    void hoverAt(QPointF local) {
        lastPointer=local; hovered=closing?-1:hitTest(local); animate(); update();
    }
public:
    static constexpr int extent=384;
    static constexpr int orbit=122;
    QStringList formats;
    QString message="Reading files…";
    QString sourceName;
    QString sourceType;
    int hovered=-1;
    std::function<void(QString)> chosen;
    RadialWheel() : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setMouseTracking(true); setFixedSize(extent,extent);
        setWindowTitle("Metroid format wheel"); setObjectName("metroid-wheel");
        connect(&reveal,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){ opening=value.toReal(); update(); });
        connect(&reveal,&QVariantAnimation::finished,this,[this]{ if (closing) hide(); });
        frames.setInterval(16); frames.setTimerType(Qt::PreciseTimer);
        connect(&frames,&QTimer::timeout,this,[this]{
            const qreal dt=qMin(qreal(clock.restart()),64.0);
            phase=std::fmod(phase+dt*0.26,360.0);
            emphasis.resize(formats.size()); bool moving=false;
            for (int i=0;i<emphasis.size();++i) {
                const qreal target=i==hovered?1:0;
                emphasis[i]+=(target-emphasis[i])*(1-std::exp(-dt/48));
                if (std::abs(target-emphasis[i])<0.003) emphasis[i]=target; else moving=true;
            }
            update(); if (!moving && !loading) frames.stop();
        });
    }
    void setFormats(QStringList values) {
        formats=std::move(values); loading=false; emphasis.fill(0,formats.size());
        hovered=hitTest(lastPointer); animate(); update();
    }
    void setLoading(bool value) { loading=value; if (isVisible()) animate(); }
    void openAt(QPoint point) {
        reveal.stop(); closing=false; setAttribute(Qt::WA_TransparentForMouseEvents,false);
        if (windowHandle()) windowHandle()->setFlag(Qt::WindowTransparentForInput,false);
        QPoint topLeft=point-QPoint(extent/2,extent/2);
        if (auto *screen=QGuiApplication::screenAt(point)) {
            const QRect area=screen->availableGeometry();
            topLeft.setX(qBound(area.left(),topLeft.x(),qMax(area.left(),area.right()-width()+1)));
            topLeft.setY(qBound(area.top(),topLeft.y(),qMax(area.top(),area.bottom()-height()+1)));
        }
        move(topLeft); hovered=-1; lastPointer=QPointF(extent/2,extent/2); emphasis.fill(0,formats.size());
        opening=0; show(); raise();
        reveal.setDuration(220); reveal.setStartValue(0.0); reveal.setEndValue(1.0);
        reveal.setEasingCurve(QEasingCurve::OutCubic); reveal.start(); animate();
    }
    void dismiss() {
        if (!isVisible() || closing) return;
        closing=true; loading=false; setAttribute(Qt::WA_TransparentForMouseEvents);
        // The native input region must also disappear while the pixels fade out.
        if (windowHandle()) windowHandle()->setFlag(Qt::WindowTransparentForInput,true);
        reveal.stop(); reveal.setDuration(110); reveal.setStartValue(opening); reveal.setEndValue(0.0);
        reveal.setEasingCurve(QEasingCurve::InCubic); reveal.start();
    }
    int hitTest(QPointF local) const {
        if (formats.isEmpty()) return -1;
        const QPointF d=local-QPointF(extent/2,extent/2);
        const double r=std::hypot(d.x(),d.y());
        if (r<82 || r>164) return -1;
        double a=std::atan2(d.y(),d.x())+pi/2; if (a<0) a+=2*pi;
        return qMin(int(a/(2*pi)*formats.size()),int(formats.size())-1);
    }
    void trackGlobal(QPointF pos) { hoverAt(pos-QPointF(x(),y())); }
    QString selected() const { return hovered>=0 && hovered<formats.size()?formats[hovered]:QString(); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.translate(extent/2,extent/2); p.setOpacity(opening);
        const qreal scale=0.90+0.10*opening; p.scale(scale,scale);
        QRadialGradient shadow(QPointF(0,7),181);
        shadow.setColorAt(0.35,QColor(0,0,0,85)); shadow.setColorAt(0.86,QColor(0,0,0,48)); shadow.setColorAt(1,Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(shadow); p.drawEllipse(QPointF(0,7),181,181);
        const int count=formats.size();
        for (int i=0;i<count;++i) {
            const qreal t=i<emphasis.size()?emphasis[i]:0;
            const qreal angle=90-(i+0.5)*360.0/count;
            const QPointF direction=polar(1,angle);
            const auto shape=petal(90-i*360.0/count-1.1,360.0/count-2.2);
            p.save(); p.translate(direction*2*t);
            QLinearGradient surface(-70,-150,80,160);
            surface.setColorAt(0,blend(QColor(44,51,57,248),QColor("#e0f5ec"),t));
            surface.setColorAt(1,blend(QColor(26,31,37,248),QColor("#a6dcca"),t));
            p.setBrush(surface); p.setPen(QPen(blend(QColor(158,183,185,54),QColor(235,255,249,230),t),1)); p.drawPath(shape);
            p.translate(direction*orbit); p.translate(0,-10-2*t);
            p.setPen(QPen(blend(QColor("#9ebbb6"),QColor("#244d43"),t),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            glyph(p,formats[i]);
            p.setFont(font(12,QFont::DemiBold)); p.setPen(blend(QColor("#edf1f2"),QColor("#163e33"),t));
            p.drawText(QRectF(-38,16,76,20),Qt::AlignCenter,formats[i].toUpper()); p.restore();
        }
        QLinearGradient hub(-60,-65,60,70); hub.setColorAt(0,QColor(40,47,53,250)); hub.setColorAt(1,QColor(22,28,34,250));
        p.setBrush(hub); p.setPen(QPen(QColor(177,206,208,48),1)); p.drawEllipse(QPointF(0,0),73,73);
        p.setPen(QPen(QColor(174,224,208,80),1)); p.drawArc(QRectF(-69,-69,138,138),38*16,104*16);
        if (loading) {
            p.setPen(QPen(QColor("#b6e8d6"),2,Qt::SolidLine,Qt::RoundCap));
            p.drawArc(QRectF(-10,-36,20,20),int(-phase*16),230*16);
        } else {
            p.save(); p.translate(0,-29); p.scale(0.68,0.68);
            p.setPen(QPen(QColor("#acd4c7"),1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); glyph(p,sourceType); p.restore();
        }
        p.setFont(font(hovered<0?15:21,QFont::DemiBold)); p.setPen(QColor("#f1f5f4"));
        p.drawText(QRectF(-69,-13,138,29),Qt::AlignCenter,loading?"Reading…":count==0?"Unavailable":hovered<0?"Convert":selected().toUpper());
        p.setFont(font(10)); p.setPen(QColor("#a7b5b8"));
        const QString detail=loading?"Finding formats":count==0?message.section('\n',0,0):hovered<0?QString("%1 formats").arg(count):"Release to convert";
        p.drawText(QRectF(-66,19,132,16),Qt::AlignCenter,detail);
        p.setFont(font(9)); p.setPen(QColor("#7f949b"));
        p.drawText(QRectF(-59,39,118,15),Qt::AlignCenter,QFontMetrics(p.font()).elidedText(sourceName,Qt::ElideMiddle,114));
        // Small, readable keyboard hint; transparent space remains around the wheel.
        p.setFont(font(10)); const QRectF hint(-83,165,166,21);
        p.setBrush(QColor(24,30,35,235)); p.setPen(QPen(QColor(154,179,185,30),1)); p.drawRoundedRect(hint,10,10);
        p.setPen(QColor("#aebdc1")); p.drawText(hint,Qt::AlignCenter,"esc  /  center to cancel");
    }
    void hideEvent(QHideEvent *event) override { reveal.stop(); frames.stop(); closing=false; QWidget::hideEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { hoverAt(event->position()); }
    void mouseReleaseEvent(QMouseEvent *event) override { if (closing) return; hoverAt(event->position()); if (chosen) chosen(selected()); dismiss(); }
    void keyPressEvent(QKeyEvent *event) override { if (event->key()==Qt::Key_Escape) { if (chosen) chosen({}); dismiss(); } else QWidget::keyPressEvent(event); }
};
