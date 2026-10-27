#pragma once

#include <QGraphicsView>
#include <QPointer>

class QLineEdit;
class QToolButton;

namespace blocks {

class Block;
class Workspace;

inline constexpr char BlockMimeType[] = "application/x-nxtstudio-block";

/** Zoomable, pannable view of a Workspace that accepts blocks dragged in from the palette. */
class Canvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit Canvas(Workspace *ws, QWidget *parent = nullptr);

    void zoomBy(qreal factor);
    void resetZoom();
    void scrollToContent();

signals:
    void undoRequested();
    void redoRequested();

protected:
    void drawBackground(QPainter *p, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void editField(Block *b, const QString &field, const QPoint &globalPos);
    void closeEditor(bool apply);
    void positionOverlay();
    Block *blockAt(const QPoint &viewPos) const;

    Workspace *m_ws;
    QPointer<QLineEdit> m_editor;
    QPointer<Block> m_editBlock;
    QString m_editField;
    QWidget *m_zoomBar = nullptr;
    QPointF m_dropGrab;
};

} // namespace blocks
