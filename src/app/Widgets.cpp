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

QIcon icon(const char *name, const QColor &color)
{
    static QHash<QString, QIcon> cache;
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
        const QColor c = onColour ? Qt::white : (kind && qstrcmp(kind, "danger") == 0 ? QColor(0xff, 0x8a, 0x96) : QColor(0xc9, 0xd4, 0xe3));
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
    p.setPen(isChecked() ? Qt::NoPen : QPen(QColor(0x2b, 0x36, 0x47), 1));
    p.setBrush(isChecked() ? theme::Accent : theme::Surface3);
    p.drawRoundedRect(track, 10, 10);
    const qreal x = isChecked() ? track.right() - 17 : track.left() + 3;
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? Qt::white : theme::Text2);
    p.drawEllipse(QRectF(x, track.top() + 3, 14, 14));
    p.setPen(underMouse() ? theme::Text : theme::Text2);
    p.drawText(QRectF(track.right() + 9, 0, width(), height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}

// ── MotorGauge ────────────────────────────────────────────

MotorGauge::MotorGauge(QWidget *parent) : QWidget(parent) { setMinimumSize(110, 100); }

void MotorGauge::setPower(int power)
{
    if (power == m_power) return;
    m_power = power;
    update();
}

void MotorGauge::setCaption(const QString &caption)
{
    m_caption = caption;
    update();
}

void MotorGauge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal side = std::min<qreal>(std::min(width(), height()) - 16.0, 150);
    const QRectF r((width() - side) / 2, (height() - side) / 2 + 6, side, side);

    QPen track(theme::Surface3, 9, Qt::SolidLine, Qt::RoundCap);
    p.setPen(track);
    p.drawArc(r, 225 * 16, -270 * 16);

    if (m_power != 0) {
        const QColor c = m_power > 0 ? theme::Accent : theme::Warm;
        QColor glow = c;
        glow.setAlpha(60);
        const int span = int(-m_power / 100.0 * 135 * 16);
        p.setPen(QPen(glow, 15, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 90 * 16, span);
        p.setPen(QPen(c, 9, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 90 * 16, span);
    }

    QFont big(QStringLiteral("Segoe UI Variable Display"));
    big.setPixelSize(int(side * 0.24));
    big.setWeight(QFont::DemiBold);
    p.setFont(big);
    p.setPen(theme::Text);
    const QFontMetricsF fm(big);
    const QRectF number(r.left(), r.center().y() - fm.height() * 0.62, r.width(), fm.height());
    p.drawText(number, Qt::AlignCenter, QString::number(m_power));
    QFont small(QStringLiteral("Segoe UI"));
    small.setPixelSize(10);
    small.setWeight(QFont::Bold);
    p.setFont(small);
    p.setPen(theme::Text3);
    p.drawText(QRectF(r.left(), number.bottom() - 2, r.width(), 14), Qt::AlignHCenter | Qt::AlignTop, m_caption.toUpper());
}

// ── ControllerWidget ──────────────────────────────────────

ControllerWidget::ControllerWidget(QWidget *parent) : QWidget(parent)
{
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    setMinimumHeight(150);
}

void ControllerWidget::setState(const drive::ControlInput &input, const std::array<bool, int(drive::PadButton::Count)> &buttons, bool live)
{
    m_input = input;
    m_buttons = buttons;
    m_live = live;
    update();
}

void ControllerWidget::paintEvent(QPaintEvent *)
{
    using drive::PadButton;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = std::min(width() / 340.0, height() / 210.0);
    p.translate((width() - 340 * s) / 2, (height() - 210 * s) / 2);
    p.scale(s, s);

    const QColor key(0x2a, 0x37, 0x4b);
    const QColor hot = theme::Warm;
    const auto on = [&](PadButton b) { return m_buttons[int(b)]; };

    // Triggers and bumpers
    for (int side = 0; side < 2; ++side) {
        const qreal x = side ? 232 : 52;
        const double v = side ? m_input.rightTrigger : m_input.leftTrigger;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x0d, 0x12, 0x1a));
        p.drawRoundedRect(QRectF(x, 6, 56, 10), 5, 5);
        if (v > 0.01) {
            p.setBrush(hot);
            p.drawRoundedRect(QRectF(x, 6, 56 * v, 10), 5, 5);
        }
        const bool bumper = side ? m_input.rightBumper : m_input.leftBumper;
        p.setBrush(bumper ? hot : key);
        p.drawRoundedRect(QRectF(side ? 224 : 46, 22, 70, 12), 6, 6);
    }

    QPainterPath body;
    body.moveTo(70, 40);
    body.lineTo(270, 40);
    body.cubicTo(302, 40, 322, 62, 328, 98);
    body.lineTo(338, 160);
    body.cubicTo(342, 186, 324, 204, 302, 204);
    body.cubicTo(286, 204, 276, 195, 268, 182);
    body.lineTo(254, 158);
    body.lineTo(86, 158);
    body.lineTo(72, 182);
    body.cubicTo(64, 195, 54, 204, 38, 204);
    body.cubicTo(16, 204, -2, 186, 2, 160);
    body.lineTo(12, 98);
    body.cubicTo(18, 62, 38, 40, 70, 40);
    QLinearGradient grad(0, 40, 0, 204);
    grad.setColorAt(0, QColor(0x25, 0x31, 0x44));
    grad.setColorAt(1, QColor(0x17, 0x1f, 0x2c));
    p.setPen(QPen(QColor(255, 255, 255, 20), 1.5));
    p.setBrush(grad);
    p.drawPath(body);

    // Sticks
    const auto drawStick = [&](QPointF c, double x, double y) {
        p.setPen(QPen(QColor(255, 255, 255, 15), 1));
        p.setBrush(QColor(0x0d, 0x12, 0x1a));
        p.drawEllipse(c, 27, 27);
        p.setPen(QPen(m_live ? QColor(0x8b, 0xbc, 0xff) : QColor(0x4d, 0x60, 0x80), 1.5));
        p.setBrush(m_live ? theme::Accent : QColor(0x33, 0x42, 0x5a));
        p.drawEllipse(c + QPointF(x * 13, -y * 13), 14, 14);
    };
    drawStick({92, 92}, m_input.leftX, m_input.leftY);
    drawStick({214, 140}, m_input.rightX, m_input.rightY);

    // D-pad
    p.setPen(Qt::NoPen);
    const struct {
        QRectF r;
        PadButton b;
    } dpad[] = {{{119, 118, 14, 16}, PadButton::Up}, {{119, 146, 14, 16}, PadButton::Down},
                {{103, 133, 16, 14}, PadButton::Left}, {{133, 133, 16, 14}, PadButton::Right}};
    for (const auto &d : dpad) {
        p.setBrush(on(d.b) ? hot : key);
        p.drawRoundedRect(d.r, 3, 3);
    }

    // Face buttons
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(10);
    f.setBold(true);
    p.setFont(f);
    const struct {
        QPointF c;
        PadButton b;
        const char *t;
        QColor color;
    } face[] = {{{250, 70}, PadButton::Y, "Y", hot},
                {{228, 92}, PadButton::X, "X", theme::Accent},
                {{272, 92}, PadButton::B, "B", theme::Bad},
                {{250, 114}, PadButton::A, "A", theme::Good}};
    for (const auto &b : face) {
        p.setPen(Qt::NoPen);
        p.setBrush(on(b.b) ? b.color : key);
        p.drawEllipse(b.c, 11, 11);
        p.setPen(on(b.b) ? QColor(0x0b, 0x0f, 0x15) : theme::Text2);
        p.drawText(QRectF(b.c.x() - 11, b.c.y() - 11, 22, 22), Qt::AlignCenter, QLatin1String(b.t));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(on(PadButton::Back) ? hot : key);
    p.drawRoundedRect(QRectF(146, 84, 16, 9), 4.5, 4.5);
    p.setBrush(on(PadButton::Start) ? hot : key);
    p.drawRoundedRect(QRectF(178, 84, 16, 9), 4.5, 4.5);
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

// ── BrickIllustration ─────────────────────────────────────

BrickIllustration::BrickIllustration(QWidget *parent) : QWidget(parent)
{
    setFixedSize(300, 250);
    // Transparent cut-out made by tools/cutout.cpp; nothing is drawn behind it.
    m_photo = QImage(QStringLiteral(":/images/nxt.png")).convertToFormat(QImage::Format_ARGB32);
    m_dimmed = m_photo;
    for (int y = 0; y < m_dimmed.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(m_dimmed.scanLine(y));
        for (int x = 0; x < m_dimmed.width(); ++x) {
            const int g = qGray(line[x]);
            line[x] = qRgba(g, g, g, qAlpha(line[x]));
        }
    }
}

void BrickIllustration::setOnline(bool on)
{
    if (on == m_on) return;
    m_on = on;
    update();
}

void BrickIllustration::paintEvent(QPaintEvent *)
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
    const QSizeF logical = QSizeF(px) / dpr;
    p.setOpacity(m_on ? 1.0 : 0.55);
    p.drawImage(QPointF((width() - logical.width()) / 2, (height() - logical.height()) / 2), m_scaled);
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
