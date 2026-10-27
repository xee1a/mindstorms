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
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    QWidget *buildHeader();
    void showView(int index);
    void onBrick(const nxt::BrickState &s);
    void openSettings();
    void restart();

    QStackedWidget *m_stack;
    QButtonGroup *m_tabs;
    QLabel *m_padChip;
    QPushButton *m_brickChip;
    ui::Toast *m_toast;
    QWidget *m_tabBar = nullptr;
    nxt::LinkStatus m_lastStatus = nxt::LinkStatus::Disconnected;
    int m_lastErrorSerial = 0;
};
