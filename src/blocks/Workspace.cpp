#include "Workspace.h"

#include <QGraphicsDropShadowEffect>
#include <QGraphicsPathItem>
#include <QJsonArray>
#include <QLineF>
#include <algorithm>

namespace blocks {

namespace {

constexpr qreal SnapStatement = 36;
constexpr qreal SnapValue = 28;

bool accepts(ArgKind slot, Shape value)
{
    switch (slot) {
    case ArgKind::Bool: return value == Shape::Boolean;
    case ArgKind::Number: return value == Shape::Reporter;
    case ArgKind::Text: return value == Shape::Reporter || value == Shape::Boolean;
    default: return false;
    }
}

} // namespace

Workspace::Workspace(QObject *parent) : QGraphicsScene(parent)
{
    setSceneRect(-4000, -4000, 8000, 8000);
    m_marker = new QGraphicsPathItem;
    m_marker->setZValue(999);
    m_marker->setVisible(false);
    addItem(m_marker);
}

QList<Block *> Workspace::topBlocks() const
{
    QList<Block *> out;
    for (QGraphicsItem *it : items())
        if (auto *b = qgraphicsitem_cast<Block *>(it); b && !b->parentItem()) out << b;
    return out;
}

int Workspace::blockCount() const
{
    int n = 0;
    for (QGraphicsItem *it : items())
        if (qgraphicsitem_cast<Block *>(it)) ++n;
    return n;
}

QJsonObject Workspace::toJson() const
{
    QJsonArray stacks;
    for (Block *b : topBlocks()) {
        if (b == m_drag) continue;
        QJsonObject s;
        s[QStringLiteral("x")] = qRound(b->scenePos().x());
        s[QStringLiteral("y")] = qRound(b->scenePos().y());
        s[QStringLiteral("block")] = b->toJson();
        stacks.append(s);
    }
    QJsonObject o;
    o[QStringLiteral("variables")] = QJsonArray::fromStringList(m_variables);
    o[QStringLiteral("stacks")] = stacks;
    return o;
}

void Workspace::loadJson(const QJsonObject &obj)
{
    clearBlocks();
    m_variables.clear();
    for (const QJsonValue &v : obj.value(QStringLiteral("variables")).toArray())
        if (!v.toString().isEmpty() && !m_variables.contains(v.toString())) m_variables << v.toString();
    for (const QJsonValue &v : obj.value(QStringLiteral("stacks")).toArray()) {
        const QJsonObject s = v.toObject();
        if (Block *b = Block::fromJson(s.value(QStringLiteral("block")).toObject()))
            addTree(b, QPointF(s.value(QStringLiteral("x")).toDouble(), s.value(QStringLiteral("y")).toDouble()));
    }
    emit variablesChanged();
}

void Workspace::clearBlocks()
{
    m_drag = nullptr;
    m_marker->setVisible(false);
    qDeleteAll(topBlocks());
}

void Workspace::addTree(Block *b, const QPointF &scenePos)
{
    addItem(b);
    b->setPos(scenePos);
    b->relayout();
}

bool Workspace::addVariable(const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || m_variables.contains(n)) return false;
    m_variables << n;
    emit variablesChanged();
    commit();
    return true;
}

void Workspace::renameVariable(const QString &from, const QString &to)
{
    const QString n = to.trimmed();
    const qsizetype i = m_variables.indexOf(from);
    if (i < 0 || n.isEmpty() || m_variables.contains(n)) return;
    m_variables[i] = n;
    for (Block *top : topBlocks())
        top->forEach([&](Block *b) {
            for (const ArgDef &a : b->def()->args)
                if (a.kind == ArgKind::Variable && b->field(a.name) == from) b->setField(a.name, n);
        });
    emit variablesChanged();
    commit();
}

void Workspace::beginDrag(Block *b, const QPointF &grabOffset)
{
    b->detach();
    m_drag = b;
    m_grab = grabOffset;
    m_target = {};
    b->setZValue(1000);
    auto *shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(22);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 150));
    b->setGraphicsEffect(shadow);
}

void Workspace::dragTo(const QPointF &scenePos, const QPoint &globalPos)
{
    if (!m_drag) return;
    m_drag->setPos(scenePos - m_grab);
    m_overDelete = isOverDeleteArea && isOverDeleteArea(globalPos);
    m_drag->setOpacity(m_overDelete ? 0.45 : 1.0);
    m_target = m_overDelete ? Target{} : findTarget(m_drag);
    showMarker(m_target, m_drag);
}

void Workspace::cancelDrag()
{
    if (!m_drag) return;
    m_marker->setVisible(false);
    removeItem(m_drag);
    m_drag->deleteLater();
    m_drag = nullptr;
}

void Workspace::endDrag(const QPoint &globalPos)
{
    if (!m_drag) return;
    m_overDelete = isOverDeleteArea && isOverDeleteArea(globalPos);
    m_marker->setVisible(false);
    Block *d = m_drag;
    m_drag = nullptr;
    d->setGraphicsEffect(nullptr);
    d->setZValue(0);
    d->setOpacity(1);

    if (m_overDelete) {
        // We're inside the block's own mouse handler, so free it once that returns.
        removeItem(d);
        d->deleteLater();
        commit();
        return;
    }

    const Target t = m_target;
    m_target = {};
    Block *host = t.block;
    // Something already attached where `d` goes moves below `d`'s stack, or out of the way.
    const auto reattach = [&](Block *old) {
        if (!old) return;
        Block *last = d->lastInChain();
        if (last->def()->hasNext() && old->def()->hasPrevious()) last->attachNext(old);
        else bump(old, d->scenePos());
    };

    switch (t.kind) {
    case Target::Next: {
        Block *old = host->nextBlock();
        if (old) old->detach();
        host->attachNext(d);
        reattach(old);
        break;
    }
    case Target::Statement: {
        Block *old = host->statement(t.name);
        if (old) old->detach();
        host->attachStatement(t.name, d);
        reattach(old);
        break;
    }
    case Target::Above:
        d->setPos(host->scenePos() - QPointF(0, d->chainHeight()));
        d->lastInChain()->attachNext(host);
        break;
    case Target::Input: {
        Block *old = host->input(t.name);
        if (old) {
            old->detach();
            bump(old, host->scenePos());
        }
        host->attachInput(t.name, d);
        break;
    }
    case Target::None:
        break;
    }
    d->relayout();
    commit();
}

