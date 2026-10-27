#include "Block.h"
#include "Workspace.h"

#include <QCursor>
#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QJsonArray>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <algorithm>

namespace blocks {

namespace {

constexpr qreal PadX = 10;
constexpr qreal Gap = 6;
constexpr qreal RowMin = 40;
constexpr qreal ValueMin = 30;
constexpr qreal Arm = 16;
constexpr qreal MouthMin = 24;
constexpr qreal BottomArm = 22;
constexpr qreal Corner = 6;
constexpr qreal HatHeight = 18;
constexpr qreal NotchX = 14;
constexpr qreal NotchDepth = 5;
constexpr qreal FieldHeight = 24;
constexpr qreal IconSize = 22;
constexpr qreal MinStatementWidth = 64;

const QFont &blockFont()
{
    static const QFont f = [] {
        QFont font(QStringLiteral("Segoe UI"));
        font.setPixelSize(12);
        font.setWeight(QFont::DemiBold);
        return font;
    }();
    return f;
}

QSvgRenderer *iconRenderer(const QString &path)
{
    static QHash<QString, QSvgRenderer *> cache;
    auto it = cache.find(path);
    if (it == cache.end()) it = cache.insert(path, new QSvgRenderer(path));
    return it.value();
}

/** Splits "%1 motor %2 on" into labels and argument references. */
struct Token {
    bool arg;
    QString text;
    int index;
};

QList<Token> tokenize(const QString &message)
{
    QList<Token> out;
    static const QRegularExpression re(QStringLiteral("%(\\d+)"));
    qsizetype last = 0;
    auto addText = [&](const QString &t) {
        const QString s = t.trimmed();
        if (!s.isEmpty()) out.append({false, s, -1});
    };
    for (auto it = re.globalMatch(message); it.hasNext();) {
        const auto m = it.next();
        addText(message.mid(last, m.capturedStart() - last));
        out.append({true, {}, m.captured(1).toInt() - 1});
        last = m.capturedEnd();
    }
    addText(message.mid(last));
    return out;
}

/** Top-edge notch, drawn left-to-right, dipping into the block. */
void notchRight(QPainterPath &p, qreal x, qreal y)
{
    p.lineTo(x, y);
    p.cubicTo(x + 3, y, x + 3, y + NotchDepth, x + 6, y + NotchDepth);
    p.lineTo(x + 12, y + NotchDepth);
    p.cubicTo(x + 15, y + NotchDepth, x + 15, y, x + 18, y);
}

/** Bottom-edge tab, drawn right-to-left, sticking out below. */
void tabLeft(QPainterPath &p, qreal x, qreal y)
{
    p.lineTo(x + 18, y);
    p.cubicTo(x + 15, y, x + 15, y + NotchDepth, x + 12, y + NotchDepth);
    p.lineTo(x + 6, y + NotchDepth);
    p.cubicTo(x + 3, y + NotchDepth, x + 3, y, x, y);
}

QPainterPath hexagon(const QRectF &r)
{
    const qreal k = r.height() / 2;
    QPainterPath p;
    p.moveTo(r.left() + k, r.top());
    p.lineTo(r.right() - k, r.top());
    p.lineTo(r.right(), r.center().y());
    p.lineTo(r.right() - k, r.bottom());
    p.lineTo(r.left() + k, r.bottom());
    p.lineTo(r.left(), r.center().y());
    p.closeSubpath();
    return p;
}

bool isEditable(ArgKind k)
{
    return k == ArgKind::Dropdown || k == ArgKind::Variable || k == ArgKind::Number || k == ArgKind::Text;
}

bool isSlot(ArgKind k) { return k == ArgKind::Number || k == ArgKind::Text || k == ArgKind::Bool; }

} // namespace

Block::Block(const BlockDef *def, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_def(def), m_fields(defaultFields(*def))
{
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);
    layoutTree();
}

Block *Block::parentBlock() const { return qgraphicsitem_cast<Block *>(parentItem()); }

Workspace *Block::workspace() const { return qobject_cast<Workspace *>(scene()); }

void Block::setField(const QString &name, const QString &value)
{
    if (m_fields.value(name) == value) return;
    m_fields.insert(name, value);
    relayout();
}

void Block::attachNext(Block *b)
{
    m_next = b;
    b->setParentItem(this);
    b->m_link = Link::Next;
    b->m_linkName.clear();
}

void Block::attachStatement(const QString &name, Block *b)
{
    m_statements.insert(name, b);
    b->setParentItem(this);
    b->m_link = Link::Statement;
    b->m_linkName = name;
}

void Block::attachInput(const QString &name, Block *b)
{
    m_inputs.insert(name, b);
    b->setParentItem(this);
    b->m_link = Link::Input;
    b->m_linkName = name;
}

Block *Block::detach()
{
    Block *p = parentBlock();
    if (!p) return nullptr;
    switch (m_link) {
    case Link::Next: p->m_next = nullptr; break;
    case Link::Statement: p->m_statements.remove(m_linkName); break;
    case Link::Input: p->m_inputs.remove(m_linkName); break;
    case Link::None: break;
    }
    const QPointF sp = scenePos();
    setParentItem(nullptr);
    setPos(sp);
    m_link = Link::None;
    m_linkName.clear();
    p->relayout();
    return p;
}

Block *Block::root()
{
    Block *b = this;
    while (Block *p = b->parentBlock()) b = p;
    return b;
}

Block *Block::lastInChain()
{
    Block *b = this;
    while (b->m_next) b = b->m_next;
    return b;
}

void Block::forEach(const std::function<void(Block *)> &fn)
{
    fn(this);
    for (Block *c : std::as_const(m_inputs)) c->forEach(fn);
    for (Block *c : std::as_const(m_statements)) c->forEach(fn);
    if (m_next) m_next->forEach(fn);
}

qreal Block::chainHeight() const { return m_height + (m_next ? m_next->chainHeight() : 0); }

QList<QString> Block::statementNames() const
{
    QList<QString> out;
    for (const RowDef &r : m_def->rows)
        if (r.statement) out << r.name;
    return out;
}

QPointF Block::statementOrigin(const QString &name) const
{
    for (const Mouth &m : m_mouths)
        if (m.name == name) return {Arm, m.y};
    return {};
}

void Block::relayout() { root()->layoutTree(); }

QString Block::displayText(const ArgDef &a) const
{
    const QString v = m_fields.value(a.name);
    if (a.kind == ArgKind::Dropdown) {
        for (const auto &opt : a.options)
            if (opt.second == v) return opt.first;
    }
    return v;
}

QSizeF Block::argSize(const ArgDef &a) const
{
    const QFontMetricsF fm(blockFont());
    switch (a.kind) {
    case ArgKind::Icon:
        return {IconSize, IconSize};
    case ArgKind::Dropdown:
    case ArgKind::Variable:
        return {fm.horizontalAdvance(displayText(a)) + 30, 26};
    case ArgKind::Number:
    case ArgKind::Text:
    case ArgKind::Bool:
        if (Block *c = m_inputs.value(a.name)) return {c->m_width, c->m_height};
        if (a.kind == ArgKind::Bool) return {44, FieldHeight};
        return {std::max<qreal>(34, fm.horizontalAdvance(displayText(a)) + 20), FieldHeight};
    }
    return {};
}

void Block::layoutTree()
{
    for (Block *c : std::as_const(m_inputs)) c->layoutTree();
    for (Block *c : std::as_const(m_statements)) c->layoutTree();

    prepareGeometryChange();
    const QFontMetricsF fm(blockFont());
    const bool value = m_def->isValue();
    const bool boolean = m_def->shape == Shape::Boolean;

    m_rows.clear();
    m_rowY.clear();
    m_mouths.clear();
    m_slots.clear();

    qreal y = m_def->shape == Shape::Hat ? HatHeight : 0;
    qreal width = value ? 0 : MinStatementWidth;
    bool endsWithMouth = false;

    for (const RowDef &row : m_def->rows) {
        if (row.statement) {
            Block *first = m_statements.value(row.name);
            const qreal h = std::max(MouthMin, first ? first->chainHeight() : 0);
            m_mouths.append({row.name, y, h});
            y += h;
            endsWithMouth = true;
            continue;
        }
        endsWithMouth = false;

        QList<Elem> elems;
        qreal contentH = fm.height();
        for (const Token &t : tokenize(row.message)) {
            Elem e;
            if (t.arg) {
                if (t.index < 0 || t.index >= m_def->args.size()) continue;
                e.kind = Elem::Arg;
                e.arg = t.index;
                e.rect.setSize(argSize(m_def->args[t.index]));
            } else {
                e.text = t.text;
                e.rect.setSize({fm.horizontalAdvance(t.text), fm.height()});
            }
            contentH = std::max(contentH, e.rect.height());
            elems.append(e);
        }

        const qreal rh = value ? std::max(ValueMin, contentH + 6) : std::max(RowMin, contentH + 16);
        const qreal pad = value ? (boolean ? rh / 2 : 8) : PadX;
        qreal x = pad;
        for (Elem &e : elems) {
            e.rect.moveTo(x, y + (rh - e.rect.height()) / 2);
            x += e.rect.width() + Gap;
        }
        width = std::max(width, x - Gap + pad);
        m_rows.append(elems);
        m_rowY.append(y);
        y += rh;
    }
    if (endsWithMouth) y += BottomArm;

    m_width = value ? std::max(width, y) : width; // a lone value is at least round
    m_height = y;

    // Place plugged-in values and the stacks inside mouths.
    for (const QList<Elem> &elems : std::as_const(m_rows)) {
        for (const Elem &e : elems) {
            if (e.kind != Elem::Arg) continue;
            const ArgDef &a = m_def->args[e.arg];
            if (!isSlot(a.kind)) continue;
            m_slots.append({a.name, a.kind, e.rect});
            if (Block *c = m_inputs.value(a.name)) c->setPos(e.rect.topLeft());
        }
    }
    for (const Mouth &m : std::as_const(m_mouths))
        if (Block *first = m_statements.value(m.name)) first->setPos(Arm, m.y);

    buildPath();
    if (m_next) {
        m_next->layoutTree();
        m_next->setPos(0, m_height);
    }
    update();
}

void Block::buildPath()
{
    QPainterPath p;
    const qreal W = m_width;
    const qreal H = m_height;
    const qreal r = Corner;

    if (m_def->shape == Shape::Reporter) {
        p.addRoundedRect(QRectF(0, 0, W, H), H / 2, H / 2);
        m_path = p;
        return;
    }
    if (m_def->shape == Shape::Boolean) {
        m_path = hexagon(QRectF(0, 0, W, H));
        return;
    }

    if (m_def->shape == Shape::Hat) {
        const qreal top = HatHeight;
        p.moveTo(0, top);
        p.cubicTo(28, -6, 78, -6, 110, top);
        p.lineTo(W - r, top);
        p.quadTo(W, top, W, top + r);
    } else {
        p.moveTo(0, r);
        p.quadTo(0, 0, r, 0);
        if (m_def->hasPrevious()) notchRight(p, NotchX, 0);
        p.lineTo(W - r, 0);
        p.quadTo(W, 0, W, r);
    }

    for (const Mouth &m : std::as_const(m_mouths)) {
        p.lineTo(W, m.y - r);
        p.quadTo(W, m.y, W - r, m.y);
        tabLeft(p, Arm + NotchX, m.y);
        p.lineTo(Arm + r, m.y);
        p.quadTo(Arm, m.y, Arm, m.y + r);
        p.lineTo(Arm, m.y + m.height - r);
        p.quadTo(Arm, m.y + m.height, Arm + r, m.y + m.height);
        p.lineTo(W - r, m.y + m.height);
        p.quadTo(W, m.y + m.height, W, m.y + m.height + r);
    }

    p.lineTo(W, H - r);
    p.quadTo(W, H, W - r, H);
    if (m_def->hasNext()) tabLeft(p, NotchX, H);
    p.lineTo(r, H);
    p.quadTo(0, H, 0, H - r);
    p.closeSubpath();
    m_path = p;
}

QRectF Block::boundingRect() const { return QRectF(-2, -2, m_width + 4, m_height + NotchDepth + 4); }

QPainterPath Block::shape() const { return m_path; }

void Block::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QColor base = categoryColor(m_def->category);
    const QColor edge = base.darker(125);
    const QColor fieldFill = base.darker(118);

    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(QPen(edge, 1));
    p->setBrush(base);
    p->drawPath(m_path);
    if (isSelected()) {
        p->setPen(QPen(QColor(0xff, 0xd1, 0x66), 2.5));
        p->setBrush(Qt::NoBrush);
        p->drawPath(m_path);
    }

