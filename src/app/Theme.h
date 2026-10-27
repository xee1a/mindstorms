#pragma once

#include <QColor>
#include <QList>
#include <QString>

class QApplication;
class QWidget;

namespace theme {

// Colour tokens. They start as the dark theme; init() swaps in the light one and the accent.
inline QColor Bg{0x0b, 0x0f, 0x15};
inline QColor Bg2{0x0f, 0x14, 0x1c};
inline QColor Header{0x0e, 0x13, 0x1b};
inline QColor Surface{0x14, 0x1b, 0x26};
inline QColor Surface2{0x1a, 0x23, 0x31};
inline QColor Surface3{0x21, 0x2c, 0x3d};
inline QColor Line{0x24, 0x2e, 0x3d};
inline QColor Border{0x2b, 0x36, 0x47};
inline QColor Text{0xe6, 0xed, 0xf6};
inline QColor Text2{0xa7, 0xb4, 0xc6};
inline QColor Text3{0x6f, 0x7f, 0x95};
inline QColor Icon{0xc9, 0xd4, 0xe3};
inline QColor DangerText{0xff, 0x8a, 0x96};
inline QColor Canvas{0x0f, 0x14, 0x1c};
inline QColor CanvasDot{0x24, 0x32, 0x46};
inline QColor PaletteBg{0x18, 0x21, 0x2e};
inline QColor Accent{0x3d, 0x8b, 0xfd};
inline QColor Accent2{0x12, 0xb5, 0xc9};
inline QColor Warm{0xff, 0xb0, 0x20};
inline QColor Good{0x2f, 0xd4, 0x7f};
inline QColor Bad{0xff, 0x5c, 0x6c};

struct AccentChoice {
    QString id;
    QString name;
    QColor swatch;
};
/** Accent colours offered in Settings. */
QList<AccentChoice> accents();

/** Reads the saved theme ("dark", "light", "system") and accent, and sets the colour tokens. Call before building widgets. */
void init();
bool isLight();
/** Fusion style, palette and the app stylesheet for the current tokens. */
void apply(QApplication &app);
/** Native title bar matching the theme on Windows 10/11. */
void styleTitleBar(QWidget *window);
/** Re-evaluates QSS after a dynamic property change. */
void repolish(QWidget *w);

} // namespace theme
