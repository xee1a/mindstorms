#pragma once

#include <QColor>

class QApplication;
class QWidget;

namespace theme {

inline const QColor Bg{0x0b, 0x0f, 0x15};
inline const QColor Bg2{0x0f, 0x14, 0x1c};
inline const QColor Surface{0x14, 0x1b, 0x26};
inline const QColor Surface2{0x1a, 0x23, 0x31};
inline const QColor Surface3{0x21, 0x2c, 0x3d};
inline const QColor Line{0x24, 0x2e, 0x3d};
inline const QColor Text{0xe6, 0xed, 0xf6};
inline const QColor Text2{0xa7, 0xb4, 0xc6};
inline const QColor Text3{0x6f, 0x7f, 0x95};
inline const QColor Accent{0x3d, 0x8b, 0xfd};
inline const QColor Accent2{0x12, 0xb5, 0xc9};
inline const QColor Warm{0xff, 0xb0, 0x20};
inline const QColor Good{0x2f, 0xd4, 0x7f};
inline const QColor Bad{0xff, 0x5c, 0x6c};

/** Fusion style, dark palette and the app stylesheet. */
void apply(QApplication &app);
/** Dark native title bar on Windows 10/11. */
void darkTitleBar(QWidget *window);
/** Re-evaluates QSS after a dynamic property change. */
void repolish(QWidget *w);

} // namespace theme
