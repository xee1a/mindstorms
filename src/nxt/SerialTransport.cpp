#include "SerialTransport.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace nxt {

namespace {

/** Translation context for this file's messages (lupdate reads Text::tr calls). */
struct Text {
    Q_DECLARE_TR_FUNCTIONS(SerialTransport)
};

std::runtime_error userError(const QString &message) { return std::runtime_error(message.toStdString()); }

bool isBluetooth(const QSerialPortInfo &info)
{
    const QString d = info.description() + info.manufacturer();
    return d.contains(QLatin1String("Bluetooth"), Qt::CaseInsensitive) || d.contains(QLatin1String("BTHENUM"), Qt::CaseInsensitive);
}

} // namespace

QStringList SerialTransport::ports()
{
    QStringList bluetooth, other;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        (isBluetooth(info) ? bluetooth : other) << info.portName();
    return bluetooth + other;
}

std::unique_ptr<SerialTransport> SerialTransport::open(const QString &port)
{
    std::unique_ptr<SerialTransport> t(new SerialTransport);
    t->m_port = std::make_unique<QSerialPort>(port);
    t->m_port->setBaudRate(QSerialPort::Baud115200);
    if (!t->m_port->open(QIODevice::ReadWrite)) {
        switch (t->m_port->error()) {
        case QSerialPort::DeviceNotFoundError: throw userError(Text::tr("Port %1 doesn't exist. Pair the NXT with Windows first.").arg(port));
        case QSerialPort::PermissionError: throw userError(Text::tr("Port %1 is in use by another program.").arg(port));
        default:
            throw userError(Text::tr("Couldn't open %1. Make sure the NXT is switched on, has Bluetooth turned on and is paired with this PC.")
                                .arg(port));
        }
    }
    return t;
}

SerialTransport::~SerialTransport()
{
    if (m_port) m_port->close();
}

QString SerialTransport::label() const { return QStringLiteral("NXT (Bluetooth %1)").arg(m_port ? m_port->portName() : QString()); }

void SerialTransport::write(const QByteArray &packet)
{
    QByteArray framed;
    framed.append(char(packet.size() & 0xff));
    framed.append(char((packet.size() >> 8) & 0xff));
    framed.append(packet);
    if (m_port->write(framed) != framed.size()) throw DisconnectedError(Text::tr("The Bluetooth link to the NXT was lost.").toStdString());
    // Bluetooth on the NXT needs the bytes flushed now; it also can't take packets faster than ~30 per second.
    if (!m_port->waitForBytesWritten(1000)) throw DisconnectedError(Text::tr("The Bluetooth link to the NXT was lost.").toStdString());
}

QByteArray SerialTransport::read(int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    for (;;) {
        if (m_buffer.size() >= 2) {
            const int length = uchar(m_buffer.at(0)) | (uchar(m_buffer.at(1)) << 8);
            if (m_buffer.size() >= 2 + length) {
                QByteArray packet = m_buffer.mid(2, length);
                m_buffer.remove(0, 2 + length);
                return packet;
            }
        }
        const qint64 left = timeoutMs - t.elapsed();
        if (left <= 0) throw TimeoutError();
        if (!m_port->waitForReadyRead(int(left))) {
            if (m_port->error() == QSerialPort::ResourceError) throw DisconnectedError(Text::tr("The Bluetooth link to the NXT was lost.").toStdString());
            continue;
        }
        m_buffer.append(m_port->readAll());
    }
}

} // namespace nxt