    p->setFont(blockFont());
    for (const QList<Elem> &elems : std::as_const(m_rows)) {
        for (const Elem &e : elems) {
            if (e.kind == Elem::Label) {
                p->setPen(Qt::white);
                p->drawText(e.rect, Qt::AlignLeft | Qt::AlignVCenter, e.text);
                continue;
            }
            const ArgDef &a = m_def->args[e.arg];
            switch (a.kind) {
            case ArgKind::Icon:
                iconRenderer(a.icon)->render(p, e.rect);
                break;
            case ArgKind::Dropdown:
            case ArgKind::Variable: {
                p->setPen(QPen(edge.darker(110), 1));
                p->setBrush(fieldFill);
                p->drawRoundedRect(e.rect, e.rect.height() / 2, e.rect.height() / 2);
                p->setPen(Qt::white);
                p->drawText(e.rect.adjusted(10, 0, -18, 0), Qt::AlignLeft | Qt::AlignVCenter, displayText(a));
                QPainterPath tri;
                const QPointF c(e.rect.right() - 11, e.rect.center().y() + 1);
                tri.moveTo(c + QPointF(-4, -2));
                tri.lineTo(c + QPointF(4, -2));
                tri.lineTo(c + QPointF(0, 3));
                tri.closeSubpath();
                p->fillPath(tri, Qt::white);
                break;
            }
            case ArgKind::Number:
            case ArgKind::Text:
                if (m_inputs.contains(a.name)) break;
                p->setPen(QPen(edge, 1));
                p->setBrush(Qt::white);
                p->drawRoundedRect(e.rect, e.rect.height() / 2, e.rect.height() / 2);
                p->setPen(QColor(0x1f, 0x26, 0x33));
                p->drawText(e.rect, Qt::AlignCenter, displayText(a));
                break;
            case ArgKind::Bool:
                if (m_inputs.contains(a.name)) break;
                p->setPen(Qt::NoPen);
                p->setBrush(base.darker(140));
                p->drawPath(hexagon(e.rect));
                break;
            }
        }
    }
}

