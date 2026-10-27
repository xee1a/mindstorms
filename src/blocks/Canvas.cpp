#include "Canvas.h"
#include "Workspace.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QShortcut>
#include <QToolButton>
#include <QWheelEvent>
#include <cmath>

namespace blocks {

namespace {
constexpr qreal GridStep = 28;
constexpr qreal MinZoom = 0.4;
constexpr qreal MaxZoom = 2.0;

template <typename Fn>
QAction *addItem(QMenu &menu, const QString &text, Fn fn, const QKeySequence &shortcut = {})
{
    QAction *a = menu.addAction(text);
    a->setShortcut(shortcut);
    QObject::connect(a, &QAction::triggered, &menu, fn);
    return a;
}
} // namespace

Canvas::Canvas(Workspace *ws, QWidget *parent) : QGraphicsView(ws, parent), m_ws(ws)
{
    setObjectName(QStringLiteral("blockCanvas"));
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(ScrollHandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(FullViewportUpdate);
    setFrameShape(QFrame::NoFrame);
    setAcceptDrops(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scale(0.95, 0.95);

    connect(ws, &Workspace::fieldEditRequested, this, &Canvas::editField);

    m_zoomBar = new QWidget(this);
    m_zoomBar->setObjectName(QStringLiteral("zoomBar"));
    auto *lay = new QHBoxLayout(m_zoomBar);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(2);
    const auto addButton = [&](const QString &text, const QString &tip, auto fn) {
        auto *b = new QToolButton(m_zoomBar);
        b->setText(text);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, fn);
        lay->addWidget(b);
    };
    addButton(QStringLiteral("−"), tr("Zoom out"), [this] { zoomBy(1 / 1.15); });
    addButton(QStringLiteral("⟲"), tr("Reset zoom"), [this] { resetZoom(); });
    addButton(QStringLiteral("+"), tr("Zoom in"), [this] { zoomBy(1.15); });
    m_zoomBar->adjustSize();
}

void Canvas::zoomBy(qreal factor)
{
    const qreal current = transform().m11();
    const qreal target = std::clamp(current * factor, MinZoom, MaxZoom);
    scale(target / current, target / current);
}

void Canvas::resetZoom()
{
    const qreal current = transform().m11();
    scale(0.95 / current, 0.95 / current);
}

void Canvas::scrollToContent()
{
    QRectF content;
    for (Block *b : m_ws->topBlocks()) content |= b->sceneBoundingRect();
    if (content.isNull()) {
        centerOn(0, 0);
        return;
    }
    const qreal s = transform().m11();
    const QPointF topLeft = content.topLeft() - QPointF(40, 40);
    centerOn(topLeft + QPointF(viewport()->width() / 2.0 / s, viewport()->height() / 2.0 / s));
}

void Canvas::drawBackground(QPainter *p, const QRectF &rect)
{
    p->fillRect(rect, QColor(0x0f, 0x14, 0x1c));
    const qreal step = transform().m11() < 0.6 ? GridStep * 2 : GridStep;
    const qreal left = std::floor(rect.left() / step) * step;
    const qreal top = std::floor(rect.top() / step) * step;
    QPen pen(QColor(0x24, 0x32, 0x46), 2.6);
    pen.setCapStyle(Qt::RoundCap);
    p->setPen(pen);
    QVector<QPointF> dots;
    for (qreal x = left; x < rect.right(); x += step)
        for (qreal y = top; y < rect.bottom(); y += step) dots.append(QPointF(x, y));
    p->drawPoints(dots.constData(), int(dots.size()));
}

void Canvas::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        zoomBy(e->angleDelta().y() > 0 ? 1.12 : 1 / 1.12);
        e->accept();
        return;
    }
    QGraphicsView::wheelEvent(e);
}

void Canvas::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    positionOverlay();
}

void Canvas::positionOverlay()
{
    const QRect vp = viewport()->geometry();
    m_zoomBar->move(vp.right() - m_zoomBar->width() - 14, vp.bottom() - m_zoomBar->height() - 14);
    m_zoomBar->raise();
}

Block *Canvas::blockAt(const QPoint &viewPos) const
{
    for (QGraphicsItem *it = itemAt(viewPos); it; it = it->parentItem())
        if (auto *b = qgraphicsitem_cast<Block *>(it)) return b;
    return nullptr;
}

void Canvas::keyPressEvent(QKeyEvent *e)
{
    const auto selected = m_ws->selectedItems();
    Block *sel = selected.isEmpty() ? nullptr : qgraphicsitem_cast<Block *>(selected.first());
    if (sel && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
        m_ws->deleteBlock(sel);
        return;
    }
    if (sel && e->key() == Qt::Key_D && (e->modifiers() & Qt::ControlModifier)) {
        m_ws->duplicate(sel);
        return;
    }
    QGraphicsView::keyPressEvent(e);
}

void Canvas::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    if (Block *b = blockAt(e->pos())) {
        m_ws->clearSelection();
        b->setSelected(true);
        addItem(menu, tr("Duplicate"), [this, b] { m_ws->duplicate(b); }, QKeySequence(Qt::CTRL | Qt::Key_D));
        addItem(menu, tr("Delete block"), [this, b] { m_ws->deleteBlock(b); }, QKeySequence(QKeySequence::Delete));
    } else {
        addItem(menu, tr("Undo"), [this] { emit undoRequested(); }, QKeySequence(QKeySequence::Undo));
        addItem(menu, tr("Redo"), [this] { emit redoRequested(); }, QKeySequence(QKeySequence::Redo));
        menu.addSeparator();
        addItem(menu, tr("Clean up blocks"), [this] {
            m_ws->cleanUp();
            scrollToContent();
        });
    }
    menu.exec(e->globalPos());
}

