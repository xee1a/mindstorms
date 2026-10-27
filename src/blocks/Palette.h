#pragma once

#include "BlockDefs.h"

#include <QGraphicsView>
#include <QList>
#include <QWidget>

class QButtonGroup;
class QGraphicsScene;

namespace blocks {

class Block;

/** The scrolling list of template blocks; dragging one out starts a block drag. */
class PaletteView : public QGraphicsView {
    Q_OBJECT
public:
    explicit PaletteView(QWidget *parent = nullptr);

signals:
    void makeVariableClicked();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    Block *templateAt(const QPoint &viewPos) const;
    QPoint m_pressPos;
    Block *m_pressed = nullptr;
};

/** Category list on the left, block flyout on the right. Dropping a block back here deletes it. */
class Palette : public QWidget {
    Q_OBJECT
public:
    explicit Palette(QWidget *parent = nullptr);

    /** Rebuilds the flyout; variables get their own reporter blocks. */
    void rebuild(const QStringList &variables);

signals:
    void makeVariableClicked();

private:
    void scrollToCategory(int row);
    void syncCategoryToScroll();

    QButtonGroup *m_categories;
    PaletteView *m_view;
    QGraphicsScene *m_scene;
    QList<qreal> m_headingY; // per category, in scene coordinates
    bool m_scrollingProgrammatically = false;
};

} // namespace blocks
