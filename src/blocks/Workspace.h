#pragma once

#include "Block.h"

#include <QGraphicsScene>
#include <QJsonObject>
#include <QStringList>
#include <functional>

class QGraphicsPathItem;

namespace blocks {

/** The editable canvas: owns top-level stacks, snapping, drag & drop and (de)serialisation. */
class Workspace : public QGraphicsScene {
    Q_OBJECT
public:
    explicit Workspace(QObject *parent = nullptr);

    QJsonObject toJson() const;
    void loadJson(const QJsonObject &obj);
    void clearBlocks();
    /** Adds a block tree built with Block::fromJson at a scene position. */
    void addTree(Block *b, const QPointF &scenePos);

    QList<Block *> topBlocks() const;
    int blockCount() const;

    const QStringList &variables() const { return m_variables; }
    bool addVariable(const QString &name);
    void renameVariable(const QString &from, const QString &to);

    // Drag protocol, driven by Block mouse events and by palette drops.
    void beginDrag(Block *b, const QPointF &grabOffset);
    void dragTo(const QPointF &scenePos, const QPoint &globalPos);
    /** Finishes the drag: snaps, or deletes when released over the delete area. */
    void endDrag(const QPoint &globalPos);
    /** Drops a block that is being dragged in from the palette. */
    void cancelDrag();
    bool isDragging() const { return m_drag != nullptr; }

    /** Removes one block; the blocks below it move up to take its place. */
    void deleteBlock(Block *b);
    Block *duplicate(Block *b);
    void cleanUp();

    void requestFieldEdit(Block *b, const QString &field, const QPoint &globalPos);
    /** Records an edit (for undo and autosave). */
    void commit() { emit changed(); }

    /** Returns true when a global screen position is over the palette (drop there = delete). */
    std::function<bool(const QPoint &)> isOverDeleteArea;

signals:
    void changed();
    void fieldEditRequested(blocks::Block *block, const QString &field, const QPoint &globalPos);
    void variablesChanged();

private:
    struct Target {
        enum Kind { None, Next, Statement, Input, Above } kind = None;
        Block *block = nullptr;
        QString name;
        qreal distance = 1e9;
    };

    Target findTarget(Block *dragged) const;
    void showMarker(const Target &t, Block *dragged);
    void bump(Block *b, const QPointF &near);

    QStringList m_variables;
    Block *m_drag = nullptr;
    QPointF m_grab;
    Target m_target;
    bool m_overDelete = false;
    QGraphicsPathItem *m_marker = nullptr;
};

} // namespace blocks
