#pragma once

#include "Mixer.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <array>

namespace drive {

enum class PadButton { A, B, X, Y, LB, RB, Back, Start, LS, RS, Up, Down, Left, Right, Count };

struct PadSnapshot {
    bool connected = false;
    int slot = -1; // XInput user index 0..3
    ControlInput input;
    std::array<bool, int(PadButton::Count)> buttons{};
    bool pressed(PadButton b) const { return buttons[int(b)]; }
};

/**
 * Polls wired XInput controllers (Xbox-style pads) at ~125 Hz.
 * Empty slots are rescanned only once a second, since XInputGetState on an
 * empty slot is slow.
 */
class Gamepad : public QObject {
    Q_OBJECT
public:
    static Gamepad &instance();

    const PadSnapshot &snapshot() const { return m_snapshot; }
    /** Connected XInput slots, for the picker. */
    QList<int> connectedSlots() const;
    void setPreferredSlot(int slot) { m_preferred = slot; }

signals:
    void updated(const drive::PadSnapshot &snap);
    /** Fired once per button press (rising edge). */
    void buttonPressed(drive::PadButton button);
    void connectionChanged(bool connected);

private:
    Gamepad();
    void poll();

    QTimer m_timer;
    QElapsedTimer m_sinceScan;
    std::array<bool, 4> m_slotConnected{};
    int m_preferred = -1;
    PadSnapshot m_snapshot;
};

} // namespace drive
