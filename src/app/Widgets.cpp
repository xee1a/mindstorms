#include "Widgets.h"
#include "Theme.h"

#include <QEvent>
#include <QFile>
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

BatteryWidget::BatteryWidget(QWidget *parent) : QWidget(parent) { setFixedSize(72, 24); }

void BatteryWidget::setLevel(int percent)
{
    m_percent = std::clamp(percent, 0, 100);
    update();
}

void BatteryWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(theme::Text3, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(1, 2, 62, 20), 5, 5);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Text3);
    p.drawRoundedRect(QRectF(65, 8, 4, 8), 1.5, 1.5);
    p.setBrush(m_percent < 20 ? theme::Bad : theme::Good);
    p.drawRoundedRect(QRectF(5, 6, 54 * m_percent / 100.0, 12), 3, 3);
}

// ── PhotoWidget ───────────────────────────────────────────

PhotoWidget::PhotoWidget(const QString &resource, QSize hint, QWidget *parent) : QWidget(parent), m_hint(hint)
{
    // Transparent cut-outs made by tools/cutout.cpp; nothing is drawn behind them.
    m_photo = QImage(resource).convertToFormat(QImage::Format_ARGB32);
    m_dimmed = m_photo;
    for (int y = 0; y < m_dimmed.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(m_dimmed.scanLine(y));
        for (int x = 0; x < m_dimmed.width(); ++x) {
            const int g = qGray(line[x]);
            line[x] = qRgba(g, g, g, qAlpha(line[x]));
        }
    }
}

void PhotoWidget::setOnline(bool on)
{
    if (on == m_on) return;
    m_on = on;
    update();
}

void PhotoWidget::paintEvent(QPaintEvent *)
{
    if (m_photo.isNull()) return;
    // Downscale with area averaging once per size; drawImage's bilinear filter aliases at 3x.
    const qreal dpr = devicePixelRatioF();
    const QSize px = (QSizeF(m_photo.size()).scaled(QSizeF(size()) * dpr, Qt::KeepAspectRatio)).toSize();
    if (m_scaledFor != px || m_scaledOn != m_on || m_scaled.isNull()) {
        m_scaled = (m_on ? m_photo : m_dimmed).scaled(px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_scaled.setDevicePixelRatio(dpr);
        m_scaledFor = px;
        m_scaledOn = m_on;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QSizeF logical = QSizeF(px) / dpr;
    const QRectF target(QPointF((width() - logical.width()) / 2, (height() - logical.height()) / 2), logical);
    p.setOpacity(m_on ? 1.0 : theme::isLight() ? 0.8 : 0.55);
    p.drawImage(target.topLeft(), m_scaled);
    p.setOpacity(1.0);
    paintOverlay(p, target);
}

// ── PadView ───────────────────────────────────────────────

namespace {

/** Button centres on resources/images/pad.png, as fractions of its width and height. */
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

} // namespace

PadView::PadView(QWidget *parent) : PhotoWidget(QStringLiteral(":/images/pad.png"), {520, 405}, parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(300, 234);
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

void PadView::paintOverlay(QPainter &p, const QRectF &photo)
{
    using drive::PadButton;
    const auto at = [&](qreal x, qreal y) { return QPointF(photo.left() + x * photo.width(), photo.top() + y * photo.height()); };
    for (const Hotspot &h : PadHotspots) {
        const QPointF c = at(h.x, h.y);
        const qreal r = h.r * photo.width();
        if (h.button == PadButton::LS || h.button == PadButton::RS) {
            const bool left = h.button == PadButton::LS;
            const double sx = left ? m_input.leftX : m_input.rightX;
            const double sy = left ? m_input.leftY : m_input.rightY;
            const double mag = std::min(1.0, std::hypot(sx, sy));
            if (m_buttons[int(h.button)]) glow(p, c, r * 0.9, theme::Warm);
            if (mag > 0.08) {
                // Where the stick points: a ring that slides out from the centre.
                const QPointF tip = c + QPointF(sx, -sy) * r * 0.75;
                glow(p, tip, r * 0.42, theme::Accent, 0.4 + 0.6 * mag);
                p.setPen(QPen(Qt::white, std::max(1.5, r * 0.06)));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(tip, r * 0.22, r * 0.22);
            }
            continue;
        }
        if (!m_buttons[int(h.button)]) continue;
        QColor color = theme::Accent;
        if (h.button == PadButton::A) color = QColor(0x4f, 0x7c, 0xff);
        else if (h.button == PadButton::B) color = theme::Bad;
        else if (h.button == PadButton::Y) color = theme::Good;
        else if (h.button == PadButton::X) color = QColor(0x8e, 0x5b, 0xf5);
        glow(p, c, r, color);
    }
    // Triggers sit behind the bumpers; show how far they're pulled.
    for (int side = 0; side < 2; ++side) {
        const double v = side ? m_input.rightTrigger : m_input.leftTrigger;
        if (v > 0.03) glow(p, at(side ? 0.80 : 0.196, 0.105), 0.05 * photo.width(), theme::Warm, v);
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
