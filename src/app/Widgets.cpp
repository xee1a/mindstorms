#include "Widgets.h"
#include "Theme.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QSvgRenderer>
#include <cmath>
#include <algorithm>
#include <vector>

namespace ui {

QLabel *label(const QString &text, const char *role, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    if (role) l->setProperty("role", QString::fromLatin1(role));
    return l;
}

QFrame *card(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setProperty("card", true);
    return f;
}

QFrame *inset(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setProperty("inset", true);
    return f;
}

QIcon icon(const char *name, const QColor &tint)
{
    static QHash<QString, QIcon> cache;
    const QColor color = tint.isValid() ? tint : theme::Icon;
    const QString key = QStringLiteral("%1/%2").arg(QLatin1String(name), color.name());
    if (auto it = cache.constFind(key); it != cache.constEnd()) return *it;
    QSvgRenderer svg(QStringLiteral(":/icons/%1.svg").arg(QLatin1String(name)));
    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        svg.render(&p);
        // Tint: the icons are drawn white.
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), color);
    }
    QIcon ic(pm);
    cache.insert(key, ic);
    return ic;
}

QPushButton *button(const QString &text, const char *kind, const char *iconName, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    if (kind) b->setProperty("kind", QString::fromLatin1(kind));
    if (iconName) {
        const bool onColour = kind && (qstrcmp(kind, "primary") == 0);
        const QColor c = onColour ? Qt::white : (kind && qstrcmp(kind, "danger") == 0 ? theme::DangerText : theme::Icon);
        b->setIcon(icon(iconName, c));
        b->setIconSize(QSize(16, 16));
    }
    return b;
}

// ── ToggleSwitch ──────────────────────────────────────────

ToggleSwitch::ToggleSwitch(const QString &text, QWidget *parent) : QAbstractButton(parent)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const { return {44 + fontMetrics().horizontalAdvance(text()), 24}; }

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track(1, (height() - 20) / 2.0, 34, 20);
    p.setPen(isChecked() ? Qt::NoPen : QPen(theme::Border, 1));
    p.setBrush(isChecked() ? theme::Accent : theme::Surface3);
    p.drawRoundedRect(track, 10, 10);
    const qreal x = isChecked() ? track.right() - 17 : track.left() + 3;
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? Qt::white : theme::Text2);
    p.drawEllipse(QRectF(x, track.top() + 3, 14, 14));
    p.setPen(underMouse() ? theme::Text : theme::Text2);
    p.drawText(QRectF(track.right() + 9, 0, width(), height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}

// ── BatteryWidget ─────────────────────────────────────────

namespace {

int batteryBars(nxt::BatteryLevel level)
{
    switch (level) {
    case nxt::BatteryLevel::Full: return 4;
    case nxt::BatteryLevel::Good: return 3;
    case nxt::BatteryLevel::Half: return 2;
    case nxt::BatteryLevel::Low: return 1;
    case nxt::BatteryLevel::Empty: break;
    }
    return 0;
}

/** Outline, terminal and bars inside `r` (about 2:1). */
void paintBattery(QPainter &p, const QRectF &r, nxt::BatteryLevel level)
{
    const QColor c = BatteryWidget::colour(level);
    const qreal stroke = std::max(1.5, r.height() / 11);
    const QRectF body(r.left() + stroke / 2, r.top() + stroke / 2, r.width() * 0.88 - stroke, r.height() - stroke);
    p.setPen(QPen(level == nxt::BatteryLevel::Empty ? c : theme::Text2, stroke));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(body, r.height() * 0.2, r.height() * 0.2);
    p.setPen(Qt::NoPen);
    p.setBrush(level == nxt::BatteryLevel::Empty ? c : theme::Text2);
    p.drawRoundedRect(QRectF(body.right() + stroke, r.top() + r.height() * 0.3, r.width() * 0.08, r.height() * 0.4), 1.5, 1.5);
    const int bars = batteryBars(level);
    const qreal pad = stroke * 1.4;
    const QRectF inner = body.adjusted(pad, pad, -pad, -pad);
    const qreal gap = std::max(1.0, inner.width() * 0.06);
    const qreal w = (inner.width() - 3 * gap) / 4;
    p.setBrush(c);
    for (int i = 0; i < bars; ++i) p.drawRoundedRect(QRectF(inner.left() + i * (w + gap), inner.top(), w, inner.height()), 1.5, 1.5);
    if (bars == 0) {
        // Empty: an exclamation mark instead of bars.
        p.setPen(QPen(c, stroke, Qt::SolidLine, Qt::RoundCap));
        const qreal x = inner.center().x();
        p.drawLine(QPointF(x, inner.top() + inner.height() * 0.1), QPointF(x, inner.top() + inner.height() * 0.6));
        p.drawPoint(QPointF(x, inner.bottom() - inner.height() * 0.05));
    }
}

} // namespace

BatteryWidget::BatteryWidget(QWidget *parent) : QWidget(parent) { setFixedSize(sizeHint()); }

QColor BatteryWidget::colour(nxt::BatteryLevel level)
{
    switch (level) {
    case nxt::BatteryLevel::Full:
    case nxt::BatteryLevel::Good: return theme::Good;
    case nxt::BatteryLevel::Half: return theme::Warm;
    case nxt::BatteryLevel::Low:
    case nxt::BatteryLevel::Empty: break;
    }
    return theme::Bad;
}

void BatteryWidget::setLevel(nxt::BatteryLevel level)
{
    m_level = level;
    setToolTip(nxt::batteryLabel(level));
    update();
}

QIcon BatteryWidget::icon(nxt::BatteryLevel level)
{
    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    paintBattery(p, QRectF(2, 13, 44, 22), level);
    return QIcon(pm);
}

void BatteryWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    paintBattery(p, QRectF(rect()).adjusted(1, 1, -1, -1), m_level);
}

