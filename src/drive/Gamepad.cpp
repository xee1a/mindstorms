#include "Gamepad.h"

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <Xinput.h>
#include <dinput.h>

#include <algorithm>
#include <cstdlib>

namespace drive {

namespace {

constexpr int PollIntervalMs = 8;
constexpr int RescanIntervalMs = 1000;

double stick(SHORT v) { return v < 0 ? v / 32768.0 : v / 32767.0; }
double axis(LONG v) { return std::clamp(v < 0 ? v / 32768.0 : v / 32767.0, -1.0, 1.0); }

/**
 * Button numbers on generic DirectInput pads (DragonRise layout, used by most cheap USB pads):
 * 1 top, 2 right, 3 bottom, 4 left face button, 5/6 L1/R1, 7/8 L2/R2, 9 Select, 10 Start, 11/12 stick clicks.
 */
enum DiButton { DiY, DiB, DiA, DiX, DiL1, DiR1, DiL2, DiR2, DiSelect, DiStart, DiL3, DiR3 };

BOOL CALLBACK firstDevice(LPCDIDEVICEINSTANCEW inst, LPVOID out)
{
    *static_cast<GUID *>(out) = inst->guidInstance;
    return DIENUM_STOP;
}

} // namespace

struct Gamepad::DirectPad {
    IDirectInput8W *di = nullptr;
    IDirectInputDevice8W *device = nullptr;

    ~DirectPad()
    {
        release();
        if (di) di->Release();
    }
    void release()
    {
        if (!device) return;
        device->Unacquire();
        device->Release();
        device = nullptr;
    }
    bool open()
    {
        if (!di && FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                             reinterpret_cast<void **>(&di), nullptr)))
            return false;
        GUID guid{};
        if (FAILED(di->EnumDevices(DI8DEVCLASS_GAMECTRL, firstDevice, &guid, DIEDFL_ATTACHEDONLY)) || guid == GUID{}) return false;
        if (FAILED(di->CreateDevice(guid, &device, nullptr))) return false;
        if (FAILED(device->SetDataFormat(&c_dfDIJoystick2))) {
            release();
            return false;
        }
        // Every axis reports -32768..32767, like XInput sticks.
        DIPROPRANGE range{};
        range.diph.dwSize = sizeof(range);
        range.diph.dwHeaderSize = sizeof(range.diph);
        range.diph.dwHow = DIPH_DEVICE;
        range.lMin = -32768;
        range.lMax = 32767;
        device->SetProperty(DIPROP_RANGE, &range.diph);
        device->Acquire();
        return true;
    }
    bool read(DIJOYSTATE2 &st)
    {
        if (!device) return false;
        if (FAILED(device->Poll())) device->Acquire();
        HRESULT hr = device->GetDeviceState(sizeof(st), &st);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            device->Acquire();
            hr = device->GetDeviceState(sizeof(st), &st);
        }
        if (FAILED(hr)) {
            release(); // unplugged
            return false;
        }
        return true;
    }
};

Gamepad &Gamepad::instance()
{
    static Gamepad pad;
    return pad;
}

Gamepad::Gamepad() : m_direct(std::make_unique<DirectPad>())
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(PollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &Gamepad::poll);
    m_timer.start();
}

Gamepad::~Gamepad() = default;

bool Gamepad::pollDirectInput(PadSnapshot &snap, bool rescan)
{
    if (!m_direct->device && !(rescan && m_direct->open())) return false;
    DIJOYSTATE2 st{};
    if (!m_direct->read(st)) return false;
    snap.connected = true;
    snap.directInput = true;
    snap.slot = 4;
    snap.input.leftX = axis(st.lX);
    snap.input.leftY = -axis(st.lY); // DirectInput Y grows downwards
    snap.input.rightX = axis(st.lZ);
    snap.input.rightY = -axis(st.lRz);
    const auto down = [&](int b) { return (st.rgbButtons[b] & 0x80) != 0; };
    snap.input.leftTrigger = down(DiL2) ? 1.0 : 0.0;
    snap.input.rightTrigger = down(DiR2) ? 1.0 : 0.0;
    snap.input.leftBumper = down(DiL1);
    snap.input.rightBumper = down(DiR1);
    // POV hat in hundredths of a degree, clockwise from up; 0xFFFF in the low word means centred.
    const DWORD pov = st.rgdwPOV[0];
    const bool centred = LOWORD(pov) == 0xFFFF;
    const auto towards = [&](DWORD angle) {
        const int d = std::abs(int(pov) - int(angle)) % 36000;
        return !centred && std::min(d, 36000 - d) <= 6750;
    };
    const std::array<bool, int(PadButton::Count)> buttons{
        down(DiA), down(DiB), down(DiX), down(DiY), down(DiL1), down(DiR1), down(DiSelect), down(DiStart),
        down(DiL3), down(DiR3), towards(0), towards(18000), towards(27000), towards(9000)};
    snap.buttons = buttons;
    return true;
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
    } else {
        pollDirectInput(snap, rescan);
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
