#pragma once

#include "Transport.h"

#include <memory>

namespace nxt {

/** Talks to an NXT bound to Windows' WinUSB driver (installed once with Zadig). */
class WinUsbTransport : public Transport {
public:
    enum class Presence {
        NotFound,    // no NXT plugged in (or it's switched off)
        NeedsDriver, // NXT found, but bound to Fantom or no driver at all
        Ready,       // NXT found with WinUSB
    };

    struct Probe {
        Presence presence = Presence::NotFound;
        /** Driver service currently bound, e.g. "WinUSB", or empty. */
        QString service;
    };

    static Probe probe();
    /** Opens the first NXT found. Throws std::runtime_error with a user-facing reason. */
    static std::unique_ptr<WinUsbTransport> open();

    ~WinUsbTransport() override;
    bool isDemo() const override { return false; }
    QString label() const override { return QStringLiteral("NXT (USB)"); }
    void write(const QByteArray &packet) override;
    QByteArray read(int timeoutMs) override;

private:
    WinUsbTransport() = default;
    void *m_file = nullptr;   // HANDLE
    void *m_winusb = nullptr; // WINUSB_INTERFACE_HANDLE
    unsigned char m_outPipe = 0x01;
    unsigned char m_inPipe = 0x82;
    int m_currentTimeout = -1;
};

} // namespace nxt