Workspace::Target Workspace::findTarget(Block *d) const
{
    Target best;
    const auto excluded = [&](Block *b) { return b == d || d->isAncestorOf(b) || b->isTemplate(); };
    QList<Block *> all;
    for (QGraphicsItem *it : items())
        if (auto *b = qgraphicsitem_cast<Block *>(it); b && !excluded(b)) all << b;

    const auto consider = [&](Target::Kind kind, Block *b, const QString &name, const QPointF &from, const QPointF &to, qreal radius) {
        const qreal dist = QLineF(from, to).length();
        if (dist < radius && dist < best.distance) best = {kind, b, name, dist};
    };

    if (d->def()->isValue()) {
        const QPointF from = d->scenePos() + QPointF(0, d->bodyHeight() / 2);
        for (Block *b : all)
            for (const SlotInfo &s : b->valueSlots())
                if (accepts(s.kind, d->def()->shape))
                    consider(Target::Input, b, s.name, from, b->mapToScene(QPointF(s.rect.left(), s.rect.center().y())), SnapValue);
        return best;
    }

    const QPointF top = d->scenePos();
    if (d->def()->hasPrevious()) {
        for (Block *b : all) {
            if (b->def()->hasNext()) consider(Target::Next, b, {}, top, b->mapToScene(QPointF(0, b->bodyHeight())), SnapStatement);
            for (const QString &name : b->statementNames())
                consider(Target::Statement, b, name, top, b->mapToScene(b->statementOrigin(name)), SnapStatement);
        }
    }
    if (d->lastInChain()->def()->hasNext()) {
        const qreal h = d->chainHeight();
        for (Block *b : all)
            if (!b->parentItem() && b->def()->hasPrevious())
                consider(Target::Above, b, {}, top, b->scenePos() - QPointF(0, h), SnapStatement);
    }
    return best;
}

void Workspace::showMarker(const Target &t, Block *dragged)
{
    if (t.kind == Target::None) {
        m_marker->setVisible(false);
        return;
    }
    QPainterPath path;
    if (t.kind == Target::Input) {
        for (const SlotInfo &s : t.block->valueSlots())
            if (s.name == t.name) {
                const QRectF r = t.block->mapRectToScene(s.rect).adjusted(-2, -2, 2, 2);
                path.addRoundedRect(r, r.height() / 2, r.height() / 2);
            }
        m_marker->setPen(QPen(QColor(255, 255, 255, 230), 2.5));
        m_marker->setBrush(QColor(255, 255, 255, 40));
    } else {
        QPointF at;
        switch (t.kind) {
        case Target::Next: at = t.block->mapToScene(QPointF(0, t.block->bodyHeight())); break;
        case Target::Statement: at = t.block->mapToScene(t.block->statementOrigin(t.name)); break;
        default: at = t.block->scenePos(); break;
        }
        path.addRoundedRect(QRectF(at.x(), at.y() - 3, std::min<qreal>(dragged->width(), 180), 6), 3, 3);
        m_marker->setPen(Qt::NoPen);
        m_marker->setBrush(QColor(255, 255, 255, 170));
    }
    m_marker->setPath(path);
    m_marker->setVisible(true);
}

void Workspace::bump(Block *b, const QPointF &near)
{
    b->setPos(near + QPointF(40, 40));
    b->relayout();
}

void Workspace::deleteBlock(Block *b)
{
    Block *parent = b->parentBlock();
    const Block::Link link = b->link();
    const QString name = b->linkName();
    const QPointF pos = b->scenePos();
    Block *next = b->nextBlock();
    if (next) next->detach();
    b->detach();
    delete b;
    if (next) {
        if (parent && link == Block::Link::Next) parent->attachNext(next);
        else if (parent && link == Block::Link::Statement) parent->attachStatement(name, next);
        else next->setPos(pos);
        next->relayout();
    }
    commit();
}

Block *Workspace::duplicate(Block *b)
{
    Block *copy = Block::fromJson(b->toJson());
    if (!copy) return nullptr;
    addTree(copy, b->scenePos() + QPointF(28, 28));
    clearSelection();
    copy->setSelected(true);
    commit();
    return copy;
}

void Workspace::cleanUp()
{
    QList<Block *> tops = topBlocks();
    std::sort(tops.begin(), tops.end(), [](Block *a, Block *b) { return a->scenePos().y() < b->scenePos().y(); });
    qreal y = 40;
    for (Block *b : tops) {
        b->setPos(40, y);
        y += b->mapRectToScene(b->childrenBoundingRect() | b->boundingRect()).height() + 32;
    }
    commit();
}

void Workspace::requestFieldEdit(Block *b, const QString &field, const QPoint &globalPos)
{
    emit fieldEditRequested(b, field, globalPos);
}

} // namespace blocks
