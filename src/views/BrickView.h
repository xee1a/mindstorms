#pragma once

#include "nxt/Brick.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace ui {
class BatteryWidget;
class PhotoWidget;
} // namespace ui

class BrickView : public QWidget {
    Q_OBJECT
public:
    explicit BrickView(QWidget *parent = nullptr);

signals:
    void message(const QString &text, const char *kind);

private:
    void onBrick(const nxt::BrickState &s);

    ui::PhotoWidget *m_illustration;
    QLabel *m_title;
    QLabel *m_subtitle;
    QLabel *m_usbHint;
    QWidget *m_facts;
    QLabel *m_firmware;
    QLabel *m_flash;
    QLabel *m_bluetooth;
    QWidget *m_batteryRow;
    ui::BatteryWidget *m_battery;
    QLabel *m_batteryText;
    QPushButton *m_connect;
    QPushButton *m_disconnect;
    QWidget *m_quick;
};
