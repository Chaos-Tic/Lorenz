#pragma once
#include <QtWidgets>
#include <cmath>
#include <functional>

// One native wheel for compositor gestures and file-context actions. It has
// three modes, like the original gesture: formats, tools and file actions.
class RadialWheel : public QWidget {
public:
    enum Mode { Convert, Tools, Actions };
    struct Item { QString id, label, glyph; };
private:
    static constexpr double pi = 3.14159265358979323846;
    struct Palette { QColor light, deep, ink, stroke; };
    QVariantAnimation reveal;
    QVariantAnimation swap;
    QTimer frames;
    QElapsedTimer clock;
    QVector<qreal> emphasis;
    qreal opening = 1;
    qreal swapping = 1;
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
    // Mint for formats, apricot for tools, lilac for actions.
    static Palette palette(Mode m) {
        if (m==Tools) return {QColor("#fbeedd"),QColor("#efc797"),QColor("#4b3217"),QColor("#e2c3a0")};
        if (m==Actions) return {QColor("#eeebfc"),QColor("#c4bbf0"),QColor("#2f2858"),QColor("#c3bce6")};
        return {QColor("#e0f5ec"),QColor("#a6dcca"),QColor("#163e33"),QColor("#9ebbb6")};
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
    static void arrowHead(QPainter &p, QPointF tip, qreal degrees) {
        p.drawLine(tip,tip+polar(5,degrees+150)); p.drawLine(tip,tip+polar(5,degrees-150));
    }
    void animate() { if (!frames.isActive()) { clock.restart(); frames.start(); } }
    void hoverAt(QPointF local) {
        lastPointer=local; hovered=closing?-1:hitTest(local); animate(); update();
    }
    QRectF tabRect(int index) const {
        const qreal width=86, left=-1.5*width;
        return QRectF(left+index*width,165,width,22).translated(extent/2,extent/2);
    }
public:
    static constexpr int extent=384;
    static constexpr int orbit=122;
    QStringList formats; // Item identifiers, kept for callers and tests.
    QVector<Item> items;
    Mode mode=Convert;
    QString message="Reading files…";
    QString sourceName;
    QString sourceType;
    QStringList tabKeys{"Shift","Ctrl","Alt"};
    bool showTabs=true;
    int hovered=-1;
    std::function<void(QString)> chosen;
    std::function<void(Mode)> modeRequested;
    // Format extensions map to families; tools and actions name their glyph.
    static QString glyphFor(const QString &type) {
        if (QStringList{"png","jpg","jpeg","webp","avif","bmp","tif","tiff","gif","heic","svg"}.contains(type)) return "image";
        if (QStringList{"mp3","m4a","wav","flac","ogg","opus","aiff","wma"}.contains(type)) return "audio";
        if (QStringList{"mp4","mov","mkv","webm","avi","wmv"}.contains(type)) return "video";
        if (QStringList{"zip","tar","tgz","gz","bz2","xz","7z","rar"}.contains(type)) return "archive";
        if (QStringList{"xlsx","xls","ods","csv"}.contains(type)) return "sheet";
        if (QStringList{"ppt","pptx","odp"}.contains(type)) return "slides";
        if (QStringList{"srt","vtt"}.contains(type)) return "subtitle";
        if (type=="pdf") return "pdf";
        if (type=="folder") return "folder";
        return "document";
    }
    static void glyph(QPainter &p, const QString &name) {
        p.setBrush(Qt::NoBrush);
        auto page=[&](qreal w=15) {
            QPainterPath d; d.moveTo(-w/2,-10); d.lineTo(w/2-5,-10); d.lineTo(w/2,-5); d.lineTo(w/2,10); d.lineTo(-w/2,10); d.closeSubpath();
            d.moveTo(w/2-5,-10); d.lineTo(w/2-5,-5); d.lineTo(w/2,-5); p.drawPath(d);
        };
        if (name=="image") {
            p.drawRoundedRect(QRectF(-10,-9,20,18),4,4);
            p.drawEllipse(QPointF(4,-3),2,2);
            QPainterPath hills; hills.moveTo(-9,6); hills.lineTo(-3,-1); hills.lineTo(2,5); hills.lineTo(5,2); hills.lineTo(9,6); p.drawPath(hills);
        } else if (name=="audio") {
            p.drawLine(QPointF(-3,6),QPointF(-3,-7)); p.drawLine(QPointF(-3,-7),QPointF(8,-10));
            p.drawLine(QPointF(8,-10),QPointF(8,3)); p.drawEllipse(QPointF(-6,7),3,2.5); p.drawEllipse(QPointF(5,4),3,2.5);
        } else if (name=="video") {
            p.drawRoundedRect(QRectF(-11,-8,22,16),4,4);
            QPainterPath play; play.moveTo(-2,-4); play.lineTo(4,0); play.lineTo(-2,4); play.closeSubpath(); p.drawPath(play);
        } else if (name=="pdf") {
            page(); p.drawLine(QPointF(-4,0),QPointF(3,0)); p.drawLine(QPointF(-4,4),QPointF(3,4));
            p.drawRoundedRect(QRectF(-10,-5,9,6),1.5,1.5);
        } else if (name=="sheet") {
            p.drawRoundedRect(QRectF(-10,-9,20,18),3,3); p.drawLine(QPointF(-10,-3),QPointF(10,-3));
            p.drawLine(QPointF(-10,3),QPointF(10,3)); p.drawLine(QPointF(-3,-9),QPointF(-3,9));
        } else if (name=="slides") {
            p.drawRoundedRect(QRectF(-11,-9,22,14),3,3); p.drawLine(QPointF(0,5),QPointF(0,9));
            p.drawLine(QPointF(-4,9),QPointF(4,9)); p.drawLine(QPointF(-5,-1),QPointF(-1,-4)); p.drawLine(QPointF(-1,-4),QPointF(5,-1));
        } else if (name=="subtitle") {
            p.drawRoundedRect(QRectF(-11,-8,22,16),4,4); p.drawLine(QPointF(-6,2),QPointF(1,2)); p.drawLine(QPointF(4,2),QPointF(6,2));
            p.drawLine(QPointF(-6,-2),QPointF(-3,-2)); p.drawLine(QPointF(0,-2),QPointF(6,-2));
        } else if (name=="archive") {
            p.drawRoundedRect(QRectF(-9,-10,18,20),3,3);
            for (int y : {-7,-3,1}) { p.drawLine(QPointF(-1,y),QPointF(1,y)); p.drawLine(QPointF(1,y+2),QPointF(-1,y+2)); }
            p.drawRoundedRect(QRectF(-2.5,4,5,4),1,1);
        } else if (name=="compress") {
            p.drawLine(QPointF(0,-11),QPointF(0,-3)); arrowHead(p,QPointF(0,-3),-90);
            p.drawLine(QPointF(0,11),QPointF(0,3)); arrowHead(p,QPointF(0,3),90); p.drawLine(QPointF(-9,0),QPointF(9,0));
        } else if (name=="resize") {
            p.drawRoundedRect(QRectF(-10,-10,20,20),3,3); p.drawRoundedRect(QRectF(-10,0,10,10),2,2);
            p.drawLine(QPointF(2,-2),QPointF(7,-7)); arrowHead(p,QPointF(7,-7),45);
        } else if (name=="rotate" || name=="rotate-left") {
            p.save(); if (name=="rotate-left") p.scale(-1,1);
            p.drawArc(QRectF(-9,-9,18,18),100*16,-280*16); arrowHead(p,polar(9,100),10); p.restore();
        } else if (name=="flip") {
            QPainterPath l; l.moveTo(-2,-9); l.lineTo(-10,8); l.lineTo(-2,8); l.closeSubpath(); p.drawPath(l);
            QPainterPath r; r.moveTo(2,-9); r.lineTo(10,8); r.lineTo(2,8); r.closeSubpath(); p.drawPath(r);
        } else if (name=="grayscale") {
            p.drawEllipse(QPointF(0,0),10,10); QPainterPath half; half.moveTo(0,-10); half.arcTo(QRectF(-10,-10,20,20),90,180); half.closeSubpath();
            p.save(); p.setBrush(p.pen().color()); p.drawPath(half); p.restore();
        } else if (name=="strip") {
            page(); p.drawLine(QPointF(-10,10),QPointF(10,-10));
        } else if (name=="mute") {
            QPainterPath s; s.moveTo(-10,-3); s.lineTo(-5,-3); s.lineTo(1,-9); s.lineTo(1,9); s.lineTo(-5,3); s.lineTo(-10,3); s.closeSubpath(); p.drawPath(s);
            p.drawLine(QPointF(5,-4),QPointF(11,4)); p.drawLine(QPointF(11,-4),QPointF(5,4));
        } else if (name=="speed") {
            for (qreal x : {-9.0,0.0}) { QPainterPath t; t.moveTo(x,-8); t.lineTo(x+9,0); t.lineTo(x,8); t.closeSubpath(); p.drawPath(t); }
        } else if (name=="normalize") {
            for (int i=0;i<5;++i) { const qreal h=QList<qreal>{6,12,16,10,7}[i]; p.drawLine(QPointF(-8+i*4,-h/2),QPointF(-8+i*4,h/2)); }
            p.drawLine(QPointF(-11,-6),QPointF(11,-6));
        } else if (name=="merge") {
            p.drawLine(QPointF(-9,-9),QPointF(0,0)); p.drawLine(QPointF(9,-9),QPointF(0,0));
            p.drawLine(QPointF(0,0),QPointF(0,10)); arrowHead(p,QPointF(0,10),-90);
        } else if (name=="split") {
            p.drawLine(QPointF(0,10),QPointF(0,1)); p.drawLine(QPointF(0,1),QPointF(-8,-8)); p.drawLine(QPointF(0,1),QPointF(8,-8));
            arrowHead(p,QPointF(-8,-8),135); arrowHead(p,QPointF(8,-8),45);
        } else if (name=="extract") {
            p.drawRoundedRect(QRectF(-10,-2,20,12),3,3); p.drawLine(QPointF(0,4),QPointF(0,-11)); arrowHead(p,QPointF(0,-11),90);
        } else if (name=="share") {
            p.drawLine(QPointF(0,3),QPointF(0,-11)); arrowHead(p,QPointF(0,-11),90);
            QPainterPath b; b.moveTo(-5,-4); b.lineTo(-9,-4); b.lineTo(-9,10); b.lineTo(9,10); b.lineTo(9,-4); b.lineTo(5,-4); p.drawPath(b);
        } else if (name=="float") {
            p.drawRoundedRect(QRectF(-8,-11,16,14),3,3); p.drawLine(QPointF(-6,8),QPointF(6,8)); p.drawLine(QPointF(-3,11),QPointF(3,11));
        } else if (name=="copy" || name=="copy-to") {
            p.drawRoundedRect(QRectF(-10,-10,13,15),3,3); p.drawRoundedRect(QRectF(-3,-4,13,15),3,3);
            if (name=="copy-to") { p.drawLine(QPointF(0,3),QPointF(7,3)); arrowHead(p,QPointF(7,3),0); }
        } else if (name=="move") {
            p.drawRoundedRect(QRectF(-11,-6,12,14),3,3); p.drawLine(QPointF(-1,1),QPointF(11,1)); arrowHead(p,QPointF(11,1),0);
        } else if (name=="path") {
            p.drawRoundedRect(QRectF(-11,-6,10,12),5,5); p.drawRoundedRect(QRectF(1,-6,10,12),5,5); p.drawLine(QPointF(-4,0),QPointF(4,0));
        } else if (name=="terminal") {
            p.drawRoundedRect(QRectF(-11,-9,22,18),4,4); p.drawLine(QPointF(-6,-3),QPointF(-2,0)); p.drawLine(QPointF(-2,0),QPointF(-6,3)); p.drawLine(QPointF(0,4),QPointF(6,4));
        } else if (name=="folder") {
            QPainterPath f; f.moveTo(-11,-7); f.lineTo(-4,-7); f.lineTo(-2,-4); f.lineTo(11,-4); f.lineTo(11,9); f.lineTo(-11,9); f.closeSubpath(); p.drawPath(f);
        } else {
            page(); p.drawLine(QPointF(-3,1),QPointF(4,1)); p.drawLine(QPointF(-3,5),QPointF(2,5));
        }
    }
    RadialWheel() : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setMouseTracking(true); setFixedSize(extent,extent);
        setWindowTitle("Metroid wheel"); setObjectName("metroid-wheel");
        connect(&reveal,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){ opening=value.toReal(); update(); });
        connect(&reveal,&QVariantAnimation::finished,this,[this]{ if (closing) hide(); });
        connect(&swap,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){ swapping=value.toReal(); update(); });
        frames.setInterval(16); frames.setTimerType(Qt::PreciseTimer);
        connect(&frames,&QTimer::timeout,this,[this]{
            const qreal dt=qMin(qreal(clock.restart()),64.0);
            phase=std::fmod(phase+dt*0.26,360.0);
            emphasis.resize(items.size()); bool moving=false;
            for (int i=0;i<emphasis.size();++i) {
                const qreal target=i==hovered?1:0;
                emphasis[i]+=(target-emphasis[i])*(1-std::exp(-dt/48));
                if (std::abs(target-emphasis[i])<0.003) emphasis[i]=target; else moving=true;
            }
            update(); if (!moving && !loading) frames.stop();
        });
    }
    void setItems(QVector<Item> values) {
        items=std::move(values); formats.clear(); for (const auto &i:items) formats << i.id;
        loading=false; emphasis.fill(0,items.size());
        hovered=hitTest(lastPointer); animate(); update();
    }
    void setFormats(const QStringList &values) {
        QVector<Item> list; for (const auto &f:values) list.append({f,f.toUpper(),glyphFor(f)}); setItems(list);
    }
    // Switching wheels keeps the position; the new sectors turn into place.
    void setMode(Mode value) {
        if (mode==value) return;
        mode=value; hovered=-1; swap.stop(); swap.setDuration(200); swap.setStartValue(0.0); swap.setEndValue(1.0);
        swap.setEasingCurve(QEasingCurve::OutCubic); swap.start(); animate(); update();
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
        move(topLeft); hovered=-1; lastPointer=QPointF(extent/2,extent/2); emphasis.fill(0,items.size());
        opening=0; swapping=1; show(); raise();
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
        if (items.isEmpty()) return -1;
        const QPointF d=local-QPointF(extent/2,extent/2);
        const double r=std::hypot(d.x(),d.y());
        if (r<82 || r>164) return -1;
        double a=std::atan2(d.y(),d.x())+pi/2; if (a<0) a+=2*pi;
        return qMin(int(a/(2*pi)*items.size()),int(items.size())-1);
    }
    int tabAt(QPointF local) const {
        if (!showTabs) return -1;
        for (int i=0;i<3;++i) if (tabRect(i).contains(local)) return i;
        return -1;
    }
    // Keyboard selection: arrows walk around the wheel, starting at the top.
    void step(int delta) {
        if (items.isEmpty()) return;
        hovered=hovered<0?(delta>0?0:items.size()-1):(hovered+delta+items.size())%items.size();
        animate(); update();
    }
    void trackGlobal(QPointF pos) { hoverAt(pos-QPointF(x(),y())); }
    QString selected() const { return hovered>=0 && hovered<items.size()?items[hovered].id:QString(); }
    QString selectedLabel() const { return hovered>=0 && hovered<items.size()?items[hovered].label:QString(); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.translate(extent/2,extent/2); p.setOpacity(opening);
        const qreal scale=0.90+0.10*opening; p.scale(scale,scale);
        const Palette tone=palette(mode);
        QRadialGradient shadow(QPointF(0,7),181);
        shadow.setColorAt(0.35,QColor(0,0,0,85)); shadow.setColorAt(0.86,QColor(0,0,0,48)); shadow.setColorAt(1,Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(shadow); p.drawEllipse(QPointF(0,7),181,181);
        const int count=items.size();
        p.save(); p.setOpacity(opening*swapping); p.rotate((1-swapping)*-14);
        for (int i=0;i<count;++i) {
            const qreal t=i<emphasis.size()?emphasis[i]:0;
            const qreal angle=90-(i+0.5)*360.0/count;
            const QPointF direction=polar(1,angle);
            const auto shape=petal(90-i*360.0/count-1.1,360.0/count-2.2);
            p.save(); p.translate(direction*2*t);
            QLinearGradient surface(-70,-150,80,160);
            surface.setColorAt(0,blend(QColor(44,51,57,248),tone.light,t));
            surface.setColorAt(1,blend(QColor(26,31,37,248),tone.deep,t));
            p.setBrush(surface); p.setPen(QPen(blend(QColor(158,183,185,54),QColor(255,255,255,230),t),1)); p.drawPath(shape);
            p.translate(direction*orbit); p.translate(0,-10-2*t);
            p.setPen(QPen(blend(tone.stroke,tone.ink,t),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            glyph(p,mode==Convert?glyphFor(items[i].id):items[i].glyph);
            p.setPen(blend(QColor("#edf1f2"),tone.ink,t));
            if (mode==Convert) { p.setFont(font(12,QFont::DemiBold)); p.drawText(QRectF(-38,16,76,20),Qt::AlignCenter,items[i].label); }
            else { p.setFont(font(count>8?10:11,QFont::DemiBold)); p.drawText(QRectF(-42,15,84,30),Qt::AlignHCenter|Qt::AlignTop|Qt::TextWordWrap,items[i].label); }
            p.restore();
        }
        p.restore();
        QLinearGradient hub(-60,-65,60,70); hub.setColorAt(0,QColor(40,47,53,250)); hub.setColorAt(1,QColor(22,28,34,250));
        p.setBrush(hub); p.setPen(QPen(QColor(177,206,208,48),1)); p.drawEllipse(QPointF(0,0),73,73);
        QColor rim=tone.deep; rim.setAlpha(90); p.setPen(QPen(rim,1)); p.drawArc(QRectF(-69,-69,138,138),38*16,104*16);
        if (loading) {
            p.setPen(QPen(tone.deep,2,Qt::SolidLine,Qt::RoundCap));
            p.drawArc(QRectF(-10,-36,20,20),int(-phase*16),230*16);
        } else {
            p.save(); p.translate(0,-29); p.scale(0.68,0.68);
            p.setPen(QPen(tone.deep,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); glyph(p,glyphFor(sourceType)); p.restore();
        }
        static const QStringList titles{"Convert","Tools","Actions"};
        static const QStringList nouns{"formats","tools","actions"};
        const QString title=loading?"Reading…":count==0?"Unavailable":hovered<0?titles[mode]:selectedLabel();
        p.setFont(font(hovered<0?15:(title.size()>9?14:(mode==Convert?21:16)),QFont::DemiBold)); p.setPen(QColor("#f1f5f4"));
        p.drawText(QRectF(-69,-13,138,29),Qt::AlignCenter,QFontMetrics(p.font()).elidedText(title,Qt::ElideRight,134));
        p.setFont(font(10)); p.setPen(QColor("#a7b5b8"));
        const QString release=mode==Convert?"Release to convert":mode==Tools?"Release to apply":"Release to run";
        const QString detail=loading?"Finding options":count==0?message.section('\n',0,0):hovered<0?QString("%1 %2 · Esc cancels").arg(count).arg(nouns[mode]):release;
        p.drawText(QRectF(-66,19,132,16),Qt::AlignCenter,detail);
        p.setFont(font(9)); p.setPen(QColor("#7f949b"));
        p.drawText(QRectF(-59,39,118,15),Qt::AlignCenter,QFontMetrics(p.font()).elidedText(sourceName,Qt::ElideMiddle,114));
        if (showTabs) {
            // Wheel switcher: the active wheel is lit with its own accent.
            p.resetTransform(); p.setOpacity(opening);
            const QRectF bar=tabRect(0).united(tabRect(2)).adjusted(-3,-3,3,3);
            p.setBrush(QColor(24,30,35,235)); p.setPen(QPen(QColor(154,179,185,30),1)); p.drawRoundedRect(bar,13,13);
            for (int i=0;i<3;++i) {
                const QRectF r=tabRect(i).adjusted(2,0,-2,0); const bool on=i==mode;
                if (on) { p.setBrush(palette(Mode(i)).light); p.setPen(Qt::NoPen); p.drawRoundedRect(r,11,11); }
                p.setFont(font(10,on?QFont::DemiBold:QFont::Normal)); p.setPen(on?palette(Mode(i)).ink:QColor("#aebdc1"));
                p.drawText(r,Qt::AlignCenter,tabKeys.value(i)+"  "+titles[i]);
            }
        }
    }
    void hideEvent(QHideEvent *event) override { reveal.stop(); swap.stop(); frames.stop(); closing=false; QWidget::hideEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { hoverAt(event->position()); }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (closing) return;
        if (const int tab=tabAt(event->position()); tab>=0 && modeRequested) { modeRequested(Mode(tab)); return; }
        hoverAt(event->position()); if (chosen) chosen(selected()); dismiss();
    }
    void keyPressEvent(QKeyEvent *event) override {
        switch (event->key()) {
        case Qt::Key_Escape: if (chosen) chosen({}); dismiss(); break;
        case Qt::Key_Left: case Qt::Key_Up: step(-1); break;
        case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_Tab: step(1); break;
        case Qt::Key_Return: case Qt::Key_Enter: if (!selected().isEmpty()) { if (chosen) chosen(selected()); dismiss(); } break;
        case Qt::Key_Shift: case Qt::Key_Control: case Qt::Key_Alt: {
            const QString name=event->key()==Qt::Key_Shift?"Shift":event->key()==Qt::Key_Control?"Ctrl":"Alt";
            if (const int tab=tabKeys.indexOf(name); tab>=0 && modeRequested) modeRequested(Mode(tab));
            break;
        }
        default: QWidget::keyPressEvent(event);
        }
    }
};
