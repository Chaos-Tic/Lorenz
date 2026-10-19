#pragma once
#include <QtWidgets>
#include <cmath>
#include <functional>
#include <numeric>

// One native wheel for compositor gestures, the Windows gesture and
// file-context actions. Three modes: formats, tools and file actions.
//
// Look: frosted glass. The desktop behind the wheel is captured once when it
// opens, blurred and slightly magnified, then tinted; choices are rounded
// glass tiles on a ring. Pointing at one magnifies it like a dock while its
// neighbours make room; every value moves on a spring, so motion can be
// interrupted at any time. Where the desktop cannot be captured (Wayland),
// the glass falls back to a denser tint.
class RadialWheel : public QWidget {
public:
    enum Mode { Convert, Tools, Actions };
    struct Item { QString id, label, glyph; };
private:
    static constexpr double pi = 3.14159265358979323846;
    // A damped spring; retargeting keeps the velocity, so motion never jumps.
    struct Spring {
        qreal x = 0, v = 0, target = 0;
        void snap(qreal value) { x = target = value; v = 0; }
        bool step(qreal dt, qreal k, qreal c) {
            const int n = qMax(1, int(std::ceil(dt / 0.004)));
            const qreal h = dt / n;
            for (int i = 0; i < n; ++i) { v += (-k * (x - target) - c * v) * h; x += v * h; }
            if (std::abs(x - target) < 0.0004 && std::abs(v) < 0.0004) { snap(target); return false; }
            return true;
        }
    };
    struct Tile { Spring scale, push, radius, alpha, appear, arm; qreal delay = 0; };
    struct Tone { QColor tint, deep, lite; qreal film; };
    struct Theme { bool dark; QColor ink, ink2, glass, hub, sheen, rimHi, rimLo, tile, shadow, capsule, keyBg, keyLine, fallback; };
    QVector<Tile> tiles;
    Spring open, hubArm, pill;
    QVariantAnimation fade;
    QTimer frames;
    QElapsedTimer clock, sinceItems;
    QImage backdrop;
    qreal phase = 0, closingOpacity = 1;
    bool closing = false;
    bool loading = false;
    bool pointing = false;
    qreal pointerAngle = 0;
    QPointF lastPointer{-1000, -1000};