// ── BrickArt ──────────────────────────────────────────────

BrickArt::BrickArt(QWidget *parent) : QWidget(parent) { setFixedSize(sizeHint()); }

void BrickArt::setState(bool online, const QString &name, std::optional<nxt::BatteryLevel> battery)
{
    if (online == m_online && name == m_name && battery == m_battery) return;
    m_online = online;
    m_name = name;
    m_battery = battery;
    update();
}

void BrickArt::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_online ? 1.0 : 0.6);
    const qreal W = width(), H = height();
    const QRectF body(W * 0.06, H * 0.08, W * 0.88, H * 0.84);

    // Ports: A B C on top, 1-4 underneath.
    QFont small = font();
    small.setPixelSize(int(W * 0.05));
    small.setBold(true);
    p.setFont(small);
    const auto port = [&](qreal cx, qreal y, const QString &label, bool top) {
        const QRectF r(cx - W * 0.07, y, W * 0.14, H * 0.07);
        p.setPen(QPen(theme::Border, 1.5));
        p.setBrush(theme::Surface);
        p.drawRoundedRect(r, 3, 3);
        p.setPen(theme::Text3);
        const QRectF t = top ? QRectF(r.left(), r.bottom() + 2, r.width(), H * 0.06) : QRectF(r.left(), r.top() - H * 0.065, r.width(), H * 0.06);
        p.drawText(t, Qt::AlignCenter, label);
    };
    for (int i = 0; i < 3; ++i) port(W * (0.28 + 0.22 * i), body.top() - H * 0.045, QString(QChar(u'A' + i)), true);
    for (int i = 0; i < 4; ++i) port(W * (0.2 + 0.2 * i), body.bottom() - H * 0.025, QString::number(i + 1), false);

    // Body and the lighter front panel.
    p.setPen(QPen(theme::Border, 2));
    p.setBrush(theme::Surface3);
    p.drawRoundedRect(body, W * 0.09, W * 0.09);
    const QRectF panel = body.adjusted(W * 0.07, H * 0.12, -W * 0.07, -H * 0.12);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Surface2);
    p.drawRoundedRect(panel, W * 0.06, W * 0.06);

    // Screen.
    const QRectF screen(panel.left() + panel.width() * 0.1, panel.top() + panel.height() * 0.07, panel.width() * 0.8, panel.height() * 0.4);
    p.setPen(QPen(theme::Border, 2));
    p.setBrush(theme::Bg2);
    p.drawRoundedRect(screen, 6, 6);
    if (m_online) {
        QFont f = font();
        f.setPixelSize(int(screen.height() * 0.2));
        f.setBold(true);
        p.setFont(f);
        p.setPen(theme::Text);
        p.drawText(screen.adjusted(8, 6, -8, -screen.height() * 0.45), Qt::AlignLeft | Qt::AlignVCenter, m_name);
        if (m_battery) paintBattery(p, QRectF(screen.right() - screen.height() * 0.62, screen.top() + screen.height() * 0.13, screen.height() * 0.5, screen.height() * 0.25), *m_battery);
        p.setPen(QPen(theme::Text3, 1));
        p.drawLine(QPointF(screen.left() + 8, screen.center().y()), QPointF(screen.right() - 8, screen.center().y()));
        f.setBold(false);
        f.setPixelSize(int(screen.height() * 0.16));
        p.setFont(f);
        p.setPen(theme::Text2);
        p.drawText(screen.adjusted(8, screen.height() * 0.52, -8, -6), Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("NXT Studio"));
    }

    // Buttons: orange square, grey arrows either side, dark grey one below.
    const QPointF c(panel.center().x(), panel.top() + panel.height() * 0.68);
    const qreal s = panel.width() * 0.17;
    p.setPen(Qt::NoPen);
    p.setBrush(m_online ? theme::Warm : theme::Text3);
    p.drawRoundedRect(QRectF(c.x() - s / 2, c.y() - s / 2, s, s), 4, 4);
    p.setBrush(theme::Text3);
    for (int side : {-1, 1}) {
        const qreal x = c.x() + side * s * 1.25;
        QPainterPath tri;
        tri.moveTo(x + side * s * 0.45, c.y());
        tri.lineTo(x - side * s * 0.35, c.y() - s * 0.45);
        tri.lineTo(x - side * s * 0.35, c.y() + s * 0.45);
        tri.closeSubpath();
        p.drawPath(tri);
    }
    p.setBrush(theme::Surface);
    p.setPen(QPen(theme::Border, 1.5));
    p.drawRoundedRect(QRectF(c.x() - s * 0.45, c.y() + s * 0.95, s * 0.9, s * 0.5), 3, 3);
}

