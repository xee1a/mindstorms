#pragma once

#include "drive/Gamepad.h"
#include "drive/Mixer.h"
#include "nxt/Brick.h"

#include <QSet>
#include <QTimer>
#include <QWidget>
#include <array>

class QButtonGroup;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;

namespace ui {
class ControllerWidget;
class MotorGauge;
} // namespace ui

class DriveView : public QWidget {
    Q_OBJECT
public:
    explicit DriveView(QWidget *parent = nullptr);

signals:
    void message(const QString &text, const char *kind);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    QWidget *buildControllerCard();
    QWidget *buildMotorsCard();
    QWidget *buildSetupCard();
    void loadConfig();
    void saveConfig();
    void setArmed(bool on);
    void emergencyStop();
    void tick();
    void onPadButton(drive::PadButton b);
    void onPadConnection(bool connected);
    void onBrick(const nxt::BrickState &s);
    void updateRoles();
    drive::ControlInput keyboardInput() const;

    drive::DriveConfig m_cfg;
    bool m_armed = false;
    QSet<int> m_keys;
    QTimer m_tick;

    ui::ControllerWidget *m_controller = nullptr;
    QLabel *m_padName = nullptr;
    QComboBox *m_padPick = nullptr;
    QPushButton *m_arm = nullptr;
    QLabel *m_linkNote = nullptr;
    std::array<ui::MotorGauge *, 3> m_gauges{};
    std::array<QLabel *, 3> m_roles{};
    std::array<QLabel *, 3> m_rotation{};
    std::array<QWidget *, 3> m_motorTiles{};
    std::array<QComboBox *, 4> m_sensorKind{};
    std::array<QLabel *, 4> m_sensorValue{};
    std::array<QProgressBar *, 4> m_sensorBar{};
    QButtonGroup *m_modeGroup = nullptr;
    QLabel *m_modeHint = nullptr;
    QComboBox *m_left = nullptr;
    QComboBox *m_right = nullptr;
    QComboBox *m_aux = nullptr;
};
