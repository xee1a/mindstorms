#pragma once

#include <QJsonObject>
#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QUndoStack;

namespace blocks {
class Canvas;
class Palette;
class Workspace;
} // namespace blocks

class ProgramView : public QWidget {
    Q_OBJECT
public:
    explicit ProgramView(QWidget *parent = nullptr);

    void newProject();
    void openProject();
    void saveProject();

signals:
    void message(const QString &text, const char *kind);

protected:
    void showEvent(QShowEvent *e) override;

private:
    void load(const QJsonObject &workspace, bool resetHistory);
    void onChanged();
    void autosave();
    void updateCount();
    QJsonObject starter() const;

    blocks::Workspace *m_ws;
    blocks::Canvas *m_canvas;
    blocks::Palette *m_palette;
    QLineEdit *m_name;
    QLabel *m_count;
    QUndoStack *m_undo;
    QJsonObject m_snapshot;
    QTimer m_autosave;
    bool m_restoring = false;
    bool m_firstShow = true;
};
