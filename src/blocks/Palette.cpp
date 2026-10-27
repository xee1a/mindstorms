#include "Palette.h"
#include "Block.h"
#include "Canvas.h"

#include <QApplication>
#include <QDrag>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollBar>

namespace blocks {

namespace {

constexpr qreal PaletteScale = 0.9;
constexpr qreal Margin = 16;

QIcon categoryDot(const QColor &c)
{
    QPixmap pm(40, 40);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c.lighter(130), 3));
    p.setBrush(c);
    p.drawEllipse(QRectF(5, 5, 30, 30));
    return QIcon(pm);
}

} // namespace

// ── PaletteView ───────────────────────────────────────────

PaletteView::PaletteView(QWidget *parent) : QGraphicsView(parent)
{
    setObjectName(QStringLiteral("paletteView"));
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(0x18, 0x21, 0x2e));
    scale(PaletteScale, PaletteScale);
}

Block *PaletteView::templateAt(const QPoint &viewPos) const
{
    for (QGraphicsItem *it = itemAt(viewPos); it; it = it->parentItem())
        if (auto *b = qgraphicsitem_cast<Block *>(it); b && b->isTemplate()) return b;
    return nullptr;
}

void PaletteView::mousePressEvent(QMouseEvent *e)
{
    m_pressed = e->button() == Qt::LeftButton ? templateAt(e->pos()) : nullptr;
    m_pressPos = e->pos();
    if (!m_pressed) QGraphicsView::mousePressEvent(e);
}

void PaletteView::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_pressed || (e->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) {
        QGraphicsView::mouseMoveEvent(e);
        return;
    }
    Block *b = m_pressed;
    m_pressed = nullptr;

    const QPointF grab = b->mapFromScene(mapToScene(m_pressPos));
    QJsonObject o;
    o[QStringLiteral("block")] = b->toJson();
    o[QStringLiteral("grabX")] = grab.x();
    o[QStringLiteral("grabY")] = grab.y();
    auto *mime = new QMimeData;
    mime->setData(QLatin1String(BlockMimeType), QJsonDocument(o).toJson(QJsonDocument::Compact));

    // A faint ghost while over the palette; on the canvas the real block follows the cursor.
    const qreal dpr = devicePixelRatioF();
    const QRectF src = b->sceneBoundingRect();
    QPixmap pm((src.size() * PaletteScale * dpr).toSize());
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(0.35);
        scene()->render(&p, QRectF(QPointF(0, 0), src.size() * PaletteScale), src);
    }
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(pm);
    drag->setHotSpot(((grab - (src.topLeft() - b->scenePos())) * PaletteScale).toPoint());
    drag->exec(Qt::CopyAction);
}

void PaletteView::mouseReleaseEvent(QMouseEvent *e)
{
    m_pressed = nullptr;
    QGraphicsView::mouseReleaseEvent(e);
}

// ── Palette ───────────────────────────────────────────────

Palette::Palette(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("palette"));
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Category column: a colour dot with the name centred underneath.
    auto *column = new QWidget(this);
    column->setObjectName(QStringLiteral("categoryList"));
    column->setAttribute(Qt::WA_StyledBackground);
    column->setFixedWidth(96);
    auto *cl = new QVBoxLayout(column);
    cl->setContentsMargins(6, 10, 6, 10);
    cl->setSpacing(2);
    m_categories = new QButtonGroup(this);
    for (int i = 0; i < CategoryCount; ++i) {
        auto *b = new QToolButton(column);
        b->setText(categoryName(Category(i)));
        b->setIcon(categoryDot(categoryColor(Category(i))));
        b->setIconSize(QSize(20, 20));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFixedHeight(54);
        m_categories->addButton(b, i);
        cl->addWidget(b);
    }
    cl->addStretch();
    lay->addWidget(column);

    m_scene = new QGraphicsScene(this);
    m_view = new PaletteView(this);
    m_view->setScene(m_scene);
    m_view->setFixedWidth(300);
    lay->addWidget(m_view);

    connect(m_view, &PaletteView::makeVariableClicked, this, &Palette::makeVariableClicked);
    connect(m_categories, &QButtonGroup::idClicked, this, &Palette::scrollToCategory);
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this, &Palette::syncCategoryToScroll);
    m_categories->button(0)->setChecked(true);
}

void Palette::rebuild(const QStringList &variables)
{
    const int keepScroll = m_view->verticalScrollBar()->value();
    m_scene->clear();
    m_headingY.clear();

    QFont headingFont(QStringLiteral("Segoe UI"));
    headingFont.setPixelSize(14);
    headingFont.setWeight(QFont::Bold);

    qreal y = Margin;
    const auto addTemplate = [&](const QString &type, const QMap<QString, QString> &fields = {}) {
        const BlockDef *def = findDef(type);
        if (!def) return;
        auto *b = new Block(def);
        for (auto it = fields.cbegin(); it != fields.cend(); ++it) b->setField(it.key(), it.value());
        b->setTemplate(true);
        b->setCursor(Qt::OpenHandCursor);
        m_scene->addItem(b);
        b->setPos(Margin, y);
        y += b->chainHeight() + 14;
    };

    for (int i = 0; i < CategoryCount; ++i) {
        const auto cat = Category(i);
        auto *heading = m_scene->addSimpleText(categoryName(cat), headingFont);
        heading->setBrush(QColor(0xc9, 0xd4, 0xe3));
        heading->setPos(Margin, y);
        m_headingY << y;
        y += 30;

        if (cat == Category::Variables) {
            auto *button = new QPushButton(tr("Make a variable"));
            button->setObjectName(QStringLiteral("makeVariable"));
            button->setCursor(Qt::PointingHandCursor);
            connect(button, &QPushButton::clicked, m_view, &PaletteView::makeVariableClicked);
            QGraphicsProxyWidget *proxy = m_scene->addWidget(button);
            proxy->setPos(Margin, y);
            y += proxy->size().height() + 14;
            for (const QString &v : variables) addTemplate(QStringLiteral("data_variable"), {{QStringLiteral("VAR"), v}});
            if (!variables.isEmpty()) {
                addTemplate(QStringLiteral("data_set"), {{QStringLiteral("VAR"), variables.first()}});
                addTemplate(QStringLiteral("data_change"), {{QStringLiteral("VAR"), variables.first()}});
            }
            continue;
        }
        for (const QString &type : paletteTypes(cat)) addTemplate(type);
        y += 12;
    }
    // Room to scroll the last category to the top.
    m_scene->setSceneRect(0, 0, 300 / PaletteScale - 4, y + 700);
    m_view->verticalScrollBar()->setValue(keepScroll);
}

void Palette::scrollToCategory(int row)
{
    if (row < 0 || row >= m_headingY.size()) return;
    QScrollBar *bar = m_view->verticalScrollBar();
    auto *anim = new QPropertyAnimation(bar, "value", this);
    anim->setDuration(260);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(bar->value());
    anim->setEndValue(int((m_headingY[row] - 8) * PaletteScale));
    m_scrollingProgrammatically = true;
    connect(anim, &QPropertyAnimation::finished, this, [this] { m_scrollingProgrammatically = false; });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void Palette::syncCategoryToScroll()
{
    if (m_scrollingProgrammatically || m_headingY.isEmpty()) return;
    const qreal top = m_view->mapToScene(0, 0).y() + 24;
    int row = 0;
    for (int i = 0; i < m_headingY.size(); ++i)
        if (m_headingY[i] <= top) row = i;
    if (auto *b = m_categories->button(row)) b->setChecked(true);
}

} // namespace blocks
