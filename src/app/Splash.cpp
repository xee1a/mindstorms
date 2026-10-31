#include "Splash.h"
#include "Theme.h"

#include <QApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSvgRenderer>
#include <QTimer>
#include <algorithm>

namespace {

constexpr QSize Size{560, 320};
constexpr int MinimumMs = 1200;

QPixmap background()
{
    const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap pm(Size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(QPointF(0, 0), QSizeF(Size));
    QPainterPath card;
    card.addRoundedRect(r.adjusted(1, 1, -1, -1), 18, 18);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0, theme::Surface);
    g.setColorAt(1, theme::Bg);
    p.fillPath(card, g);
    p.setPen(QPen(theme::Border, 1.5));
    p.drawPath(card);
    // A band of block colours along the bottom, like a stack in the editor.
    const QColor band[] = {theme::Warm, theme::Accent, theme::Good, theme::Accent2, theme::Bad};
    const qreal w = (r.width() - 2) / 5;
    p.setClipPath(card);
    for (int i = 0; i < 5; ++i) p.fillRect(QRectF(1 + i * w, r.bottom() - 7, w, 6), band[i]);
    p.setClipping(false);
    QSvgRenderer(QStringLiteral(":/icons/app.svg")).render(&p, QRectF(48, 92, 96, 96));
    return pm;
}

} // namespace

Splash::Splash() : QSplashScreen(background())
{
    setAttribute(Qt::WA_TranslucentBackground);
    m_status = tr("Starting…");
    m_shownAt = QDateTime::currentMSecsSinceEpoch();
}

void Splash::setStatus(const QString &text)
{
    m_status = text;
    repaint();
    QApplication::processEvents();
}

void Splash::drawContents(QPainter *p)
{
    p->setRenderHint(QPainter::Antialiasing);
    QFont title = font();
    title.setPixelSize(40);
    title.setBold(true);
    p->setFont(title);
    p->setPen(theme::Text);
    p->drawText(QRectF(170, 92, 360, 52), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("NXT Studio"));
    QFont body = font();
    body.setPixelSize(15);
    p->setFont(body);
    p->setPen(theme::Text2);
    p->drawText(QRectF(172, 144, 360, 44), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                tr("Block programming and gamepad driving for LEGO MINDSTORMS NXT"));
    body.setPixelSize(12);
    p->setFont(body);
    p->setPen(theme::Text3);
    p->drawText(QRectF(48, 262, 300, 24), Qt::AlignLeft | Qt::AlignVCenter, m_status);
    p->drawText(QRectF(260, 262, 252, 24), Qt::AlignRight | Qt::AlignVCenter, tr("Version %1").arg(QApplication::applicationVersion()));
}

void Splash::finishWhenReady(QWidget *window)
{
    const qint64 left = MinimumMs - (QDateTime::currentMSecsSinceEpoch() - m_shownAt);
    QTimer::singleShot(std::max<qint64>(0, left), this, [this, window] {
        window->show();
        finish(window);
        deleteLater();
    });
}
