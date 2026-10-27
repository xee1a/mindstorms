#include "Gamepad.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>

namespace drive {

namespace {

constexpr int PollIntervalMs = 8;
constexpr int RescanIntervalMs = 1000;

double stick(SHORT v) { return v < 0 ? v / 32768.0 : v / 32767.0; }

} // namespace

Gamepad &Gamepad::instance()
{
    static Gamepad pad;
    return pad;
}

Gamepad::Gamepad()
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(PollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &Gamepad::poll);
    m_timer.start();
}

QList<int> Gamepad::connectedSlots() const
{
    QList<int> out;
    for (int i = 0; i < 4; ++i)
        if (m_slotConnected[i]) out << i;
    return out;
}

void Gamepad::poll()
{
    const bool rescan = !m_sinceScan.isValid() || m_sinceScan.elapsed() > RescanIntervalMs;
    if (rescan) m_sinceScan.restart();

    XINPUT_STATE chosen{};
    int chosenSlot = -1;
    for (DWORD i = 0; i < 4; ++i) {
        if (!m_slotConnected[i] && !rescan) continue;
        XINPUT_STATE st{};
        m_slotConnected[i] = XInputGetState(i, &st) == ERROR_SUCCESS;
        if (!m_slotConnected[i]) continue;
        if (chosenSlot < 0 || int(i) == m_preferred) {
            chosen = st;
            chosenSlot = int(i);
        }
    }

    const bool wasConnected = m_snapshot.connected;
    PadSnapshot snap;
    if (chosenSlot >= 0) {
        const XINPUT_GAMEPAD &g = chosen.Gamepad;
        snap.connected = true;
        snap.slot = chosenSlot;
        snap.input.leftX = stick(g.sThumbLX);
        snap.input.leftY = stick(g.sThumbLY); // XInput Y is already up-positive
        snap.input.rightX = stick(g.sThumbRX);
        snap.input.rightY = stick(g.sThumbRY);
        snap.input.leftTrigger = g.bLeftTrigger / 255.0;
        snap.input.rightTrigger = g.bRightTrigger / 255.0;
        const auto down = [&](WORD mask) { return (g.wButtons & mask) != 0; };
        snap.input.leftBumper = down(XINPUT_GAMEPAD_LEFT_SHOULDER);
        snap.input.rightBumper = down(XINPUT_GAMEPAD_RIGHT_SHOULDER);
        const std::array<WORD, int(PadButton::Count)> masks{
            XINPUT_GAMEPAD_A,           XINPUT_GAMEPAD_B,          XINPUT_GAMEPAD_X,         XINPUT_GAMEPAD_Y,
            XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
            XINPUT_GAMEPAD_LEFT_THUMB,  XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_DPAD_UP,  XINPUT_GAMEPAD_DPAD_DOWN,
            XINPUT_GAMEPAD_DPAD_LEFT,   XINPUT_GAMEPAD_DPAD_RIGHT};
        for (int b = 0; b < int(PadButton::Count); ++b) snap.buttons[b] = down(masks[b]);
    }

    const PadSnapshot previous = m_snapshot;
    m_snapshot = snap;
    if (snap.connected != wasConnected) emit connectionChanged(snap.connected);
    if (snap.connected && previous.slot == snap.slot) {
        for (int b = 0; b < int(PadButton::Count); ++b)
            if (snap.buttons[b] && !previous.buttons[b]) emit buttonPressed(PadButton(b));
    }
    emit updated(m_snapshot);
}

} // namespace drive
