#include "DemoTransport.h"
#include "Protocol.h"

#include <QtEndian>
#include <algorithm>
#include <cmath>

namespace nxt {

namespace {

QByteArray ok(uint8_t op, QByteArray payload = {})
{
    QByteArray b;
    b.append(static_cast<char>(CommandType::Reply));
    b.append(static_cast<char>(op));
    b.append('\0');
    b.append(payload);
    return b;
}

void putU16(QByteArray &b, int at, uint16_t v) { qToLittleEndian(v, b.data() + at); }
void putS16(QByteArray &b, int at, int16_t v) { qToLittleEndian(v, b.data() + at); }
void putS32(QByteArray &b, int at, int32_t v) { qToLittleEndian(v, b.data() + at); }
void putU32(QByteArray &b, int at, uint32_t v) { qToLittleEndian(v, b.data() + at); }

} // namespace

DemoTransport::DemoTransport() { m_clock.start(); }

void DemoTransport::write(const QByteArray &packet)
{
    tick();
    QByteArray reply = handle(packet);
    if (expectsReply(packet)) m_replies.push_back(std::move(reply));
}

QByteArray DemoTransport::read(int)
{
    if (m_replies.empty()) throw TimeoutError();
    QByteArray r = std::move(m_replies.front());
    m_replies.pop_front();
    return r;
}

void DemoTransport::tick()
{
    const qint64 now = m_clock.elapsed();
    const double dt = (now - m_lastTickMs) / 1000.0;
    m_lastTickMs = now;
    // An NXT motor at full power turns at roughly 170 rpm, about 1000 deg/s.
    for (auto &m : m_motors)
        if (m.running) m.tacho += m.power * 10.0 * dt;
}

QByteArray DemoTransport::handle(const QByteArray &p)
{
    const auto op = static_cast<uint8_t>(p.at(1));
    const auto arg = [&](int i) { return i < p.size() ? static_cast<uint8_t>(p.at(i)) : uint8_t(0); };
    const double t = m_clock.elapsed() / 1000.0;

    switch (op) {
    case DirectOp::SetOutputState: {
        const int port = arg(2);
        for (int i = 0; i < 3; ++i) {
            if (port != 0xff && port != i) continue;
            m_motors[i].power = static_cast<int8_t>(arg(3));
            m_motors[i].running = (arg(4) & OutputMode::MotorOn) && arg(7) != 0;
        }
        return ok(op);
    }
    case DirectOp::SetInputMode:
        if (arg(2) < 4) m_sensorTypes[arg(2)] = arg(3);
        return ok(op);
    case DirectOp::GetInputValues: {
        const int port = arg(2) & 3;
        const int type = m_sensorTypes[port];
        const int scaled = simulateSensor(type, t, port);
        QByteArray b(16, '\0');
        b[0] = char(CommandType::Reply);
        b[1] = char(op);
        b[3] = char(port);
        b[4] = 1;
        b[6] = char(type);
        putU16(b, 8, uint16_t(scaled * 10));
        putU16(b, 10, uint16_t(scaled * 10));
        putS16(b, 12, int16_t(scaled));
        return b;
    }
    case DirectOp::GetOutputState: {
        const int port = arg(2) % 3;
        const Motor &m = m_motors[port];
        QByteArray b(25, '\0');
        b[0] = char(CommandType::Reply);
        b[1] = char(op);
        b[3] = char(port);
        b[4] = char(m.power);
        putS32(b, 13, int32_t(std::lround(m.tacho)));
        putS32(b, 21, int32_t(std::lround(m.tacho)));
        return b;
    }
    case DirectOp::ResetMotorPosition:
        m_motors[arg(2) % 3].tacho = 0;
        return ok(op);
    case DirectOp::GetBatteryLevel: {
        QByteArray b = ok(op, QByteArray(2, '\0'));
        putU16(b, 3, uint16_t(7900 - std::fmod(t, 600.0) * 0.5));
        return b;
    }
    case DirectOp::KeepAlive:
        return ok(op, QByteArray(4, '\0'));
    case SystemOp::GetFirmwareVersion:
        return ok(op, QByteArray::fromHex("7c011f01"));
    case SystemOp::GetDeviceInfo: {
        QByteArray b(33, '\0');
        b[0] = char(CommandType::Reply);
        b[1] = char(op);
        b.replace(3, 8, "DEMO-NXT");
        b.replace(18, 6, QByteArray::fromHex("0016530abcde"));
        putU32(b, 29, 41472);
        return b;
    }
    default:
        return ok(op);
    }
}

int DemoTransport::simulateSensor(int type, double t, int port) const
{
    switch (type) {
    case SensorType::Switch:
        return std::sin(t * 1.3 + port) > 0.6 ? 1 : 0;
    case SensorType::LightActive:
    case SensorType::LightInactive:
        return int(std::lround(45 + 25 * std::sin(t * 0.9 + port)));
    case SensorType::SoundDb:
    case SensorType::SoundDba:
        return int(std::lround(std::max(0.0, 20 + 30 * std::sin(t * 2.1) * std::sin(t * 0.37))));
    default:
        return 0;
    }
}

} // namespace nxt
