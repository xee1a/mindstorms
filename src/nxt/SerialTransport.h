#pragma once

#include "Transport.h"

#include <QStringList>
#include <memory>

class QSerialPort;

namespace nxt {

/**
 * Talks to an NXT over Bluetooth through the serial (COM) port Windows creates once the
 * brick is paired. Bluetooth packets carry a two-byte little-endian length prefix.
 */
class SerialTransport : public Transport {
public:
    /** Serial ports that could be a paired NXT (Bluetooth ones first). */
    static QStringList ports();
    /** Opens `port`, e.g. "COM5". Throws std::runtime_error with a user-facing reason. */
    static std::unique_ptr<SerialTransport> open(const QString &port);

    ~SerialTransport() override;
    QString label() const override;
    void write(const QByteArray &packet) override;
    QByteArray read(int timeoutMs) override;

private:
    SerialTransport() = default;
    std::unique_ptr<QSerialPort> m_port;
    QByteArray m_buffer;
};

} // namespace nxt
