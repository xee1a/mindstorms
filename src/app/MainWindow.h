#pragma once

#include "nxt/Brick.h"

#include <QMainWindow>

class QButtonGroup;
class QPushButton;
class QStackedWidget;
class QLabel;

namespace ui {
class Toast;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    QWidget *buildHeader();
    void showView(int index);
    void onBrick(const nxt::BrickState &s);
    void switchLanguage(const QString &code);

    QStackedWidget *m_stack;
    QButtonGroup *m_tabs;
    QLabel *m_padChip;
    QPushButton *m_brickChip;
    ui::Toast *m_toast;
    int m_lastErrorSerial = 0;
};