const Block::Elem *Block::editableAt(const QPointF &pos) const
{
    for (const QList<Elem> &elems : m_rows)
        for (const Elem &e : elems)
            if (e.kind == Elem::Arg && e.rect.contains(pos)) {
                const ArgDef &a = m_def->args[e.arg];
                if (isEditable(a.kind) && !m_inputs.contains(a.name)) return &e;
            }
    return nullptr;
}

void Block::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (m_template) return;
    setCursor(editableAt(e->pos()) ? Qt::PointingHandCursor : Qt::OpenHandCursor);
}

void Block::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (m_template || e->button() != Qt::LeftButton) {
        e->ignore();
        return;
    }
    if (scene()) scene()->clearSelection();
    setSelected(true);
    m_pressPos = e->pos();
    m_pressed = true;
    m_dragging = false;
    e->accept();
}

void Block::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    Workspace *ws = workspace();
    if (!m_pressed || !ws) return;
    if (!m_dragging) {
        if ((e->screenPos() - e->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() < 4) return;
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
        ws->beginDrag(this, m_pressPos);
    }
    ws->dragTo(e->scenePos(), e->screenPos());
}

void Block::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    Workspace *ws = workspace();
    const bool wasDragging = m_dragging;
    m_pressed = false;
    m_dragging = false;
    if (!ws) return;
    if (wasDragging) {
        ws->endDrag(e->screenPos());
        return; // the block may have been deleted
    }
    if (const Elem *el = editableAt(m_pressPos)) ws->requestFieldEdit(this, m_def->args[el->arg].name, e->screenPos());
}