    static QColor rgba(int r, int g, int b, qreal a) { return QColor::fromRgbF(r / 255.0, g / 255.0, b / 255.0, a); }
    static QColor alpha(QColor c, qreal a) { c.setAlphaF(qBound(0.0, a, 1.0)); return c; }
    static QColor mix(QColor a, QColor b, qreal t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    }
    static QFont font(qreal pixels, QFont::Weight weight = QFont::Normal, bool mono = false) {
        QFont f = QApplication::font();
        f.setFamilies(mono ? QStringList{"Geist Mono", "Cascadia Mono", "JetBrains Mono", "monospace"}
                           : QStringList{"Geist", "Segoe UI Variable Text", "Segoe UI", "Inter", "Cantarell", f.family()});
        f.setPixelSize(qRound(pixels)); f.setWeight(weight);
        return f;
    }
    // Each wheel sees the same thin-film glass from its own angle.
    static Tone tone(Mode m) {
        if (m == Tools) return {QColor(255, 168, 64), QColor("#8a4a00"), QColor("#ffd08a"), 40};
        if (m == Actions) return {QColor(206, 108, 240), QColor("#83299f"), QColor("#ecb6fa"), 290};
        return {QColor(64, 186, 240), QColor("#0b6690"), QColor("#9fdcf8"), 190};
    }
    static bool darkScheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (const auto scheme = QGuiApplication::styleHints()->colorScheme(); scheme != Qt::ColorScheme::Unknown)
            return scheme == Qt::ColorScheme::Dark;
#endif
        return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
    }
    static Theme theme(bool dark) {
        if (dark) return {true, QColor("#f3f4f8"), QColor("#bcc2d0"), rgba(22, 22, 40, .34), rgba(30, 30, 52, .46), rgba(255, 255, 255, .14),
            rgba(255, 255, 255, .5), rgba(255, 255, 255, .12), rgba(255, 255, 255, .06), rgba(0, 0, 0, .7), rgba(24, 24, 44, .42),
            rgba(255, 255, 255, .08), rgba(255, 255, 255, .18), rgba(26, 28, 44, .9)};
        return {false, QColor("#151820"), QColor("#4a5060"), rgba(255, 255, 255, .30), rgba(255, 255, 255, .52), rgba(255, 255, 255, .42),
            rgba(255, 255, 255, .98), rgba(255, 255, 255, .4), rgba(255, 255, 255, .22), rgba(42, 30, 92, .5), rgba(255, 255, 255, .4),
            rgba(21, 24, 32, .06), rgba(21, 24, 32, .14), rgba(244, 245, 250, .9)};
    }
    // Short descriptions shown under a format and in the hub.
    static QPair<QString, QString> describe(const QString &id) {
        static const QHash<QString, QPair<QString, QString>> text{
            {"jpg", {"Universal", "Opens anywhere"}}, {"jpeg", {"Universal", "Opens anywhere"}},
            {"webp", {"Web", "Smaller, keeps quality"}}, {"avif", {"Smallest", "Smallest file, newer apps"}},
            {"png", {"Lossless", "Sharp edges, transparency"}}, {"pdf", {"Document", "One page per image"}},
            {"tiff", {"Lossless", "Lossless, for print"}}, {"tif", {"Lossless", "Lossless, for print"}},
            {"heic", {"Compact", "Compact, phone photos"}}, {"gif", {"Animation", "Short loops"}}, {"bmp", {"Bitmap", "Uncompressed"}},
            {"mp3", {"Universal", "Plays everywhere"}}, {"m4a", {"AAC", "Small, good quality"}}, {"flac", {"Lossless", "Exact copy of the sound"}},
            {"wav", {"Raw", "Uncompressed audio"}}, {"ogg", {"Vorbis", "Open format"}}, {"opus", {"Opus", "Smallest for speech"}},
            {"mp4", {"Universal", "Plays everywhere"}}, {"webm", {"Web", "Open web video"}}, {"mkv", {"Matroska", "Keeps every track"}},
            {"mov", {"QuickTime", "Apple devices"}}, {"docx", {"Word", "Editable document"}}, {"odt", {"OpenDocument", "Editable document"}},
            {"md", {"Markdown", "Plain text with formatting"}}, {"html", {"Web page", "Opens in a browser"}}, {"txt", {"Text", "Plain text"}},
            {"epub", {"E-book", "For readers"}}, {"compress", {"", "Same pixels, lighter file"}}, {"resize-half", {"", "Half the width and height"}},
            {"rotate-right", {"", "90° clockwise"}}, {"rotate-left", {"", "90° anticlockwise"}}, {"flip", {"", "Flip left to right"}},
            {"grayscale", {"", "Greyscale copy"}}, {"strip", {"", "Removes GPS and camera data"}}, {"zip", {"", "One archive beside it"}},
            {"copy", {"", "To the clipboard"}}, {"copy-to", {"", "Pick a folder"}}, {"move-to", {"", "Pick a folder"}},
            {"path", {"", "Full path as text"}}, {"terminal", {"", "Opens in this folder"}}, {"reveal", {"", "Reveal in the file manager"}}};
        return text.value(id.toLower());
    }
    static void arrowHead(QPainter &p, QPointF tip, qreal degrees) {
        p.drawLine(tip, tip + polar(5, degrees + 150)); p.drawLine(tip, tip + polar(5, degrees - 150));
    }
    static QPointF polar(qreal radius, qreal degrees) {
        const qreal a = degrees * pi / 180;
        return {radius * std::cos(a), -radius * std::sin(a)};
    }
    // Frosted glass source: a small, blurred and slightly saturated copy.
    static QImage frost(const QImage &source) {
        if (source.isNull()) return {};
        QImage img = source.convertToFormat(QImage::Format_RGB32).scaled(qMax(1, source.width() / 6), qMax(1, source.height() / 6),
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const int w = img.width(), h = img.height(), r = 3;
        QVector<QRgb> line(qMax(w, h));
        for (int pass = 0; pass < 3; ++pass) {
            for (int y = 0; y < h; ++y) {
                auto *row = reinterpret_cast<QRgb *>(img.scanLine(y));
                for (int x = 0; x < w; ++x) line[x] = row[x];
                for (int x = 0; x < w; ++x) {
                    int sr = 0, sg = 0, sb = 0, n = 0;
                    for (int k = qMax(0, x - r); k <= qMin(w - 1, x + r); ++k) { sr += qRed(line[k]); sg += qGreen(line[k]); sb += qBlue(line[k]); ++n; }
                    row[x] = qRgb(sr / n, sg / n, sb / n);
                }
            }
            for (int x = 0; x < w; ++x) {
                for (int y = 0; y < h; ++y) line[y] = reinterpret_cast<QRgb *>(img.scanLine(y))[x];
                for (int y = 0; y < h; ++y) {
                    int sr = 0, sg = 0, sb = 0, n = 0;
                    for (int k = qMax(0, y - r); k <= qMin(h - 1, y + r); ++k) { sr += qRed(line[k]); sg += qGreen(line[k]); sb += qBlue(line[k]); ++n; }
                    reinterpret_cast<QRgb *>(img.scanLine(y))[x] = qRgb(sr / n, sg / n, sb / n);
                }
            }
        }
        for (int y = 0; y < h; ++y) {
            auto *row = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const int l = qGray(row[x]);
                auto boost = [l](int c) { return qBound(0, l + (c - l) * 16 / 10, 255); };
                row[x] = qRgb(boost(qRed(row[x])), boost(qGreen(row[x])), boost(qBlue(row[x])));
            }
        }
        return img;
    }
    void animate() { if (!frames.isActive()) { clock.restart(); frames.start(); } }
    int count() const { return int(items.size()); }
    qreal sigma() const { return count() > 6 ? 0.5 : 0.62; }
    qreal slot(int i) const { return count() ? i * 2 * pi / count() : 0; }
    // Targets of the dock-like magnification: the springs chase them.
    void retarget() {
        const int n = count();
        if (tiles.size() != n) tiles.resize(n);
        for (int i = 0; i < n; ++i) {
            Tile &t = tiles[i];
            if (pointing && n) {
                qreal d = slot(i) - pointerAngle;
                while (d > pi) d -= 2 * pi;
                while (d < -pi) d += 2 * pi;
                const qreal u = d / sigma(), w = std::exp(-u * u);
                t.scale.target = 0.88 + (magnify - 0.88) * w;
                t.push.target = 0.2 * u * std::exp(-u * u / 2);
                t.radius.target = ring + 10 * w;
                t.alpha.target = 0.6 + 0.4 * w;
            } else {
                t.scale.target = 1; t.push.target = 0; t.radius.target = ring; t.alpha.target = 1;
            }
            t.arm.target = i == hovered ? 1 : 0;
        }
        hubArm.target = hovered >= 0 ? 1 : 0;
        animate();
    }
    void hoverAt(QPointF local) {
        lastPointer = local;
        const QPointF d = local - center();
        const qreal r = std::hypot(d.x(), d.y());
        pointing = !closing && count() && r >= hubRadius + 4 && r <= discRadius + 12;
        if (pointing) { pointerAngle = std::atan2(d.x(), -d.y()); if (pointerAngle < 0) pointerAngle += 2 * pi; }
        hovered = closing ? -1 : hitTest(local);
        retarget(); update();
    }
    // Rest positions of the tiles, before magnification: one per slot, the
    // first at the top, clockwise.
    void restartAppear() {
        tiles.resize(count());
        for (int i = 0; i < tiles.size(); ++i) {
            Tile &t = tiles[i];
            t.appear.snap(0); t.appear.target = 0; t.delay = 40 + i * 18;
            t.scale.snap(1); t.push.snap(0); t.radius.snap(ring); t.alpha.snap(1); t.arm.snap(0);
        }
        sinceItems.restart();
    }
