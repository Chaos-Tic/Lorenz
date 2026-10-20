#pragma once
#include <QtWidgets>

// The Liquid Glass look of Lorenz's windows (batch manager, settings,
// diagnostics), matching the wheels: a soft peach, blue and lilac environment
// painted by the window, translucent surfaces with bright rims, capsule
// buttons and crisp text. It follows the system's light or dark scheme;
// LORENZ_THEME=light|dark forces one (documentation captures).
namespace style {

inline bool dark() {
    if (const QString forced = qEnvironmentVariable("LORENZ_THEME"); forced == "dark" || forced == "light") return forced == "dark";
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (const auto scheme = QGuiApplication::styleHints()->colorScheme(); scheme != Qt::ColorScheme::Unknown)
        return scheme == Qt::ColorScheme::Dark;
#endif
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

struct Colors { QColor ink, ink2, faint, ok, failed, busy; };

inline Colors colors() {
    if (dark()) return {QColor("#f3f4f8"), QColor("#bcc2d0"), QColor("#8a90a2"), QColor("#8fe0bf"), QColor("#ff9aa2"), QColor("#ffcf7a")};
    return {QColor("#151820"), QColor("#4a5060"), QColor("#737a8c"), QColor("#1f7a5a"), QColor("#b3261e"), QColor("#9a5d00")};
}

// Three soft colour fields on a pale (or deep) base, painted behind everything.
inline void environment(QPainter &p, const QRectF &r) {
    const bool d = dark();
    QLinearGradient base(r.topLeft(), r.bottomRight());
    if (d) { base.setColorAt(0, QColor("#17182a")); base.setColorAt(.5, QColor("#141a2a")); base.setColorAt(1, QColor("#1d1630")); }
    else { base.setColorAt(0, QColor("#fbefe8")); base.setColorAt(.5, QColor("#eef0fb")); base.setColorAt(1, QColor("#f1e8fa")); }
    p.fillRect(r, base);
    auto blob = [&](QPointF c, qreal radius, const char *color, qreal a) {
        QColor in(color), out(color); in.setAlphaF(a); out.setAlphaF(0);
        QRadialGradient g(c, radius); g.setColorAt(0, in); g.setColorAt(1, out);
        p.fillRect(r, g);
    };
    const qreal s = qMax(r.width(), r.height()), k = d ? .38 : .75;
    blob({r.width() * .08, r.height() * .1}, s * .55, "#ffb48c", k);
    blob({r.width() * .95, r.height() * .3}, s * .5, "#8fd0f6", k);
    blob({r.width() * .45, r.height() * 1.02}, s * .55, "#c3a6f5", k);
}

// A widget that paints the environment: the window's central widget and dialogs.
class Backdrop : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent *) override { QPainter p(this); environment(p, rect()); }
};

inline QString sheet() {
    const bool d = dark();
    const Colors c = colors();
    const QString ink = c.ink.name(), ink2 = c.ink2.name(), faint = c.faint.name();
    const QString glass = d ? "rgba(30,30,52,0.55)" : "rgba(255,255,255,0.55)";
    const QString glassStrong = d ? "rgba(36,36,60,0.78)" : "rgba(255,255,255,0.78)";
    const QString line = d ? "rgba(255,255,255,0.16)" : "rgba(21,24,32,0.12)";
    const QString hover = d ? "rgba(255,255,255,0.16)" : "rgba(255,255,255,0.92)";
    const QString primary = d ? "#9fdcf8" : "#151820", primaryInk = d ? "#10131c" : "#ffffff";
    const QString focus = d ? "#9fdcf8" : "#2b6fd6", accent = d ? "#9fdcf8" : "#40baf0";
    const QString selection = d ? "rgba(159,220,248,0.22)" : "rgba(64,186,240,0.18)";
    const QString chevron = d ? ":/chevron.png" : ":/chevron-dark.png";
    return QString(R"(
        QWidget { color:%1; font-family:"Geist","Segoe UI Variable Text","Segoe UI","Inter","Cantarell","Noto Sans"; }
        QMainWindow, QDialog { background:%13; }
        QFrame#sidebar { background:%4; border:none; border-right:1px solid %6; }
        QLabel { background:transparent; }
        QLabel#brandName { font-size:17px; font-weight:600; letter-spacing:2px; }
        QLabel#hero { font-size:30px; font-weight:600; }
        QLabel#eyebrow { color:%3; font-size:10px; letter-spacing:2px; }
        QLabel#muted { color:%2; font-size:12px; }
        QLabel#local { color:%12; font-size:10px; font-weight:600; }
        QLabel#navActive { background:%5; border:1px solid %6; border-radius:14px; padding:11px 14px; font-weight:600; }
        QFrame#dropZone { background:%4; border:1.5px dashed %12; border-radius:22px; }
        QLabel#dropSymbol { color:%12; font-size:34px; }
        QLabel#dropTitle { font-size:17px; font-weight:600; }
        QPushButton { background:%4; border:1px solid %6; padding:9px 16px; border-radius:17px; }
        QPushButton:hover { background:%7; }
        QPushButton:focus, QComboBox:focus, QTableWidget:focus { border:2px solid %10; }
        QPushButton:disabled { color:%3; }
        QPushButton#primary { color:%9; background:%8; border:none; font-weight:700; padding:11px 26px; border-radius:19px; }
        QPushButton#primary:disabled { color:%3; background:%6; }
        QComboBox { background:%5; border:1px solid %6; padding:8px 12px; border-radius:14px; }
        QComboBox QAbstractItemView { background:%13; border:1px solid %6; selection-background-color:%11; }
        QComboBox::drop-down { border:0; width:30px; }
        QComboBox::down-arrow { image:url(%14); width:12px; height:8px; }
        QCheckBox { spacing:10px; }
        QCheckBox::indicator { width:18px; height:18px; border-radius:6px; border:1px solid %6; background:%5; }
        QCheckBox::indicator:checked { background:%12; border-color:%12; }
        QTableWidget, QTableWidget QWidget#qt_scrollarea_viewport { background:%4; border:1px solid %6; border-radius:16px; selection-background-color:%11; selection-color:%1; }
        QHeaderView { background:transparent; border:0; }
        QHeaderView::section { background:transparent; color:%3; border:0; border-bottom:1px solid %6; padding:10px; font-size:10px; letter-spacing:1px; }
        QTableCornerButton::section { background:transparent; border:0; }
        QProgressBar { border:0; background:%6; border-radius:2px; } QProgressBar::chunk { background:%12; border-radius:2px; }
        QToolTip { color:%1; background:%13; border:1px solid %6; }
        QMenu { background:%13; border:1px solid %6; border-radius:10px; padding:6px; }
        QMenu::item { padding:8px 22px 8px 14px; border-radius:6px; } QMenu::item:selected { background:%11; }
        QScrollBar:vertical { background:transparent; width:8px; margin:2px; }
        QScrollBar::handle:vertical { background:%6; border-radius:3px; min-height:30px; }
        QScrollBar::add-line, QScrollBar::sub-line { height:0; }
    )").arg(ink, ink2, faint, glass, glassStrong, line, hover, primary, primaryInk, focus, selection, accent,
              d ? "#23253a" : "#f6f3fa", chevron);
}

} // namespace style
