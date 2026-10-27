#include "Theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHash>
#include <QSettings>
#include <QStyleHints>
#include <cmath>
#include <iterator>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace theme {

namespace {

const char *styleSheet = R"QSS(
* { outline: 0; }
QWidget { color: #e6edf6; font-family: "Segoe UI Variable Text", "Segoe UI"; font-size: 13px; }
QMainWindow, #central { background: #0b0f15; }
QToolTip { background: #212c3d; color: #e6edf6; border: 1px solid #33425a; border-radius: 6px; padding: 6px 8px; }

/* Header */
#header { background: #0e131b; border-bottom: 1px solid #1d2633; }
#brandName { font-family: "Segoe UI Variable Display", "Segoe UI"; font-size: 15px; font-weight: 700; }
#tabs { background: #141b26; border: 1px solid #222c3a; border-radius: 11px; }
QPushButton#tab { background: transparent; border: none; border-radius: 8px; padding: 6px 16px; color: #a7b4c6; font-weight: 600; }
QPushButton#tab:hover { background: #1a2331; color: #e6edf6; }
QPushButton#tab:checked { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2a3a52, stop:1 #223047); color: #ffffff; }
#chip { background: #141b26; border: 1px solid #222c3a; border-radius: 13px; padding: 3px 12px; color: #a7b4c6; font-size: 12px; font-weight: 600; }
#chip[ok="true"] { color: #e6edf6; border-color: #275c45; }
QPushButton#chip:hover { border-color: #33425a; color: #e6edf6; }
QToolButton#settingsButton { background: transparent; border: none; border-radius: 8px; padding: 5px; }
QToolButton#settingsButton:hover { background: #1a2331; }
QToolButton#settingsButton::menu-indicator { image: none; }

/* Cards */
QFrame[card="true"] { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #151c27, stop:1 #121822); border: 1px solid #1f2836; border-radius: 14px; }
QFrame[inset="true"] { background: #0f141c; border: 1px solid #1d2633; border-radius: 12px; }
QLabel { background: transparent; }
QLabel[role="h1"] { font-family: "Segoe UI Variable Display", "Segoe UI"; font-size: 22px; font-weight: 700; }
QLabel[role="h2"] { font-family: "Segoe UI Variable Display", "Segoe UI"; font-size: 15px; font-weight: 700; }
QLabel[role="muted"] { color: #a7b4c6; }
QLabel[role="faint"] { color: #6f7f95; font-size: 12px; }
QLabel[role="value"] { font-family: "Segoe UI Variable Display", "Segoe UI"; font-size: 20px; font-weight: 600; }
QLabel[role="mono"] { font-family: "Cascadia Mono", Consolas; font-size: 13px; font-weight: 600; }
QLabel[role="caps"] { color: #6f7f95; font-size: 11px; font-weight: 700; }
QLabel[role="warn"] { background: #2a2414; border: 1px solid #5c4a1d; border-radius: 10px; color: #ffd27a; padding: 10px 12px; }
QLabel[role="step"] { background: #1a2c4a; color: #8bbcff; border-radius: 12px; font-weight: 700; min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px; qproperty-alignment: AlignCenter; }
QLabel[role="badge"] { background: #212c3d; border-radius: 6px; font-weight: 700; padding: 2px 7px; }

/* Buttons */
QPushButton { background: #1a2331; border: 1px solid #2b3647; border-radius: 9px; padding: 7px 14px; font-weight: 600; }
QPushButton:hover { background: #212c3d; }
QPushButton:pressed { background: #18202c; }
QPushButton:disabled { color: #5b6878; background: #151b25; border-color: #1f2836; }
QPushButton[kind="ghost"] { background: transparent; border-color: transparent; color: #a7b4c6; }
QPushButton[kind="ghost"]:hover { background: #1a2331; color: #e6edf6; }
QPushButton[kind="primary"] { border: none; color: white; background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4a96ff, stop:1 #2f78ea); }
QPushButton[kind="primary"]:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5aa0ff, stop:1 #3a82f2); }
QPushButton[kind="primary"]:disabled { background: #22324a; color: #7f93ad; }
QPushButton[kind="danger"] { background: #2a1820; border: 1px solid #5c2a33; color: #ff8a96; }
QPushButton[kind="danger"]:hover { background: #3a1d27; }
QPushButton#armButton { text-align: left; padding: 12px 16px; border-radius: 12px; font-size: 14px; }
QPushButton#armButton:checked { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #173827, stop:1 #12261d); border: 1px solid #2fd47f; color: #ffffff; }
QPushButton#estop { border-radius: 12px; padding: 12px 20px; font-size: 14px; }
QPushButton#makeVariable { background: #2b3647; border: 1px solid #3a4a60; border-radius: 8px; padding: 6px 14px; }

/* Segmented */
#segmented { background: #0f141c; border: 1px solid #1d2633; border-radius: 11px; }
#segmented QPushButton { background: transparent; border: none; border-radius: 8px; padding: 6px 10px; color: #a7b4c6; }
#segmented QPushButton:checked { background: #212c3d; color: #ffffff; }

/* Inputs */
QComboBox { background: #1a2331; border: 1px solid #2b3647; border-radius: 9px; padding: 6px 10px; min-height: 20px; font-weight: 600; }
QComboBox:hover { border-color: #3a4a60; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(:/icons/chevron.svg); width: 12px; height: 12px; }
QComboBox QAbstractItemView { background: #1a2331; border: 1px solid #2b3647; border-radius: 8px; padding: 4px; selection-background-color: #2a3a52; }
QComboBox[compact="true"] { padding: 3px 8px; font-size: 12px; }
QLineEdit { background: #141b26; border: 1px solid #2b3647; border-radius: 8px; padding: 6px 8px; selection-background-color: #3d8bfd; }
QLineEdit:focus { border-color: #3d8bfd; }
QLineEdit#projectName { background: transparent; border: 1px solid transparent; font-size: 15px; font-weight: 700; padding: 4px 8px; }
QLineEdit#projectName:hover { border-color: #1f2836; }
QLineEdit#projectName:focus { border-color: #3d8bfd; background: #141b26; }
QLineEdit#fieldEditor { background: white; color: #1f2633; border: 2px solid #ffd166; border-radius: 12px; padding: 0 6px; }
QSlider::groove:horizontal { height: 5px; background: #212c3d; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #3d8bfd; border-radius: 2px; }
QSlider::handle:horizontal { background: white; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; border: 3px solid #3d6fc0; }
QProgressBar { background: #212c3d; border: none; border-radius: 2px; max-height: 5px; }
QProgressBar::chunk { border-radius: 2px; background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #12b5c9, stop:1 #3d8bfd); }

/* Menus */
QMenu { background: #1a2331; border: 1px solid #2b3647; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 26px 7px 12px; border-radius: 6px; }
QMenu::item:selected { background: #2a3a52; }
QMenu::separator { height: 1px; background: #2b3647; margin: 5px 8px; }
QMenu::indicator { width: 14px; height: 14px; left: 6px; }

/* Scrollbars */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #2b3647; border-radius: 3px; min-height: 30px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #2b3647; border-radius: 3px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }

/* Program view */
#toolbar { background: #0f141c; border-bottom: 1px solid #1d2633; }
#categoryList { background: #131a24; border-right: 1px solid #1d2633; }
#categoryList QToolButton { background: transparent; border: none; border-radius: 10px; color: #a7b4c6; font-size: 11px; font-weight: 600; padding: 4px 0; }
#categoryList QToolButton:hover { background: #1a2331; color: #e6edf6; }
#categoryList QToolButton:checked { background: #212c3d; color: #ffffff; }
#paletteView { border-right: 1px solid #1d2633; }
#zoomBar { background: #1a2331; border: 1px solid #2b3647; border-radius: 10px; }
#zoomBar QToolButton { background: transparent; border: none; border-radius: 7px; min-width: 28px; min-height: 28px; font-size: 16px; font-weight: 700; color: #c9d4e3; }
#zoomBar QToolButton:hover { background: #2a3a52; }

/* Drive view */
QLabel[role="key"] { background: #1a2331; border: 1px solid #2b3647; border-radius: 8px; padding: 4px 9px; min-width: 30px; font-size: 12px; font-weight: 700; color: #c9d4e3; }
QLabel[role="key"][active="true"] { background: #3d8bfd; border-color: #8bbcff; color: white; }
QLabel[role="pill"] { background: #141b26; border: 1px solid #222c3a; border-radius: 12px; padding: 4px 12px; color: #a7b4c6; font-size: 12px; font-weight: 600; }
QLabel[role="pill"][ok="true"] { color: #e6edf6; border-color: #275c45; }

/* Settings */
QDialog#settings { background: #0b0f15; }
QListWidget#settingsNav { background: transparent; border: none; outline: 0; }
QListWidget#settingsNav::item { padding: 0 12px; border-radius: 9px; color: #a7b4c6; font-weight: 600; }
QListWidget#settingsNav::item:hover { background: #1a2331; color: #e6edf6; }
QListWidget#settingsNav::item:selected { background: #212c3d; color: #ffffff; }
#restartBanner { background: #1a2c4a; border: 1px solid #3d6fc0; border-radius: 12px; }

/* Toast */
#toast { background: #212c3d; border: 1px solid #33425a; border-radius: 12px; padding: 11px 16px; font-weight: 600; }
#toast[kind="error"] { background: #2a1a20; border-color: #6b2f3a; }
#toast[kind="success"] { background: #15271f; border-color: #2a6b4a; }
)QSS";

/** Dark-theme colours in the stylesheet and their light-theme counterparts. */
const std::pair<const char *, const char *> LightColours[] = {
    {"#e6edf6", "#1b2430"}, {"#0b0f15", "#eef1f5"}, {"#212c3d", "#e3e8ef"}, {"#33425a", "#c3ccd8"},
    {"#0e131b", "#ffffff"}, {"#1d2633", "#dde3ea"}, {"#141b26", "#ffffff"}, {"#222c3a", "#d9e0e8"},
    {"#a7b4c6", "#4d5b6e"}, {"#1a2331", "#eef2f7"}, {"#2a3a52", "#dbe7fb"}, {"#223047", "#cfdff8"},
    {"#ffffff", "#0f1722"}, {"#275c45", "#8fd1ae"}, {"#151c27", "#ffffff"}, {"#121822", "#fbfcfe"},
    {"#1f2836", "#dfe5ec"}, {"#0f141c", "#f4f6f9"}, {"#6f7f95", "#7a879a"}, {"#2a2414", "#fff6e0"},
    {"#5c4a1d", "#f0d48a"}, {"#ffd27a", "#7a5200"}, {"#1a2c4a", "#dce9ff"}, {"#8bbcff", "#1f5fc4"},
    {"#2b3647", "#d3dae4"}, {"#18202c", "#e4e9f0"}, {"#5b6878", "#9aa5b4"}, {"#151b25", "#f1f3f6"},
    {"#2a1820", "#ffe9ec"}, {"#5c2a33", "#f3b4bd"}, {"#ff8a96", "#c62f42"}, {"#3a1d27", "#ffdbe0"},
    {"#173827", "#e3f7ec"}, {"#12261d", "#d5f2e2"}, {"#3a4a60", "#b9c4d2"}, {"#c9d4e3", "#3c4a5e"},
    {"#7f93ad", "#6d7f97"}, {"#131a24", "#f7f9fb"}, {"#2a1a20", "#ffe9ec"}, {"#6b2f3a", "#f3b4bd"},
    {"#15271f", "#e3f7ec"}, {"#2a6b4a", "#8fd1ae"}, {"#22324a", "#c9d8ee"},
};

/** Colours in the blue accent family; the accent setting rotates their hue. */
const char *const AccentFamily[] = {"#3d8bfd", "#4a96ff", "#2f78ea", "#5aa0ff", "#3a82f2", "#3d6fc0", "#8bbcff", "#1a2c4a",
                                    "#2a3a52", "#223047", "#dbe7fb", "#cfdff8", "#dce9ff", "#1f5fc4", "#c9d8ee", "#22324a"};

bool g_light = false;
int g_hueShift = 0;

QColor shifted(QColor c)
{
    if (g_hueShift == 0) return c;
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);
    if (h < 0) return c;
    return QColor::fromHslF(std::fmod(h + g_hueShift / 360.0f + 1.0f, 1.0f), s, l, a);
}

QString themedStyleSheet()
{
    QString qss = QString::fromUtf8(styleSheet);
    if (g_light) {
        // Two passes through placeholders so a replacement is never replaced again.
        for (size_t i = 0; i < std::size(LightColours); ++i)
            qss.replace(QLatin1String(LightColours[i].first), QStringLiteral("@%1@").arg(i), Qt::CaseInsensitive);
        for (size_t i = 0; i < std::size(LightColours); ++i)
            qss.replace(QStringLiteral("@%1@").arg(i), QLatin1String(LightColours[i].second));
        qss.replace(QStringLiteral(":/icons/chevron.svg"), QStringLiteral(":/icons/chevron-dark.svg"));
    }
    if (g_hueShift)
        for (const char *hex : AccentFamily) qss.replace(QLatin1String(hex), shifted(QColor(QLatin1String(hex))).name(), Qt::CaseInsensitive);
    return qss;
}

} // namespace

QList<AccentChoice> accents()
{
    return {{QStringLiteral("blue"), QApplication::translate("Theme", "Blue"), QColor(0x3d, 0x8b, 0xfd)},
            {QStringLiteral("green"), QApplication::translate("Theme", "Green"), QColor(0x22, 0xb8, 0x6b)},
            {QStringLiteral("orange"), QApplication::translate("Theme", "Orange"), QColor(0xf5, 0x84, 0x1f)},
            {QStringLiteral("purple"), QApplication::translate("Theme", "Purple"), QColor(0x8e, 0x5b, 0xf5)},
            {QStringLiteral("pink"), QApplication::translate("Theme", "Pink"), QColor(0xe8, 0x4a, 0x9a)}};
}

void init()
{
    QSettings s;
    const QString mode = s.value(QStringLiteral("appearance/theme"), QStringLiteral("dark")).toString();
    g_light = mode == QLatin1String("light") ||
              (mode == QLatin1String("system") && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light);
    const QString accent = s.value(QStringLiteral("appearance/accent"), QStringLiteral("blue")).toString();
    const QHash<QString, int> hues{{QStringLiteral("green"), -70}, {QStringLiteral("orange"), -188},
                                   {QStringLiteral("purple"), 46}, {QStringLiteral("pink"), 112}};
    g_hueShift = hues.value(accent, 0);

    if (g_light) {
        Bg = QColor(0xee, 0xf1, 0xf5);
        Bg2 = QColor(0xf4, 0xf6, 0xf9);
        Header = Qt::white;
        Surface = Qt::white;
        Surface2 = QColor(0xee, 0xf2, 0xf7);
        Surface3 = QColor(0xe3, 0xe8, 0xef);
        Line = QColor(0xdd, 0xe3, 0xea);
        Border = QColor(0xd3, 0xda, 0xe4);
        Text = QColor(0x1b, 0x24, 0x30);
        Text2 = QColor(0x4d, 0x5b, 0x6e);
        Text3 = QColor(0x7a, 0x87, 0x9a);
        Icon = QColor(0x3c, 0x4a, 0x5e);
        DangerText = QColor(0xc6, 0x2f, 0x42);
        Canvas = QColor(0xf4, 0xf6, 0xf9);
        CanvasDot = QColor(0xd2, 0xd9, 0xe3);
        PaletteBg = QColor(0xee, 0xf2, 0xf6);
        Warm = QColor(0xe8, 0x95, 0x00);
        Good = QColor(0x1f, 0xa8, 0x62);
        Bad = QColor(0xe5, 0x3e, 0x52);
    }
    Accent = shifted(Accent);
}

bool isLight() { return g_light; }

void apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    p.setColor(QPalette::Window, Bg);
    p.setColor(QPalette::WindowText, Text);
    p.setColor(QPalette::Base, Surface);
    p.setColor(QPalette::AlternateBase, Surface2);
    p.setColor(QPalette::Text, Text);
    p.setColor(QPalette::Button, Surface2);
    p.setColor(QPalette::ButtonText, Text);
    p.setColor(QPalette::Highlight, Accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, Surface3);
    p.setColor(QPalette::ToolTipText, Text);
    p.setColor(QPalette::PlaceholderText, Text3);
    p.setColor(QPalette::Disabled, QPalette::Text, Text3);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, Text3);
    app.setPalette(p);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(13);
    app.setFont(f);
    app.setStyleSheet(themedStyleSheet());
}

void styleTitleBar(QWidget *window)
{
#ifdef Q_OS_WIN
    auto hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = g_light ? FALSE : TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    // Windows 11: tint the caption to match the header bar.
    const COLORREF caption = RGB(Header.red(), Header.green(), Header.blue());
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
#else
    Q_UNUSED(window);
#endif
}

void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

} // namespace theme
