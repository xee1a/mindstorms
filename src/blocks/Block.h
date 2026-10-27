#pragma once

#include "BlockDefs.h"

#include <QGraphicsObject>
#include <QJsonObject>
#include <QPainterPath>
#include <functional>

namespace blocks {

class Workspace;

/** Where a value block can be plugged in, in the host block's local coordinates. */
struct SlotInfo {
    QString name;
    ArgKind kind;
    QRectF rect;
};

/**
 * One block on a canvas. Blocks nest as QGraphicsItem children: the next block in a
 * stack, the blocks inside a C-shaped mouth and the values plugged into slots are all
 * children of this block, so moving a block moves everything attached below/inside it.
 */
class Block : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 1 };
    enum class Link { None, Next, Input, Statement };

    explicit Block(const BlockDef *def, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }
    const BlockDef *def() const { return m_def; }

    QString field(const QString &name) const { return m_fields.value(name); }
    void setField(const QString &name, const QString &value);
    const QMap<QString, QString> &fields() const { return m_fields; }

    Block *input(const QString &name) const { return m_inputs.value(name); }
    Block *statement(const QString &name) const { return m_statements.value(name); }
    Block *nextBlock() const { return m_next; }
    Block *parentBlock() const;
    Link link() const { return m_link; }
    QString linkName() const { return m_linkName; }

    /** Attach `b` (already detached) below / inside / into this block. Call relayout() afterwards. */
    void attachNext(Block *b);
    void attachStatement(const QString &name, Block *b);
    void attachInput(const QString &name, Block *b);
    /** Detaches from the parent block, keeping the scene position. Returns the old parent. */
    Block *detach();

    Block *root();
    Block *lastInChain();
    /** Visits this block and everything attached to it. */
    void forEach(const std::function<void(Block *)> &fn);

    /** Recomputes geometry for the whole tree this block belongs to. */
    void relayout();

    qreal width() const { return m_width; }
    qreal bodyHeight() const { return m_height; }
    /** Height of this block plus every block stacked below it. */
    qreal chainHeight() const;
    QPointF statementOrigin(const QString &name) const;
    QList<QString> statementNames() const;
    const QList<SlotInfo> &valueSlots() const { return m_slots; }

    bool isTemplate() const { return m_template; }
    void setTemplate(bool t) { m_template = t; }

    QJsonObject toJson() const;
    /** Builds a block tree. Unknown block types are skipped (returns nullptr). */
    static Block *fromJson(const QJsonObject &obj);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *opt, QWidget *w) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override;

private:
    struct Elem {
        enum Kind { Label, Arg } kind = Label;
        QString text;
        int arg = -1;
        QRectF rect;
    };
    struct Mouth {
        QString name;
        qreal y = 0;
        qreal height = 0;
    };

    void layoutTree();
    QSizeF argSize(const ArgDef &a) const;
    QString displayText(const ArgDef &a) const;
    void buildPath();
    Workspace *workspace() const;
    const Elem *editableAt(const QPointF &pos) const;

    const BlockDef *m_def;
    QMap<QString, QString> m_fields;
    QMap<QString, Block *> m_inputs;
    QMap<QString, Block *> m_statements;
    Block *m_next = nullptr;
    Link m_link = Link::None;
    QString m_linkName;
    bool m_template = false;

    qreal m_width = 0;
    qreal m_height = 0;
    QList<QList<Elem>> m_rows; // inline rows only
    QList<qreal> m_rowY;
    QList<Mouth> m_mouths;
    QList<SlotInfo> m_slots;
    QPainterPath m_path;

    QPointF m_pressPos;
    bool m_pressed = false;
    bool m_dragging = false;
};

} // namespace blocks