// ── PadView ───────────────────────────────────────────────

namespace {

// The drawing is laid out on a 520 × 405 grid and scaled to the widget.
constexpr qreal ArtW = 520, ArtH = 405;

struct Hotspot {
    drive::PadButton button;
    qreal x, y, r;
};

constexpr Hotspot PadHotspots[] = {
    {drive::PadButton::Y, 398, 132, 16},    {drive::PadButton::X, 364, 166, 16},     {drive::PadButton::B, 432, 166, 16},
    {drive::PadButton::A, 398, 200, 16},    {drive::PadButton::Up, 122, 138, 13},    {drive::PadButton::Down, 122, 194, 13},
    {drive::PadButton::Left, 94, 166, 13},  {drive::PadButton::Right, 150, 166, 13}, {drive::PadButton::Back, 228, 160, 11},
    {drive::PadButton::Start, 292, 160, 11}, {drive::PadButton::LB, 128, 84, 22},    {drive::PadButton::RB, 392, 84, 22},
    {drive::PadButton::LS, 192, 240, 33},   {drive::PadButton::RS, 328, 240, 33},
};
// Triggers sit above the bumpers; only their tops show behind the body.
constexpr QPointF TriggerCentre[2] = {{128, 56}, {392, 56}};

/** Outline of the controller, one closed shape: the left half is drawn and mirrored. */
QPainterPath padBody()
{
    // Left half from the top centre, anticlockwise to the bottom centre.
    const auto half = [](QPainterPath &p, bool mirror) {
        const auto pt = [mirror](qreal x, qreal y) { return QPointF(mirror ? ArtW - x : x, y); };
        p.moveTo(pt(260, 96));
        p.lineTo(pt(170, 96));
        p.cubicTo(pt(110, 94), pt(66, 104), pt(54, 150));
        p.cubicTo(pt(42, 196), pt(30, 290), pt(48, 336));
        p.cubicTo(pt(62, 372), pt(110, 384), pt(134, 356));
        p.cubicTo(pt(152, 334), pt(166, 300), pt(200, 292));
        p.cubicTo(pt(230, 286), pt(250, 286), pt(260, 286));
    };
    QPainterPath left, right;
    half(left, false);
    half(right, true);
    // Join the halves: down the left side, then back up the right side reversed.
    QPainterPath body = left;
    body.connectPath(right.toReversed());
    body.closeSubpath();
    return body;
}

void glow(QPainter &p, QPointF c, qreal r, QColor color, qreal strength = 1.0)
{
    QRadialGradient g(c, r * 1.9);
    QColor inner = color;
    inner.setAlphaF(float(0.85 * strength));
    QColor mid = color;
    mid.setAlphaF(float(0.35 * strength));
    QColor outer = color;
    outer.setAlpha(0);
    g.setColorAt(0, inner);
    g.setColorAt(0.5, mid);
    g.setColorAt(1, outer);
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(c, r * 1.9, r * 1.9);
}

QColor faceColour(drive::PadButton b)
{
    using drive::PadButton;
    if (b == PadButton::B) return theme::Bad;
    if (b == PadButton::Y) return theme::Good;
    if (b == PadButton::X) return theme::Warm;
    return theme::Accent;
}

} // namespace

