#pragma once

#include "Mixer.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <array>
#include <memory>

namespace drive {

enum class PadButton { A, B, X, Y, LB, RB, Back, Start, LS, RS, Up, Down, Left, Right, Count };

struct PadSnapshot {
    bool connected = false;
    int slot = -1; // XInput user index 0..3; 4 for a DirectInput pad
    bool directInput = false;
    ControlInput input;
    std::array<bool, int(PadButton::Count)> buttons{};
    bool pressed(PadButton b) const { return buttons[int(b)]; }
};

/**
 * Polls wired controllers at ~125 Hz. XInput (Xbox-style) pads come first; when there
 * is none, the first DirectInput game controller is used instead (generic USB pads,
 * e.g. the DragonRise chip in many cheap "PC/PS3" gamepads).
 * Empty slots are rescanned only once a second, since probing them is slow.
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
    ~Gamepad() override;
    void poll();
    /** Reads the DirectInput pad into `snap`; false when there isn't one. */
    bool pollDirectInput(PadSnapshot &snap, bool rescan);

    QTimer m_timer;
    QElapsedTimer m_sinceScan;
    std::array<bool, 4> m_slotConnected{};
    int m_preferred = -1;
    PadSnapshot m_snapshot;
    struct DirectPad;
    std::unique_ptr<DirectPad> m_direct;
};

} // namespace drive