public:
    // Layout, in widget pixels: the disc on top, the key bar under it.
    static constexpr int extent = 560;
    static constexpr int tall = 620;
    static constexpr qreal discRadius = 220, hubRadius = 72, ring = 150;
    static QPointF center() { return {extent / 2.0, 250}; }
    qreal magnify = 1.32;
    QStringList formats; // Item identifiers, kept for callers and tests.
    QVector<Item> items;
    Mode mode = Convert;
    QString message = "Reading files…";
    QString sourceName;
    QString sourceType;
    QStringList tabKeys{"Shift", "Ctrl", "Alt"};
    bool showTabs = true;
    // The desktop under the wheel is captured when it opens, unless the
    // platform forbids it (Wayland clients, the KWin extension).
    bool captureBackdrop = !QGuiApplication::platformName().startsWith("wayland");
    int hovered = -1;
    std::function<void(QString)> chosen;
    std::function<void(Mode)> modeRequested;
    // Format extensions map to families; tools and actions name their glyph.
    static QString glyphFor(const QString &type) {
        if (QStringList{"png", "jpg", "jpeg", "webp", "avif", "bmp", "tif", "tiff", "gif", "heic", "svg"}.contains(type)) return "image";
        if (QStringList{"mp3", "m4a", "wav", "flac", "ogg", "opus", "aiff", "wma"}.contains(type)) return "audio";
        if (QStringList{"mp4", "mov", "mkv", "webm", "avi", "wmv"}.contains(type)) return "video";
        if (QStringList{"zip", "tar", "tgz", "gz", "bz2", "xz", "7z", "rar"}.contains(type)) return "archive";
        if (QStringList{"xlsx", "xls", "ods", "csv"}.contains(type)) return "sheet";
        if (QStringList{"ppt", "pptx", "odp"}.contains(type)) return "slides";
        if (QStringList{"srt", "vtt"}.contains(type)) return "subtitle";
        if (type == "pdf") return "pdf";
        if (type == "folder") return "folder";
        return "document";
    }
    static void glyph(QPainter &p, const QString &name) {
        p.setBrush(Qt::NoBrush);
        auto page = [&](qreal w = 15) {
            QPainterPath d; d.moveTo(-w / 2, -10); d.lineTo(w / 2 - 5, -10); d.lineTo(w / 2, -5); d.lineTo(w / 2, 10); d.lineTo(-w / 2, 10); d.closeSubpath();
            d.moveTo(w / 2 - 5, -10); d.lineTo(w / 2 - 5, -5); d.lineTo(w / 2, -5); p.drawPath(d);
        };
        if (name == "image") {
            p.drawRoundedRect(QRectF(-10, -9, 20, 18), 4, 4);
            p.drawEllipse(QPointF(4, -3), 2, 2);
            QPainterPath hills; hills.moveTo(-9, 6); hills.lineTo(-3, -1); hills.lineTo(2, 5); hills.lineTo(5, 2); hills.lineTo(9, 6); p.drawPath(hills);
        } else if (name == "audio") {
            p.drawLine(QPointF(-3, 6), QPointF(-3, -7)); p.drawLine(QPointF(-3, -7), QPointF(8, -10));
            p.drawLine(QPointF(8, -10), QPointF(8, 3)); p.drawEllipse(QPointF(-6, 7), 3, 2.5); p.drawEllipse(QPointF(5, 4), 3, 2.5);
        } else if (name == "video") {
            p.drawRoundedRect(QRectF(-11, -8, 22, 16), 4, 4);
            QPainterPath play; play.moveTo(-2, -4); play.lineTo(4, 0); play.lineTo(-2, 4); play.closeSubpath(); p.drawPath(play);
        } else if (name == "pdf") {
            page(); p.drawLine(QPointF(-4, 0), QPointF(3, 0)); p.drawLine(QPointF(-4, 4), QPointF(3, 4));
            p.drawRoundedRect(QRectF(-10, -5, 9, 6), 1.5, 1.5);
        } else if (name == "sheet") {
            p.drawRoundedRect(QRectF(-10, -9, 20, 18), 3, 3); p.drawLine(QPointF(-10, -3), QPointF(10, -3));
            p.drawLine(QPointF(-10, 3), QPointF(10, 3)); p.drawLine(QPointF(-3, -9), QPointF(-3, 9));
        } else if (name == "slides") {
            p.drawRoundedRect(QRectF(-11, -9, 22, 14), 3, 3); p.drawLine(QPointF(0, 5), QPointF(0, 9));
            p.drawLine(QPointF(-4, 9), QPointF(4, 9)); p.drawLine(QPointF(-5, -1), QPointF(-1, -4)); p.drawLine(QPointF(-1, -4), QPointF(5, -1));
        } else if (name == "subtitle") {
            p.drawRoundedRect(QRectF(-11, -8, 22, 16), 4, 4); p.drawLine(QPointF(-6, 2), QPointF(1, 2)); p.drawLine(QPointF(4, 2), QPointF(6, 2));
            p.drawLine(QPointF(-6, -2), QPointF(-3, -2)); p.drawLine(QPointF(0, -2), QPointF(6, -2));
        } else if (name == "archive") {
            p.drawRoundedRect(QRectF(-9, -10, 18, 20), 3, 3);
            for (int y : {-7, -3, 1}) { p.drawLine(QPointF(-1, y), QPointF(1, y)); p.drawLine(QPointF(1, y + 2), QPointF(-1, y + 2)); }
            p.drawRoundedRect(QRectF(-2.5, 4, 5, 4), 1, 1);
        } else if (name == "compress") {
            p.drawLine(QPointF(0, -11), QPointF(0, -3)); arrowHead(p, QPointF(0, -3), -90);
            p.drawLine(QPointF(0, 11), QPointF(0, 3)); arrowHead(p, QPointF(0, 3), 90); p.drawLine(QPointF(-9, 0), QPointF(9, 0));
        } else if (name == "resize") {
            p.drawRoundedRect(QRectF(-10, -10, 20, 20), 3, 3); p.drawRoundedRect(QRectF(-10, 0, 10, 10), 2, 2);
            p.drawLine(QPointF(2, -2), QPointF(7, -7)); arrowHead(p, QPointF(7, -7), 45);
        } else if (name == "rotate" || name == "rotate-left") {
            p.save(); if (name == "rotate-left") p.scale(-1, 1);
            p.drawArc(QRectF(-9, -9, 18, 18), 100 * 16, -280 * 16); arrowHead(p, polar(9, 100), 10); p.restore();
        } else if (name == "flip") {
            QPainterPath l; l.moveTo(-2, -9); l.lineTo(-10, 8); l.lineTo(-2, 8); l.closeSubpath(); p.drawPath(l);
            QPainterPath r; r.moveTo(2, -9); r.lineTo(10, 8); r.lineTo(2, 8); r.closeSubpath(); p.drawPath(r);
        } else if (name == "grayscale") {
            p.drawEllipse(QPointF(0, 0), 10, 10); QPainterPath half; half.moveTo(0, -10); half.arcTo(QRectF(-10, -10, 20, 20), 90, 180); half.closeSubpath();
            p.save(); p.setBrush(p.pen().color()); p.drawPath(half); p.restore();
        } else if (name == "strip") {
            page(); p.drawLine(QPointF(-10, 10), QPointF(10, -10));
        } else if (name == "mute") {
            QPainterPath s; s.moveTo(-10, -3); s.lineTo(-5, -3); s.lineTo(1, -9); s.lineTo(1, 9); s.lineTo(-5, 3); s.lineTo(-10, 3); s.closeSubpath(); p.drawPath(s);
            p.drawLine(QPointF(5, -4), QPointF(11, 4)); p.drawLine(QPointF(11, -4), QPointF(5, 4));
        } else if (name == "speed") {
            for (qreal x : {-9.0, 0.0}) { QPainterPath t; t.moveTo(x, -8); t.lineTo(x + 9, 0); t.lineTo(x, 8); t.closeSubpath(); p.drawPath(t); }
        } else if (name == "normalize") {
            for (int i = 0; i < 5; ++i) { const qreal h = QList<qreal>{6, 12, 16, 10, 7}[i]; p.drawLine(QPointF(-8 + i * 4, -h / 2), QPointF(-8 + i * 4, h / 2)); }
            p.drawLine(QPointF(-11, -6), QPointF(11, -6));
        } else if (name == "merge") {
            p.drawLine(QPointF(-9, -9), QPointF(0, 0)); p.drawLine(QPointF(9, -9), QPointF(0, 0));
            p.drawLine(QPointF(0, 0), QPointF(0, 10)); arrowHead(p, QPointF(0, 10), -90);
        } else if (name == "split") {
            p.drawLine(QPointF(0, 10), QPointF(0, 1)); p.drawLine(QPointF(0, 1), QPointF(-8, -8)); p.drawLine(QPointF(0, 1), QPointF(8, -8));
            arrowHead(p, QPointF(-8, -8), 135); arrowHead(p, QPointF(8, -8), 45);
        } else if (name == "extract") {
            p.drawRoundedRect(QRectF(-10, -2, 20, 12), 3, 3); p.drawLine(QPointF(0, 4), QPointF(0, -11)); arrowHead(p, QPointF(0, -11), 90);
        } else if (name == "share") {
            p.drawLine(QPointF(0, 3), QPointF(0, -11)); arrowHead(p, QPointF(0, -11), 90);
            QPainterPath b; b.moveTo(-5, -4); b.lineTo(-9, -4); b.lineTo(-9, 10); b.lineTo(9, 10); b.lineTo(9, -4); b.lineTo(5, -4); p.drawPath(b);
        } else if (name == "float") {
            p.drawRoundedRect(QRectF(-8, -11, 16, 14), 3, 3); p.drawLine(QPointF(-6, 8), QPointF(6, 8)); p.drawLine(QPointF(-3, 11), QPointF(3, 11));
        } else if (name == "copy" || name == "copy-to") {
            p.drawRoundedRect(QRectF(-10, -10, 13, 15), 3, 3); p.drawRoundedRect(QRectF(-3, -4, 13, 15), 3, 3);
            if (name == "copy-to") { p.drawLine(QPointF(0, 3), QPointF(7, 3)); arrowHead(p, QPointF(7, 3), 0); }
        } else if (name == "move") {
            p.drawRoundedRect(QRectF(-11, -6, 12, 14), 3, 3); p.drawLine(QPointF(-1, 1), QPointF(11, 1)); arrowHead(p, QPointF(11, 1), 0);
        } else if (name == "path") {
            p.drawRoundedRect(QRectF(-11, -6, 10, 12), 5, 5); p.drawRoundedRect(QRectF(1, -6, 10, 12), 5, 5); p.drawLine(QPointF(-4, 0), QPointF(4, 0));
        } else if (name == "terminal") {
            p.drawRoundedRect(QRectF(-11, -9, 22, 18), 4, 4); p.drawLine(QPointF(-6, -3), QPointF(-2, 0)); p.drawLine(QPointF(-2, 0), QPointF(-6, 3)); p.drawLine(QPointF(0, 4), QPointF(6, 4));
        } else if (name == "folder") {
            QPainterPath f; f.moveTo(-11, -7); f.lineTo(-4, -7); f.lineTo(-2, -4); f.lineTo(11, -4); f.lineTo(11, 9); f.lineTo(-11, 9); f.closeSubpath(); p.drawPath(f);
        } else {
            page(); p.drawLine(QPointF(-3, 1), QPointF(4, 1)); p.drawLine(QPointF(-3, 5), QPointF(2, 5));
        }
    }
    RadialWheel() : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setMouseTracking(true); setFixedSize(extent, tall);
        setWindowTitle("Lorenz wheel"); setObjectName("lorenz-wheel");
        connect(&fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) { closingOpacity = value.toReal(); update(); });
        connect(&fade, &QVariantAnimation::finished, this, [this] { if (closing) hide(); });
        frames.setInterval(8); frames.setTimerType(Qt::PreciseTimer);
        connect(&frames, &QTimer::timeout, this, [this] {
            const qreal dt = qMin(qreal(clock.restart()), 48.0) / 1000.0;
            phase = std::fmod(phase + dt * 260, 360.0);
            bool moving = open.step(dt, 380, 34);
            moving |= hubArm.step(dt, 300, 24);
            moving |= pill.step(dt, 420, 30);
            const qreal elapsed = sinceItems.isValid() ? sinceItems.elapsed() : 1e9;
            for (Tile &t : tiles) {
                if (elapsed >= t.delay) t.appear.target = 1;
                else if (t.delay < 1e8) moving = true;
                moving |= t.appear.step(dt, 380, 34);
                moving |= t.scale.step(dt, 420, 30);
                moving |= t.push.step(dt, 420, 30);
                moving |= t.radius.step(dt, 420, 30);
                moving |= t.alpha.step(dt, 420, 40);
                moving |= t.arm.step(dt, 500, 46);
            }
            update();
            if (!moving && !loading) frames.stop();
        });
    }
    void setItems(QVector<Item> values) {
        items = std::move(values); formats.clear(); for (const auto &i : items) formats << i.id;
        loading = false; restartAppear();
        if (hovered >= count()) hovered = -1;
        if (lastPointer.x() > -1000) hoverAt(lastPointer); else retarget();
        update();
    }
    void setFormats(const QStringList &values) {
        QVector<Item> list; for (const auto &f : values) list.append({f, f.toUpper(), glyphFor(f)}); setItems(list);
    }
    // Switching wheels keeps the ring still: the tiles fade out, and the
    // new ones appear in place when their items arrive.
    void setMode(Mode value) {
        if (mode == value) return;
        mode = value; hovered = -1; pill.target = int(mode);
        for (Tile &t : tiles) { t.appear.target = 0; t.delay = 1e9; }
        sinceItems.restart(); retarget(); update();
    }
    void setLoading(bool value) { loading = value; if (isVisible()) animate(); }
    void openAt(QPoint point) {
        fade.stop(); closing = false; closingOpacity = 1; setAttribute(Qt::WA_TransparentForMouseEvents, false);
        if (windowHandle()) windowHandle()->setFlag(Qt::WindowTransparentForInput, false);
        QPoint topLeft = point - center().toPoint();
        if (auto *screen = QGuiApplication::screenAt(point)) {
            const QRect area = screen->availableGeometry();
            topLeft.setX(qBound(area.left(), topLeft.x(), qMax(area.left(), area.right() - width() + 1)));
            topLeft.setY(qBound(area.top(), topLeft.y(), qMax(area.top(), area.bottom() - height() + 1)));
            // The desktop does not move during a drag: capture it once.
            if (captureBackdrop && !isVisible()) {
                const QRect g = screen->geometry();
                backdrop = frost(screen->grabWindow(0, topLeft.x() - g.x(), topLeft.y() - g.y(), width(), height()).toImage());
            }
        }
        move(topLeft); hovered = -1; pointing = false; lastPointer = QPointF(-1000, -1000);
        pill.snap(int(mode)); hubArm.snap(0); open.snap(0); open.target = 1;
        restartAppear(); show(); raise(); animate();
    }
    void dismiss() {
        if (!isVisible() || closing) return;
        closing = true; loading = false; setAttribute(Qt::WA_TransparentForMouseEvents);
        // The native input region must also disappear while the pixels fade out.
        if (windowHandle()) windowHandle()->setFlag(Qt::WindowTransparentForInput, true);
        fade.stop(); fade.setDuration(110); fade.setStartValue(1.0); fade.setEndValue(0.0);
        fade.setEasingCurve(QEasingCurve::InCubic); fade.start();
    }
    // The choice under a point: the nearest slot by angle, anywhere on the
    // disc outside the hub. The hub cancels.
    int hitTest(QPointF local) const {
        if (items.isEmpty()) return -1;
        const QPointF d = local - center();
        const qreal r = std::hypot(d.x(), d.y());
        if (r < hubRadius + 4 || r > discRadius + 12) return -1;
        qreal a = std::atan2(d.x(), -d.y()); if (a < 0) a += 2 * pi;
        return int(std::lround(a / (2 * pi / count()))) % count();
    }
    // Where a choice sits at rest.
    QPointF itemCenter(int i) const {
        const qreal a = slot(i);
        return center() + QPointF(ring * std::sin(a), -ring * std::cos(a));
    }
    QRectF tabRect(int index) const {
        const QRectF bar = barRect();
        return QRectF(bar.left() + 5 + index * 132, bar.top() + 5, 132, 44);
    }
    QRectF barRect() const { return QRectF(extent / 2.0 - 254, center().y() + discRadius + 42, 508, 54); }
    int tabAt(QPointF local) const {
        if (!showTabs) return -1;
        for (int i = 0; i < 3; ++i) if (tabRect(i).contains(local)) return i;
        return -1;
    }
    // Keyboard selection: arrows walk around the wheel, starting at the top.
    void step(int delta) {
        if (items.isEmpty()) return;
        hovered = hovered < 0 ? (delta > 0 ? 0 : count() - 1) : (hovered + delta + count()) % count();
        pointing = true; pointerAngle = slot(hovered); retarget(); update();
    }
    void trackGlobal(QPointF pos) { hoverAt(pos - QPointF(x(), y())); }
    QString selected() const { return hovered >= 0 && hovered < count() ? items[hovered].id : QString(); }
    QString selectedLabel() const { return hovered >= 0 && hovered < count() ? items[hovered].label : QString(); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        const Theme t = theme(darkScheme());
        const Tone tn = tone(mode);
        const QColor accentText = t.dark ? tn.lite : tn.deep;
        const qreal o = qBound(0.0, open.x, 1.2);
        p.setOpacity(qBound(0.0, o, 1.0) * closingOpacity);
        const QPointF c = center();
        // Disc: scales in from 0.92.
        p.save();
        p.translate(c); p.scale(0.92 + 0.08 * o, 0.92 + 0.08 * o); p.translate(-c);
        const QRectF disc(c.x() - discRadius, c.y() - discRadius, 2 * discRadius, 2 * discRadius);
        QRadialGradient shade(c + QPointF(0, 30), discRadius + 50);
        shade.setColorAt(0.70, alpha(t.shadow, t.shadow.alphaF() * 0.55)); shade.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(shade); p.drawEllipse(c + QPointF(0, 30), discRadius + 50, discRadius + 50);
        QPainterPath discPath; discPath.addEllipse(disc);
        paintGlass(p, discPath, c, 1.07, t.glass, t);
        QRadialGradient wash(c, discRadius);
        wash.setColorAt(0.52, alpha(tn.tint, 0)); wash.setColorAt(1, alpha(tn.tint, t.dark ? 0.2 : 0.14));
        p.setBrush(wash); p.drawPath(discPath);
        QLinearGradient sheen(disc.topLeft(), disc.topLeft() + QPointF(disc.width() * 0.42, disc.height() * 0.9));
        sheen.setColorAt(0, t.sheen); sheen.setColorAt(0.44, alpha(t.sheen, 0));
        p.setBrush(sheen); p.drawPath(discPath);
        paintRim(p, disc, t, tn, true);
        p.restore();
        // Tiles, the most magnified last so it sits on top.
        QVector<int> order(count());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [this](int a, int b) { return tiles.value(a).scale.x < tiles.value(b).scale.x; });
        for (int i : order) paintTile(p, i, t, tn, accentText);
        paintHub(p, t, tn, accentText);
        if (showTabs) paintBar(p, t);
    }
    void hideEvent(QHideEvent *event) override { fade.stop(); frames.stop(); closing = false; backdrop = QImage(); QWidget::hideEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { hoverAt(event->position()); }
    void leaveEvent(QEvent *event) override { if (!closing) { pointing = false; hovered = -1; retarget(); } QWidget::leaveEvent(event); }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (closing) return;
        if (const int tab = tabAt(event->position()); tab >= 0 && modeRequested) { modeRequested(Mode(tab)); return; }
        hoverAt(event->position()); if (chosen) chosen(selected()); dismiss();
    }
    void keyPressEvent(QKeyEvent *event) override {
        switch (event->key()) {
        case Qt::Key_Escape: if (chosen) chosen({}); dismiss(); break;
        case Qt::Key_Left: case Qt::Key_Up: step(-1); break;
        case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_Tab: step(1); break;
        case Qt::Key_Return: case Qt::Key_Enter: if (!selected().isEmpty()) { if (chosen) chosen(selected()); dismiss(); } break;
        case Qt::Key_Shift: case Qt::Key_Control: case Qt::Key_Alt: {
            const QString name = event->key() == Qt::Key_Shift ? "Shift" : event->key() == Qt::Key_Control ? "Ctrl" : "Alt";
            if (const int tab = tabKeys.indexOf(name); tab >= 0 && modeRequested) modeRequested(Mode(tab));
            break;
        }
        default: QWidget::keyPressEvent(event);
        }
    }
