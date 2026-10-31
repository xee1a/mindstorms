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

/** Button centres as fractions of the drawing's width and height. */
struct Hotspot {
    drive::PadButton button;
    qreal x, y, r; // r is a fraction of the width
};

constexpr Hotspot PadHotspots[] = {
    {drive::PadButton::Y, 0.782, 0.294, 0.034},    {drive::PadButton::X, 0.704, 0.385, 0.034},
    {drive::PadButton::B, 0.854, 0.385, 0.034},    {drive::PadButton::A, 0.780, 0.483, 0.034},
    {drive::PadButton::Up, 0.222, 0.318, 0.028},   {drive::PadButton::Down, 0.222, 0.450, 0.028},
    {drive::PadButton::Left, 0.170, 0.383, 0.028}, {drive::PadButton::Right, 0.276, 0.383, 0.028},
    {drive::PadButton::Back, 0.501, 0.411, 0.026}, {drive::PadButton::Start, 0.501, 0.473, 0.026},
    {drive::PadButton::LB, 0.229, 0.150, 0.050},   {drive::PadButton::RB, 0.767, 0.150, 0.050},
    {drive::PadButton::LS, 0.361, 0.594, 0.066},   {drive::PadButton::RS, 0.638, 0.594, 0.066},
};

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
    const QSizeF art = QSizeF(sizeHint()).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    const QRectF r(QPointF((width() - art.width()) / 2, (height() - art.height()) / 2), art);
    p.setOpacity(m_on ? 1.0 : 0.55);
    paintPad(p, r);
    p.setOpacity(1.0);
    if (m_on) paintHighlights(p, r);
}

void PadView::paintPad(QPainter &p, const QRectF &r) const
{
    using drive::PadButton;
    const qreal W = r.width(), H = r.height();
    const auto at = [&](qreal x, qreal y) { return QPointF(r.left() + x * W, r.top() + y * H); };
    const auto box = [&](qreal x, qreal y, qreal w, qreal h) { return QRectF(r.left() + x * W, r.top() + y * H, w * W, h * H); };

    // Triggers behind the shoulder buttons, then the bumpers.
    for (qreal x : {0.229, 0.767}) {
        p.setPen(QPen(theme::Border, 1.5));
        p.setBrush(theme::Surface);
        p.drawRoundedRect(box(x - 0.07, 0.07, 0.14, 0.08), H * 0.03, H * 0.03);
        p.setBrush(theme::Surface2);
        p.drawRoundedRect(box(x - 0.09, 0.115, 0.18, 0.075), H * 0.035, H * 0.035);
    }

    // Body: a wide rounded bar with two grips.
    QPainterPath body;
    body.addRoundedRect(box(0.1, 0.17, 0.8, 0.43), H * 0.16, H * 0.16);
    for (qreal x : {0.235, 0.765}) {
        QPainterPath grip;
        grip.addEllipse(at(x, 0.62), W * 0.15, H * 0.27);
        body = body.united(grip);
    }
    p.setPen(QPen(theme::Border, 2.5));
    p.setBrush(theme::Surface3);
    p.drawPath(body.simplified());

    // D-pad.
    const QPointF d = at(0.222, 0.384);
    const qreal arm = W * 0.088, thick = W * 0.05;
    QPainterPath cross;
    cross.addRoundedRect(QRectF(d.x() - arm, d.y() - thick / 2, arm * 2, thick), 4, 4);
    cross.addRoundedRect(QRectF(d.x() - thick / 2, d.y() - arm, thick, arm * 2), 4, 4);
    p.setPen(QPen(theme::Border, 1.5));
    p.setBrush(theme::Surface);
    p.drawPath(cross.simplified());

    QFont f = font();
    f.setBold(true);
    f.setPixelSize(std::max(8, int(W * 0.028)));
    p.setFont(f);
    for (const Hotspot &h : PadHotspots) {
        const QPointF c = at(h.x, h.y);
        const qreal rad = h.r * W;
        switch (h.button) {
        case PadButton::A:
        case PadButton::B:
        case PadButton::X:
        case PadButton::Y: {
            p.setPen(QPen(faceColour(h.button), 2));
            p.setBrush(theme::Surface);
            p.drawEllipse(c, rad, rad);
            p.setPen(theme::Text2);
            const char *names[] = {"A", "B", "X", "Y"};
            p.drawText(QRectF(c.x() - rad, c.y() - rad, rad * 2, rad * 2), Qt::AlignCenter, QLatin1String(names[int(h.button)]));
            break;
        }
        case PadButton::Back:
        case PadButton::Start:
            p.setPen(QPen(theme::Border, 1.5));
            p.setBrush(theme::Surface);
            p.drawRoundedRect(QRectF(c.x() - rad * 1.5, c.y() - rad * 0.55, rad * 3, rad * 1.1), rad * 0.55, rad * 0.55);
            break;
        case PadButton::LS:
        case PadButton::RS:
            p.setPen(QPen(theme::Border, 2));
            p.setBrush(theme::Surface2);
            p.drawEllipse(c, rad, rad);
            p.setBrush(theme::Surface);
            p.drawEllipse(c, rad * 0.68, rad * 0.68);
            break;
        case PadButton::LB:
        case PadButton::RB: {
            p.setPen(theme::Text3);
            p.drawText(box(h.x - 0.09, 0.115, 0.18, 0.075), Qt::AlignCenter, h.button == PadButton::LB ? QStringLiteral("L1") : QStringLiteral("R1"));
            break;
        }
        default: break; // the D-pad is drawn above
        }
    }
    p.setPen(theme::Text3);
    p.drawText(box(0.159, 0.07, 0.14, 0.08), Qt::AlignCenter, QStringLiteral("L2"));
    p.drawText(box(0.697, 0.07, 0.14, 0.08), Qt::AlignCenter, QStringLiteral("R2"));
}

void PadView::paintHighlights(QPainter &p, const QRectF &r) const
{
    using drive::PadButton;
    const auto at = [&](qreal x, qreal y) { return QPointF(r.left() + x * r.width(), r.top() + y * r.height()); };
    for (const Hotspot &h : PadHotspots) {
        const QPointF c = at(h.x, h.y);
        const qreal rad = h.r * r.width();
        if (h.button == PadButton::LS || h.button == PadButton::RS) {
            const bool left = h.button == PadButton::LS;
            const double sx = left ? m_input.leftX : m_input.rightX;
            const double sy = left ? m_input.leftY : m_input.rightY;
            const double mag = std::min(1.0, std::hypot(sx, sy));
            if (m_buttons[int(h.button)]) glow(p, c, rad * 0.9, theme::Warm);
            // The cap follows the stick.
            const QPointF tip = c + QPointF(sx, -sy) * rad * 0.45;
            if (mag > 0.08) glow(p, tip, rad * 0.42, theme::Accent, 0.4 + 0.6 * mag);
            p.setPen(QPen(mag > 0.08 ? theme::Accent : theme::Border, 2));
            p.setBrush(theme::Surface);
            p.drawEllipse(tip, rad * 0.55, rad * 0.55);
            continue;
        }
        if (!m_buttons[int(h.button)]) continue;
        const bool face = h.button == PadButton::A || h.button == PadButton::B || h.button == PadButton::X || h.button == PadButton::Y;
        glow(p, c, rad, face ? faceColour(h.button) : theme::Accent);
    }
    for (int side = 0; side < 2; ++side) {
        const double v = side ? m_input.rightTrigger : m_input.leftTrigger;
        if (v > 0.03) glow(p, at(side ? 0.767 : 0.229, 0.11), 0.05 * r.width(), theme::Warm, v);
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