void Canvas::mousePressEvent(QMouseEvent *e)
{
    if (m_editor) closeEditor(true);
    QGraphicsView::mousePressEvent(e);
}

// ── Drops from the palette ────────────────────────────────

void Canvas::dragEnterEvent(QDragEnterEvent *e)
{
    if (!e->mimeData()->hasFormat(QLatin1String(BlockMimeType))) {
        e->ignore();
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(e->mimeData()->data(QLatin1String(BlockMimeType))).object();
    Block *b = Block::fromJson(o.value(QStringLiteral("block")).toObject());
    if (!b) {
        e->ignore();
        return;
    }
    m_dropGrab = QPointF(o.value(QStringLiteral("grabX")).toDouble(), o.value(QStringLiteral("grabY")).toDouble());
    m_ws->addTree(b, mapToScene(e->position().toPoint()) - m_dropGrab);
    m_ws->beginDrag(b, m_dropGrab);
    e->acceptProposedAction();
}

void Canvas::dragMoveEvent(QDragMoveEvent *e)
{
    if (!m_ws->isDragging()) {
        e->ignore();
        return;
    }
    const QPoint pos = e->position().toPoint();
    m_ws->dragTo(mapToScene(pos), mapToGlobal(pos));
    e->acceptProposedAction();
}

void Canvas::dragLeaveEvent(QDragLeaveEvent *e)
{
    m_ws->cancelDrag();
    e->accept();
}

void Canvas::dropEvent(QDropEvent *e)
{
    if (!m_ws->isDragging()) return;
    const QPoint pos = e->position().toPoint();
    m_ws->dragTo(mapToScene(pos), mapToGlobal(pos));
    m_ws->endDrag(mapToGlobal(pos));
    e->acceptProposedAction();
}

// ── Field editing ─────────────────────────────────────────

void Canvas::editField(Block *b, const QString &field, const QPoint &globalPos)
{
    const ArgDef *a = b->def()->arg(field);
    if (!a) return;

    if (a->kind == ArgKind::Dropdown) {
        QMenu menu(this);
        for (const auto &opt : a->options) {
            QAction *act = menu.addAction(opt.first);
            act->setCheckable(true);
            act->setChecked(opt.second == b->field(field));
            connect(act, &QAction::triggered, this, [this, b, field, v = opt.second] {
                b->setField(field, v);
                m_ws->commit();
            });
        }
        menu.exec(globalPos);
        return;
    }

    if (a->kind == ArgKind::Variable) {
        QMenu menu(this);
        for (const QString &v : m_ws->variables()) {
            QAction *act = menu.addAction(v);
            act->setCheckable(true);
            act->setChecked(v == b->field(field));
            connect(act, &QAction::triggered, this, [this, b, field, v] {
                b->setField(field, v);
                m_ws->commit();
            });
        }
        menu.addSeparator();
        const QString current = b->field(field);
        if (!current.isEmpty()) {
            addItem(menu, tr("Rename variable…"), [this, current] {
                bool ok = false;
                const QString name =
                    QInputDialog::getText(this, tr("Rename variable"), tr("New name for “%1”:").arg(current), QLineEdit::Normal, current, &ok);
                if (ok) m_ws->renameVariable(current, name);
            });
        }
        addItem(menu, tr("New variable…"), [this, b, field] {
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("New variable"), tr("Variable name:"), QLineEdit::Normal, {}, &ok);
            if (ok && m_ws->addVariable(name)) {
                b->setField(field, name.trimmed());
                m_ws->commit();
            }
        });
        menu.exec(globalPos);
        return;
    }

    // Number / text: an inline editor over the field.
    QRectF fieldRect;
    for (const SlotInfo &s : b->valueSlots())
        if (s.name == field) fieldRect = s.rect;
    if (fieldRect.isNull()) return;
    const QRect r = mapFromScene(b->mapRectToScene(fieldRect)).boundingRect();

    closeEditor(true);
    m_editBlock = b;
    m_editField = field;
    m_editor = new QLineEdit(viewport());
    m_editor->setObjectName(QStringLiteral("fieldEditor"));
    m_editor->setText(b->field(field));
    m_editor->setAlignment(Qt::AlignCenter);
    QFont f = m_editor->font();
    f.setPixelSize(std::max(10, int(12 * transform().m11())));
    f.setWeight(QFont::DemiBold);
    m_editor->setFont(f);
    if (a->kind == ArgKind::Number)
        m_editor->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^-?\\d{0,6}([.,]\\d{0,3})?$")), m_editor));
    QRect geo = r;
    geo.setWidth(std::max(r.width(), 64));
    m_editor->setGeometry(geo);
    // Fires on Enter and when focus moves away.
    connect(m_editor, &QLineEdit::editingFinished, this, [this] { closeEditor(true); });
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), m_editor);
    connect(esc, &QShortcut::activated, this, [this] { closeEditor(false); });
    m_editor->show();
    m_editor->setFocus();
    m_editor->selectAll();
}

void Canvas::closeEditor(bool apply)
{
    if (!m_editor) return;
    QLineEdit *ed = m_editor;
    m_editor = nullptr;
    if (apply && m_editBlock) {
        QString v = ed->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.'));
        const ArgDef *a = m_editBlock->def()->arg(m_editField);
        if (a && a->kind == ArgKind::Number && (v.isEmpty() || v == QLatin1String("-"))) v = QStringLiteral("0");
        if (v != m_editBlock->field(m_editField)) {
            m_editBlock->setField(m_editField, v);
            m_ws->commit();
        }
    }
    ed->deleteLater();
    setFocus();
}

} // namespace blocks