private:
    // Frosted glass inside a shape: the captured desktop, blurred and
    // magnified around `focus`, under a translucent tint.
    void paintGlass(QPainter &p, const QPainterPath &shape, QPointF focus, qreal zoom, QColor tint, const Theme &t) {
        p.save();
        p.setClipPath(shape, Qt::IntersectClip);
        if (!backdrop.isNull()) {
            p.save(); p.translate(focus); p.scale(zoom, zoom); p.translate(-focus);
            p.drawImage(QRectF(0, 0, width(), height()), backdrop);
            p.restore();
            p.fillPath(shape, tint);
        } else {
            p.fillPath(shape, t.fallback);
            p.fillPath(shape, alpha(tint, tint.alphaF() * 0.5));
        }
        p.restore();
    }
    // A fine light edge, brighter at the top, plus the thin-film shimmer.
    void paintRim(QPainter &p, const QRectF &r, const Theme &t, const Tone &tn, bool film) {
        QConicalGradient light(r.center(), 210);
        light.setColorAt(0, t.rimHi); light.setColorAt(0.22, alpha(t.rimHi, 0)); light.setColorAt(0.44, alpha(t.rimHi, 0));
        light.setColorAt(0.52, alpha(t.rimHi, t.rimHi.alphaF() * 0.6)); light.setColorAt(0.64, alpha(t.rimHi, 0));
        light.setColorAt(0.86, alpha(t.rimHi, 0)); light.setColorAt(1, t.rimHi);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QBrush(light), 1.6)); p.drawEllipse(r.adjusted(0.8, 0.8, -0.8, -0.8));
        p.setPen(QPen(t.rimLo, 0.5)); p.drawEllipse(r.adjusted(0.5, 0.5, -0.5, -0.5));
        if (!film) return;
        QConicalGradient shimmer(r.center(), 90 - tn.film);
        shimmer.setColorAt(0, rgba(120, 224, 255, .95)); shimmer.setColorAt(0.18, rgba(170, 150, 255, .9));
        shimmer.setColorAt(0.34, rgba(255, 150, 214, .85)); shimmer.setColorAt(0.5, rgba(255, 214, 140, .8));
        shimmer.setColorAt(0.62, rgba(255, 255, 255, 0)); shimmer.setColorAt(0.8, rgba(255, 255, 255, 0)); shimmer.setColorAt(1, rgba(120, 224, 255, .95));
        p.save(); p.setOpacity(p.opacity() * (t.dark ? 0.7 : 0.85));
        p.setPen(QPen(QBrush(shimmer), 1.2)); p.drawEllipse(r.adjusted(-0.4, -0.4, 0.4, 0.4));
        p.restore();
    }
    void paintTile(QPainter &p, int i, const Theme &t, const Tone &tn, QColor accentText) {
        if (i >= tiles.size()) return;
        const Tile &s = tiles[i];
        const qreal appear = qBound(0.0, s.appear.x, 1.2);
        if (appear <= 0.001) return;
        const qreal a = slot(i) + s.push.x;
        const QPointF at = center() + QPointF(s.radius.x * std::sin(a), -s.radius.x * std::cos(a));
        const qreal scale = s.scale.x * (0.86 + 0.14 * appear), arm = qBound(0.0, s.arm.x, 1.0);
        p.save();
        p.setOpacity(p.opacity() * qBound(0.0, appear, 1.0) * qBound(0.0, s.alpha.x, 1.0));
        p.translate(at); p.scale(scale, scale);
        const QRectF box(-46, -40, 92, 80);
        QPainterPath shape; shape.addRoundedRect(box, 20, 20);
        if (arm > 0.01) {
            QRadialGradient glow(QPointF(0, 14), 60);
            glow.setColorAt(0, alpha(tn.tint, 0.45 * arm)); glow.setColorAt(1, alpha(tn.tint, 0));
            p.setPen(Qt::NoPen); p.setBrush(glow); p.drawEllipse(QPointF(0, 16), 62, 40);
        }
        p.fillPath(shape, mix(t.tile, alpha(tn.tint, t.dark ? 0.3 : 0.2), arm));
        if (arm > 0.01) {
            QLinearGradient lit(box.topLeft(), box.bottomLeft());
            lit.setColorAt(0, rgba(255, 255, 255, (t.dark ? 0.2 : 0.7) * arm)); lit.setColorAt(1, rgba(255, 255, 255, (t.dark ? 0.06 : 0.4) * arm));
            p.fillPath(shape, lit);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(mix(t.rimLo, alpha(tn.tint, t.dark ? 0.7 : 0.55), arm), 0.5 + 0.5 * arm)); p.drawPath(shape);
        QPainterPath top; top.moveTo(box.left() + 18, box.top() + 0.8); top.lineTo(box.right() - 18, box.top() + 0.8);
        p.setPen(QPen(t.rimHi, 1)); p.drawPath(top);
        const QColor ink = mix(t.ink, accentText, arm);
        const Item &item = items[i];
        const auto [desc, hint] = describe(item.id);
        Q_UNUSED(hint);
        if (mode == Convert) {
            p.setPen(ink); p.setFont(font(item.label.size() > 5 ? 15 : 18, QFont::DemiBold));
            p.drawText(QRectF(-44, desc.isEmpty() ? -12 : -18, 88, 24), Qt::AlignCenter, item.label);
            if (!desc.isEmpty()) {
                p.setPen(mix(t.ink2, accentText, arm)); p.setFont(font(11, QFont::Medium));
                p.drawText(QRectF(-44, 6, 88, 16), Qt::AlignCenter, desc);
            }
        } else {
            p.save(); p.translate(0, -12);
            p.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); glyph(p, item.glyph);
            p.restore();
            p.setPen(ink); p.setFont(font(12, QFont::Medium));
            p.drawText(QRectF(-44, 4, 88, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
        }
        p.restore();
    }
    void paintHub(QPainter &p, const Theme &t, const Tone &tn, QColor accentText) {
        const QPointF c = center();
        const qreal arm = qBound(0.0, hubArm.x, 1.0);
        const QRectF r(c.x() - hubRadius, c.y() - hubRadius, 2 * hubRadius, 2 * hubRadius);
        QRadialGradient shade(c + QPointF(0, 10), hubRadius + 18);
        shade.setColorAt(0.75, alpha(t.shadow, 0.35)); shade.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(shade); p.drawEllipse(c + QPointF(0, 10), hubRadius + 18, hubRadius + 18);
        QPainterPath hub; hub.addEllipse(r);
        paintGlass(p, hub, c, 1.18, t.hub, t);
        p.fillPath(hub, alpha(tn.tint, (t.dark ? 0.22 : 0.14) * arm));
        QRadialGradient gloss(QPointF(c.x() - 0.16 * r.width(), r.top() + 0.1 * r.height()), r.width() * 0.62);
        gloss.setColorAt(0, t.dark ? rgba(255, 255, 255, .18) : rgba(255, 255, 255, .75)); gloss.setColorAt(1, rgba(255, 255, 255, 0));
        p.fillPath(hub, gloss);
        paintRim(p, r, t, tn, false);
        if (arm > 0.01) { p.setPen(QPen(alpha(tn.tint, 0.35 * arm), 1)); p.setBrush(Qt::NoBrush); p.drawEllipse(r.adjusted(-0.5, -0.5, 0.5, 0.5)); }
        static const QStringList titles{"Convert", "Tools", "Actions"};
        static const QStringList verbs{"Release to convert", "Release to apply", "Release to run"};
        // At rest: what is dragged and which wheel is open.
        p.save();
        p.setOpacity(p.opacity() * (1 - arm));
        p.translate(c); p.scale(1 + 0.06 * arm, 1 + 0.06 * arm);
        if (loading) {
            p.setPen(QPen(accentText, 2, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(-10, -40, 20, 20), int(-phase * 16), 230 * 16);
        } else {
            p.save(); p.translate(0, -30); p.scale(0.9, 0.9);
            p.setPen(QPen(t.ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); glyph(p, glyphFor(sourceType)); p.restore();
        }
        const QString title = loading ? "Reading…" : count() == 0 ? "Unavailable" : titles[mode];
        p.setPen(t.ink); p.setFont(font(18, QFont::DemiBold));
        p.drawText(QRectF(-64, -8, 128, 24), Qt::AlignCenter, title);
        p.setPen(t.ink2); p.setFont(font(11.5, QFont::Medium));
        const QString detail = loading ? "Finding options" : count() == 0 ? message.section('\n', 0, 0) : sourceName;
        p.drawText(QRectF(-60, 18, 120, 16), Qt::AlignCenter, QFontMetrics(p.font()).elidedText(detail, Qt::ElideMiddle, 116));
        p.restore();
        // Armed: the choice, big, and what releasing does.
        const int shown = hovered >= 0 ? hovered : -1;
        if (arm > 0.01 && shown >= 0 && shown < count()) {
            const Item &item = items[shown];
            const QString hint = describe(item.id).second;
            p.save();
            p.setOpacity(p.opacity() * arm);
            p.translate(c); p.scale(0.92 + 0.08 * arm, 0.92 + 0.08 * arm);
            const int len = item.label.size();
            p.setPen(accentText); p.setFont(font(len <= 5 ? 30 : len <= 9 ? 20 : 16, QFont::DemiBold));
            const qreal top = hint.isEmpty() ? -22 : -32;
            p.drawText(QRectF(-62, top, 124, 36), Qt::AlignCenter | Qt::TextWordWrap, item.label);
            p.setPen(t.ink); p.setFont(font(11.5, QFont::DemiBold));
            p.drawText(QRectF(-62, top + 38, 124, 16), Qt::AlignCenter, verbs[mode]);
            if (!hint.isEmpty()) {
                p.setPen(t.ink2); p.setFont(font(11, QFont::Medium));
                p.drawText(QRectF(-56, top + 55, 112, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, hint);
            }
            p.restore();
        }
    }
    // The key bar: which key opens which wheel; the lit pill slides.
    void paintBar(QPainter &p, const Theme &t) {
        static const QStringList titles{"Convert", "Tools", "Actions"};
        const QRectF bar = barRect();
        QPainterPath capsule; capsule.addRoundedRect(bar, 27, 27);
        QRadialGradient shade(bar.center() + QPointF(0, 14), bar.width() * 0.5);
        shade.setColorAt(0.6, alpha(t.shadow, 0.18)); shade.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(shade); p.drawRoundedRect(bar.adjusted(-14, -4, 14, 22), 40, 40);
        paintGlass(p, capsule, bar.center(), 1.04, t.capsule, t);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(t.rimLo, 0.5)); p.drawPath(capsule);
        QPainterPath top; top.moveTo(bar.left() + 26, bar.top() + 0.8); top.lineTo(bar.right() - 26, bar.top() + 0.8);
        p.setPen(QPen(t.rimHi, 1)); p.drawPath(top);
        const Tone lit = tone(Mode(qBound(0, int(std::lround(pill.x)), 2)));
        QRectF pillRect = tabRect(0).translated(132 * pill.x, 0);
        QPainterPath pillPath; pillPath.addRoundedRect(pillRect, 22, 22);
        p.fillPath(pillPath, alpha(lit.tint, t.dark ? 0.3 : 0.2));
        QLinearGradient pillLit(pillRect.topLeft(), pillRect.bottomLeft());
        pillLit.setColorAt(0, rgba(255, 255, 255, t.dark ? 0.16 : 0.7)); pillLit.setColorAt(1, rgba(255, 255, 255, t.dark ? 0.04 : 0.35));
        p.fillPath(pillPath, pillLit);
        p.setPen(QPen(alpha(lit.tint, 0.5), 1)); p.setBrush(Qt::NoBrush); p.drawPath(pillPath);
        for (int i = 0; i < 3; ++i) {
            const QRectF r = tabRect(i);
            const bool on = i == int(mode);
            const QColor ink = on ? (t.dark ? QColor(Qt::white) : tone(Mode(i)).deep) : t.ink2;
            const QString key = tabKeys.value(i);
            const QFont keyFont = font(11, QFont::Medium, true), labelFont = font(13, QFont::Medium);
            const qreal keyW = QFontMetricsF(keyFont).horizontalAdvance(key) + 14, labelW = QFontMetricsF(labelFont).horizontalAdvance(titles[i]);
            qreal x = r.center().x() - (keyW + 8 + labelW) / 2;
            const QRectF chip(x, r.center().y() - 10, keyW, 20);
            p.setPen(QPen(t.keyLine, 0.5)); p.setBrush(t.keyBg); p.drawRoundedRect(chip, 7, 7);
            p.setFont(keyFont); p.setPen(ink); p.drawText(chip, Qt::AlignCenter, key);
            p.setFont(labelFont); p.drawText(QRectF(chip.right() + 8, r.top(), labelW + 2, r.height()), Qt::AlignVCenter | Qt::AlignLeft, titles[i]);
        }
        const qreal sep = tabRect(2).right() + 4;
        p.setPen(QPen(t.keyLine, 1)); p.drawLine(QPointF(sep, bar.center().y() - 11), QPointF(sep, bar.center().y() + 11));
        const QRectF rest(sep + 4, bar.top(), bar.right() - sep - 8, bar.height());
        const QFont keyFont = font(11, QFont::Medium, true), labelFont = font(13, QFont::Medium);
        const qreal keyW = QFontMetricsF(keyFont).horizontalAdvance("Esc") + 14, labelW = QFontMetricsF(labelFont).horizontalAdvance("Cancel");
        const QRectF chip(rest.center().x() - (keyW + 8 + labelW) / 2, rest.center().y() - 10, keyW, 20);
        p.setPen(QPen(t.keyLine, 0.5)); p.setBrush(t.keyBg); p.drawRoundedRect(chip, 7, 7);
        p.setFont(keyFont); p.setPen(t.ink); p.drawText(chip, Qt::AlignCenter, "Esc");
        p.setFont(labelFont); p.setPen(t.ink2); p.drawText(QRectF(chip.right() + 8, rest.top(), labelW + 2, rest.height()), Qt::AlignVCenter | Qt::AlignLeft, "Cancel");
    }
};