PadView::PadView(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(300, 234);
}

void PadView::setOnline(bool on)
{
    if (on == m_on) return;
    m_on = on;
    update();
}

void PadView::setState(const drive::ControlInput &input, const std::array<bool, int(drive::PadButton::Count)> &buttons)
{
    if (buttons == m_buttons && input.leftX == m_input.leftX && input.leftY == m_input.leftY && input.rightX == m_input.rightX &&
        input.rightY == m_input.rightY && input.leftTrigger == m_input.leftTrigger && input.rightTrigger == m_input.rightTrigger)
        return;
    m_input = input;
    m_buttons = buttons;
    update();
}

void PadView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QSizeF art = QSizeF(ArtW, ArtH).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    p.translate((width() - art.width()) / 2, (height() - art.height()) / 2);
    p.scale(art.width() / ArtW, art.height() / ArtH);
    const QRectF r(0, 0, ArtW, ArtH);
    p.setOpacity(m_on ? 1.0 : 0.55);
    paintPad(p, r);
    paintHighlights(p, r);
}

void PadView::paintPad(QPainter &p, const QRectF &) const
{
    using drive::PadButton;
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(13);
    p.setFont(f);

    // Triggers and bumpers peek out from behind the top edge.
    for (int side = 0; side < 2; ++side) {
        const QPointF t = TriggerCentre[side];
        p.setPen(QPen(theme::Border, 1.5));
        p.setBrush(theme::Surface);
        p.drawRoundedRect(QRectF(t.x() - 40, t.y() - 16, 80, 40), 12, 12);
        p.setPen(theme::Text3);
        p.drawText(QRectF(t.x() - 40, t.y() - 15, 80, 18), Qt::AlignCenter, side ? QStringLiteral("R2") : QStringLiteral("L2"));
        const Hotspot &b = PadHotspots[side ? 11 : 10];
        p.setPen(QPen(theme::Border, 1.5));
        p.setBrush(theme::Surface2);
        p.drawRoundedRect(QRectF(b.x - 56, b.y - 14, 112, 36), 14, 14);
        p.setPen(theme::Text3);
        p.drawText(QRectF(b.x - 56, b.y - 13, 112, 20), Qt::AlignCenter, side ? QStringLiteral("R1") : QStringLiteral("L1"));
    }

    QLinearGradient shade(0, 96, 0, 380);
    shade.setColorAt(0, theme::Surface3);
    shade.setColorAt(1, theme::Surface2);
    p.setPen(QPen(theme::Border, 2));
    p.setBrush(shade);
    p.drawPath(padBody());

    // D-pad: one plus-shaped key in a shallow round well.
    const QPointF d(122, 166);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Surface2);
    p.drawEllipse(d, 46, 46);
    QPainterPath cross;
    cross.addRoundedRect(QRectF(d.x() - 40, d.y() - 13, 80, 26), 5, 5);
    cross.addRoundedRect(QRectF(d.x() - 13, d.y() - 40, 26, 80), 5, 5);
    p.setPen(QPen(theme::Border, 1.5));
    p.setBrush(theme::Surface);
    p.drawPath(cross.simplified());
    p.setPen(QPen(theme::Border, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(d, 7, 7);

    // Face buttons sit in a shallow well too.
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Surface2);
    p.drawEllipse(QPointF(398, 166), 58, 58);

    for (const Hotspot &h : PadHotspots) {
        const QPointF c(h.x, h.y);
        switch (h.button) {
        case PadButton::A:
        case PadButton::B:
        case PadButton::X:
        case PadButton::Y: {
            p.setPen(QPen(faceColour(h.button), 2));
            p.setBrush(theme::Surface);
            p.drawEllipse(c, h.r, h.r);
            p.setPen(faceColour(h.button));
            const char *names[] = {"A", "B", "X", "Y"};
            p.drawText(QRectF(c.x() - h.r, c.y() - h.r, h.r * 2, h.r * 2), Qt::AlignCenter, QLatin1String(names[int(h.button)]));
            break;
        }
        case PadButton::Back:
        case PadButton::Start:
            p.setPen(QPen(theme::Border, 1.5));
            p.setBrush(theme::Surface);
            p.drawRoundedRect(QRectF(c.x() - 15, c.y() - 6, 30, 12), 6, 6);
            break;
        case PadButton::LS:
        case PadButton::RS:
            p.setPen(Qt::NoPen);
            p.setBrush(theme::Surface2);
            p.drawEllipse(c, h.r + 6, h.r + 6);
            p.setPen(QPen(theme::Border, 2));
            p.setBrush(theme::Bg2);
            p.drawEllipse(c, h.r, h.r);
            break;
        default: break; // D-pad and shoulders are drawn above
        }
    }
    QFont small = f;
    small.setPixelSize(9);
    p.setFont(small);
    p.setPen(theme::Text3);
    p.drawText(QRectF(203, 168, 50, 14), Qt::AlignCenter, QStringLiteral("SELECT"));
    p.drawText(QRectF(267, 168, 50, 14), Qt::AlignCenter, QStringLiteral("START"));
}