QJsonObject Block::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("type")] = m_def->type;
    if (!m_fields.isEmpty()) {
        QJsonObject f;
        for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) f[it.key()] = it.value();
        o[QStringLiteral("fields")] = f;
    }
    if (!m_inputs.isEmpty()) {
        QJsonObject in;
        for (auto it = m_inputs.cbegin(); it != m_inputs.cend(); ++it) in[it.key()] = it.value()->toJson();
        o[QStringLiteral("inputs")] = in;
    }
    if (!m_statements.isEmpty()) {
        QJsonObject st;
        for (auto it = m_statements.cbegin(); it != m_statements.cend(); ++it) st[it.key()] = it.value()->toJson();
        o[QStringLiteral("statements")] = st;
    }
    if (m_next) o[QStringLiteral("next")] = m_next->toJson();
    return o;
}

Block *Block::fromJson(const QJsonObject &obj)
{
    const BlockDef *def = findDef(obj.value(QStringLiteral("type")).toString());
    if (!def) return nullptr;
    auto *b = new Block(def);
    const QJsonObject f = obj.value(QStringLiteral("fields")).toObject();
    for (auto it = f.begin(); it != f.end(); ++it)
        if (def->arg(it.key())) b->m_fields.insert(it.key(), it.value().toVariant().toString());

    const QJsonObject in = obj.value(QStringLiteral("inputs")).toObject();
    for (auto it = in.begin(); it != in.end(); ++it) {
        const ArgDef *a = def->arg(it.key());
        if (!a || !isSlot(a->kind)) continue;
        if (Block *c = fromJson(it.value().toObject())) {
            if (c->def()->isValue()) b->attachInput(it.key(), c);
            else delete c;
        }
    }
    const QJsonObject st = obj.value(QStringLiteral("statements")).toObject();
    for (auto it = st.begin(); it != st.end(); ++it) {
        if (!b->statementNames().contains(it.key())) continue;
        if (Block *c = fromJson(it.value().toObject())) {
            if (c->def()->hasPrevious()) b->attachStatement(it.key(), c);
            else delete c;
        }
    }
    if (def->hasNext() && obj.contains(QStringLiteral("next"))) {
        if (Block *c = fromJson(obj.value(QStringLiteral("next")).toObject())) {
            if (c->def()->hasPrevious()) b->attachNext(c);
            else delete c;
        }
    }
    b->layoutTree();
    return b;
}

} // namespace blocks
