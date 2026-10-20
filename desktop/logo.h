#pragma once
#include <QtWidgets>
#include <cmath>

// The Lorenz icon (assets/lorenz.svg), drawn with QPainter so the window,
// the sidebar and the installed icon are the same picture: the frosted wheel
// of the README banner, one choice armed, on the soft peach, blue and lilac
// tile shared by Chaos-Tic applications.
inline void lorenzLogo(QPainter &p, const QRectF &box) {
    auto rgba = [](int r, int g, int b, qreal a) { return QColor::fromRgbF(r / 255.0, g / 255.0, b / 255.0, a); };
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.translate(box.topLeft()); p.scale(box.width() / 256.0, box.height() / 256.0);
    const QRectF tileRect(8, 8, 240, 240);
    QPainterPath tile; tile.addRoundedRect(tileRect, 58, 58);
    p.save(); p.setClipPath(tile);
    p.fillRect(tileRect, QColor("#f1eefb"));
    auto blob = [&](QPointF c, qreal r, const char *color) {
        QColor clear(color); clear.setAlphaF(0);
        QRadialGradient g(c, r * 1.6); g.setColorAt(0, QColor(color)); g.setColorAt(1, clear);
        p.fillRect(tileRect, g);
    };
    blob({40, 40}, 90, "#ffb48f"); blob({235, 110}, 95, "#8fcaff"); blob({120, 250}, 95, "#d6b0ff");
    p.restore();
    p.setBrush(Qt::NoBrush); p.setPen(QPen(rgba(255, 255, 255, .9), 2));
    p.drawRoundedRect(QRectF(9, 9, 238, 238), 57, 57);
    // Frosted disc with a thin-film rim.
    const QPointF c(128, 128);
    for (int i = 6; i >= 1; --i) {
        p.setPen(Qt::NoPen); p.setBrush(rgba(42, 30, 92, .035));
        p.drawEllipse(c + QPointF(0, 8), 94 + i * 2.5, 94 + i * 2.5);
    }
    p.setBrush(rgba(255, 255, 255, .38)); p.drawEllipse(c, 94, 94);
    QLinearGradient sheen(0, 34, 0, 222); sheen.setColorAt(0, rgba(255, 255, 255, .75)); sheen.setColorAt(.55, rgba(255, 255, 255, 0));
    p.setBrush(sheen); p.drawEllipse(c, 94, 94);
    p.setBrush(Qt::NoBrush); p.setPen(QPen(Qt::white, 2)); p.drawEllipse(c, 93, 93);
    QLinearGradient film(34, 34, 222, 222);
    film.setColorAt(0, QColor("#78e0ff")); film.setColorAt(.3, QColor("#aa96ff")); film.setColorAt(.55, QColor("#ff96d6"));
    film.setColorAt(.8, QColor("#ffd68c")); film.setColorAt(1, QColor("#78e0ff"));
    p.setPen(QPen(QBrush(film), 3)); p.drawEllipse(c, 95.5, 95.5);
    // Six choices around the ring; the last one is armed.
    for (int i = 0; i < 6; ++i) {
        const qreal a = (-90 + i * 60) * M_PI / 180;
        const QPointF t = c + QPointF(62 * std::cos(a), 62 * std::sin(a));
        if (i == 5) {
            p.setBrush(QColor("#e6f6fd")); p.setPen(QPen(rgba(64, 186, 240, .9), 2));
            p.drawRoundedRect(QRectF(t.x() - 21, t.y() - 18, 42, 36), 11, 11);
        } else {
            p.setBrush(rgba(255, 255, 255, .45)); p.setPen(QPen(rgba(255, 255, 255, .9), 1.4));
            p.drawRoundedRect(QRectF(t.x() - 15, t.y() - 13, 30, 26), 8, 8);
        }
    }
    // Hub.
    p.setPen(Qt::NoPen);
    for (int i = 4; i >= 1; --i) { p.setBrush(rgba(42, 30, 92, .04)); p.drawEllipse(c + QPointF(0, 6), 27 + i * 2.5, 27 + i * 2.5); }
    p.setBrush(rgba(255, 255, 255, .92)); p.drawEllipse(c, 27, 27);
    p.setBrush(QColor("#40baf0")); p.drawEllipse(c, 9, 9);
    p.restore();
}

inline QIcon lorenzIcon() {
    QIcon icon = QIcon::fromTheme("lorenz");
    if (!icon.isNull()) return icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size); pm.fill(Qt::transparent);
        QPainter p(&pm); lorenzLogo(p, QRectF(0, 0, size, size)); p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

inline QPixmap lorenzLogoPixmap(int size, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent);
    QPainter p(&pm); lorenzLogo(p, QRectF(0, 0, size, size)); p.end();
    return pm;
}