void PadView::paintHighlights(QPainter &p, const QRectF &) const
{
    using drive::PadButton;
    for (const Hotspot &h : PadHotspots) {
        const QPointF c(h.x, h.y);
        if (h.button == PadButton::LS || h.button == PadButton::RS) {
            const bool left = h.button == PadButton::LS;
            // Offline, the caps just sit centred.
            const double sx = !m_on ? 0 : left ? m_input.leftX : m_input.rightX;
            const double sy = !m_on ? 0 : left ? m_input.leftY : m_input.rightY;
            const double mag = std::min(1.0, std::hypot(sx, sy));
            if (m_on && m_buttons[int(h.button)]) glow(p, c, h.r * 0.9, theme::Warm);
            // The cap follows the stick.
            const QPointF tip = c + QPointF(sx, -sy) * h.r * 0.4;
            if (mag > 0.08) glow(p, tip, h.r * 0.45, theme::Accent, 0.4 + 0.6 * mag);
            p.setPen(QPen(mag > 0.08 ? theme::Accent : theme::Border, 2));
            p.setBrush(theme::Surface3);
            p.drawEllipse(tip, h.r * 0.6, h.r * 0.6);
            p.setPen(QPen(theme::Border, 1));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(tip, h.r * 0.42, h.r * 0.42);
            continue;
        }
        if (!m_on || !m_buttons[int(h.button)]) continue;
        const bool face = h.button == PadButton::A || h.button == PadButton::B || h.button == PadButton::X || h.button == PadButton::Y;
        glow(p, c, h.r, face ? faceColour(h.button) : theme::Accent);
    }
    for (int side = 0; side < 2; ++side) {
        const double v = side ? m_input.rightTrigger : m_input.leftTrigger;
        if (m_on && v > 0.03) glow(p, TriggerCentre[side], 22, theme::Warm, v);
    }
}

// ── Toast ─────────────────────────────────────────────────

Toast::Toast(QWidget *parent) : QLabel(parent)
{
    setObjectName(QStringLiteral("toast"));
    setWordWrap(true);
    setMaximumWidth(560);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
    parent->installEventFilter(this);
    m_hide.setSingleShot(true);
    connect(&m_hide, &QTimer::timeout, this, [this] {
        auto *fx = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(fx);
        auto *a = new QPropertyAnimation(fx, "opacity", this);
        a->setDuration(220);
        a->setStartValue(1.0);
        a->setEndValue(0.0);
        connect(a, &QPropertyAnimation::finished, this, &QWidget::hide);
        a->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void Toast::showMessage(const QString &message, const char *kind)
{
    setGraphicsEffect(nullptr);
    setText(message);
    setProperty("kind", QString::fromLatin1(kind));
    theme::repolish(this);
    adjustSize();
    reposition();
    raise();
    QLabel::show();
    m_hide.start(qstrcmp(kind, "error") == 0 ? 6000 : 2800);
}

bool Toast::eventFilter(QObject *o, QEvent *e)
{
    if (o == parentWidget() && e->type() == QEvent::Resize) reposition();
    return QLabel::eventFilter(o, e);
}

void Toast::reposition()
{
    if (!parentWidget()) return;
    const QSize s = sizeHint().boundedTo(QSize(560, 200));
    resize(s);
    move((parentWidget()->width() - s.width()) / 2, parentWidget()->height() - s.height() - 24);
}

} // namespace ui
