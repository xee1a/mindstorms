#include "Brick.h"
#include "DemoTransport.h"
#include "Protocol.h"
#include "WinUsbTransport.h"

#include <QCoreApplication>
#include <QSettings>
#include <algorithm>

namespace nxt {

namespace {

constexpr int TelemetryIntervalMs = 120;
constexpr int AutoConnectIntervalMs = 2000;
constexpr int BatteryIntervalMs = 10'000;
constexpr int KeepAliveIntervalMs = 60'000;
constexpr int MotorRefreshMs = 1'000;
constexpr int ReplyTimeoutMs = 800;

QString errorText(const std::exception &e) { return QString::fromStdString(e.what()); }

} // namespace

SensorInfo sensorInfo(SensorKind kind)
{
    switch (kind) {
    case SensorKind::Touch: return {SensorType::Switch, SensorMode::Boolean, ""};
    case SensorKind::Light: return {SensorType::LightActive, SensorMode::PercentFullScale, "%"};
    case SensorKind::Ambient: return {SensorType::LightInactive, SensorMode::PercentFullScale, "%"};
    case SensorKind::Sound: return {SensorType::SoundDb, SensorMode::PercentFullScale, "dB"};
    case SensorKind::None: break;
    }
    return {SensorType::None, SensorMode::Raw, ""};
}

QString sensorLabel(SensorKind kind)
{
    switch (kind) {
    case SensorKind::Touch: return QCoreApplication::translate("Sensors", "Touch");
    case SensorKind::Light: return QCoreApplication::translate("Sensors", "Light (reflected)");
    case SensorKind::Ambient: return QCoreApplication::translate("Sensors", "Light (ambient)");
    case SensorKind::Sound: return QCoreApplication::translate("Sensors", "Sound");
    case SensorKind::None: break;
    }
    return QCoreApplication::translate("Sensors", "Not used");
}

int batteryPercent(int mv) { return std::clamp((mv - 6500) * 100 / 2500, 0, 100); }

// ── Worker ────────────────────────────────────────────────

BrickWorker::BrickWorker(std::array<SensorKind, 4> sensors) { m_state.sensors = sensors; }

BrickWorker::~BrickWorker() = default;

void BrickWorker::start()
{
    m_clock.start();
    m_telemetry = new QTimer(this);
    m_telemetry->setInterval(TelemetryIntervalMs);
    connect(m_telemetry, &QTimer::timeout, this, &BrickWorker::pollTelemetry);
    m_autoConnect = new QTimer(this);
    m_autoConnect->setInterval(AutoConnectIntervalMs);
    connect(m_autoConnect, &QTimer::timeout, this, &BrickWorker::tryAutoConnect);
    m_autoConnect->start();
    tryAutoConnect();
}

void BrickWorker::publish() { emit stateChanged(m_state); }

void BrickWorker::fail(const QString &message)
{
    m_state.lastError = message;
    ++m_state.errorSerial;
}

void BrickWorker::tryAutoConnect()
{
    if (m_transport) return;
    const auto probe = WinUsbTransport::probe();
    const auto usb = probe.presence == WinUsbTransport::Presence::Ready         ? BrickState::Usb::Ready
                     : probe.presence == WinUsbTransport::Presence::NeedsDriver ? BrickState::Usb::NeedsDriver
                                                                                : BrickState::Usb::NotFound;
    if (usb != m_state.usb || probe.service != m_state.usbDriver) {
        m_state.usb = usb;
        m_state.usbDriver = probe.service;
        publish();
    }
    if (!m_autoConnectEnabled || usb != BrickState::Usb::Ready) return;
    try {
        attach(WinUsbTransport::open());
    } catch (const std::exception &) {
        // Stay quiet; the user gets the reason when they press Connect.
    }
}

void BrickWorker::connectUsb()
{
    m_state.status = LinkStatus::Connecting;
    publish();
    try {
        attach(WinUsbTransport::open());
    } catch (const std::exception &e) {
        m_state.status = LinkStatus::Disconnected;
        fail(errorText(e));
        publish();
    }
}

void BrickWorker::connectDemo() { attach(std::make_unique<DemoTransport>()); }

void BrickWorker::attach(std::unique_ptr<Transport> transport)
{
    disconnectBrick();
    m_transport = std::move(transport);
    m_state.status = LinkStatus::Connecting;
    m_state.demo = m_transport->isDemo();
    publish();
    m_failures = 0;
    m_sent = {};
    try {
        const FirmwareVersion fw = parseFirmwareVersion(request(getFirmwareVersion()));
        const DeviceInfo info = parseDeviceInfo(request(getDeviceInfo()));
        m_state.batteryMv = parseBatteryLevel(request(getBatteryLevel()));
        m_lastBattery = m_lastKeepAlive = m_clock.elapsed();
        m_state.status = LinkStatus::Connected;
        m_state.name = info.name.isEmpty() ? QStringLiteral("NXT") : info.name;
        m_state.firmware = fw.firmware;
        m_state.bluetoothAddress = info.bluetoothAddress;
        m_state.freeFlash = info.freeFlash;
        m_state.lastError.clear();
        applySensorModes();
        m_telemetry->start();
        publish();
    } catch (const std::exception &e) {
        linkLost(errorText(e));
    }
}

void BrickWorker::disconnectBrick()
{
    if (!m_transport) return;
    try {
        m_transport->write(motorPower(PortAll, 0, {m_brakeOnStop, false}));
    } catch (const std::exception &) {
    }
    m_telemetry->stop();
    m_transport.reset();
    m_state.status = LinkStatus::Disconnected;
    m_state.batteryMv.reset();
    m_state.sensorValues = {};
    publish();
}

void BrickWorker::linkLost(const QString &message)
{
    m_telemetry->stop();
    m_transport.reset();
    m_state.status = LinkStatus::Disconnected;
    m_state.batteryMv.reset();
    m_state.sensorValues = {};
    fail(message);
    publish();
}

QByteArray BrickWorker::request(const QByteArray &packet)
{
    if (!m_transport) throw std::runtime_error("Not connected to an NXT");
    flushMotors();
    m_transport->write(packet);
    if (!expectsReply(packet)) return {};
    // Skip any reply left over from an earlier request.
    for (int attempt = 0; attempt < 3; ++attempt) {
        QByteArray reply = m_transport->read(ReplyTimeoutMs);
        if (reply.size() >= 2 && reply.at(1) == packet.at(1)) return reply;
    }
    throw ProtocolError("The NXT sent replies out of order");
}

void BrickWorker::setMotors(int a, int b, int c)
{
    const std::array<int, 3> next{std::clamp(a, -100, 100), std::clamp(b, -100, 100), std::clamp(c, -100, 100)};
    if (next == m_desired && next == m_state.motorPower) return;
    m_desired = next;
    m_state.motorPower = next;
    try {
        flushMotors();
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
        return;
    } catch (const std::exception &) {
        ++m_failures;
    }
    publish();
}

void BrickWorker::flushMotors(bool force)
{
    if (!m_transport || m_state.status != LinkStatus::Connected) return;
    const qint64 now = m_clock.elapsed();
    // Resend everything now and then, in case a no-reply packet was lost.
    const bool refresh = force || now - m_lastMotorSend > MotorRefreshMs;
    for (int i = 0; i < 3; ++i) {
        if (!refresh && m_sent[i] == m_desired[i]) continue;
        m_transport->write(motorPower(uint8_t(i), m_desired[i], {m_brakeOnStop, m_regulated}));
        m_sent[i] = m_desired[i];
    }
    if (refresh) m_lastMotorSend = now;
}

void BrickWorker::stopAll()
{
    m_desired = {};
    m_state.motorPower = {};
    if (m_transport) {
        try {
            m_transport->write(motorPower(PortAll, 0, {m_brakeOnStop, false}));
            m_sent = {0, 0, 0};
        } catch (const std::exception &) {
        }
    }
    publish();
}

void BrickWorker::playTone(int hz, int ms)
{
    if (!m_transport) return;
    try {
        request(nxt::playTone(hz, ms));
    } catch (const std::exception &) {
    }
}

void BrickWorker::resetRotation(int port)
{
    if (!m_transport) return;
    try {
        request(resetMotorPosition(uint8_t(port)));
    } catch (const std::exception &) {
    }
}

void BrickWorker::setSensor(int port, int kind)
{
    if (port < 0 || port > 3) return;
    m_state.sensors[port] = static_cast<SensorKind>(kind);
    m_state.sensorValues[port].reset();
    if (m_transport) {
        try {
            const SensorInfo s = sensorInfo(m_state.sensors[port]);
            request(setInputMode(uint8_t(port), s.type, s.mode));
        } catch (const std::exception &) {
        }
    }
    publish();
}

void BrickWorker::setBrakeOnStop(bool on) { m_brakeOnStop = on; }
void BrickWorker::setSpeedRegulation(bool on)
{
    m_regulated = on;
    m_sent = {}; // resend with the new mode
}
void BrickWorker::setAutoConnect(bool on) { m_autoConnectEnabled = on; }

void BrickWorker::applySensorModes()
{
    for (int port = 0; port < 4; ++port) {
        const SensorInfo s = sensorInfo(m_state.sensors[port]);
        request(setInputMode(uint8_t(port), s.type, s.mode));
    }
}

void BrickWorker::pollTelemetry()
{
    if (!m_transport || m_state.status != LinkStatus::Connected) return;
    try {
        for (int port = 0; port < 4; ++port) {
            if (m_state.sensors[port] == SensorKind::None) {
                m_state.sensorValues[port].reset();
                continue;
            }
            const InputValues v = parseInputValues(request(getInputValues(uint8_t(port))));
            m_state.sensorValues[port] = v.valid ? std::optional<int>(v.scaled) : std::nullopt;
        }
        for (int port = 0; port < 3; ++port)
            m_state.motorRotation[port] = parseOutputState(request(getOutputState(uint8_t(port)))).rotationCount;

        const qint64 now = m_clock.elapsed();
        if (now - m_lastBattery > BatteryIntervalMs) {
            m_state.batteryMv = parseBatteryLevel(request(getBatteryLevel()));
            m_lastBattery = now;
        }
        if (now - m_lastKeepAlive > KeepAliveIntervalMs) {
            request(keepAlive());
            m_lastKeepAlive = now;
        }
        m_failures = 0;
        publish();
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
    } catch (const std::exception &e) {
        if (++m_failures >= 3) linkLost(errorText(e));
    }
}

// ── Facade ────────────────────────────────────────────────

Brick &Brick::instance()
{
    static Brick brick;
    return brick;
}

Brick::Brick()
{
    qRegisterMetaType<BrickState>();
    QSettings settings;
    std::array<SensorKind, 4> sensors = m_state.sensors;
    for (int i = 0; i < 4; ++i)
        sensors[i] = static_cast<SensorKind>(settings.value(QStringLiteral("sensors/port%1").arg(i + 1), int(sensors[i])).toInt());
    m_state.sensors = sensors;
    m_brake = settings.value(QStringLiteral("drive/brakeOnStop"), true).toBool();
    m_regulated = settings.value(QStringLiteral("drive/speedRegulation"), false).toBool();

    m_worker = new BrickWorker(sensors);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, m_worker, &BrickWorker::start);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &BrickWorker::stateChanged, this, &Brick::onState);
    m_thread.setObjectName(QStringLiteral("nxt-io"));
    m_thread.start();
    QMetaObject::invokeMethod(m_worker, [w = m_worker, b = m_brake, r = m_regulated] {
        w->setBrakeOnStop(b);
        w->setSpeedRegulation(r);
    });
}

Brick::~Brick() { shutdown(); }

void Brick::shutdown()
{
    if (!m_thread.isRunning()) return;
    // Leave the motors stopped when the app closes.
    QMetaObject::invokeMethod(m_worker, &BrickWorker::disconnectBrick, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void Brick::onState(const BrickState &s)
{
    m_state = s;
    emit stateChanged(m_state);
}

void Brick::connectUsb()
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker] {
        w->setAutoConnect(true);
        w->connectUsb();
    });
}
void Brick::connectDemo() { QMetaObject::invokeMethod(m_worker, &BrickWorker::connectDemo); }
void Brick::disconnectBrick()
{
    // Don't immediately reconnect a brick the user just disconnected.
    QMetaObject::invokeMethod(m_worker, [w = m_worker] {
        w->setAutoConnect(false);
        w->disconnectBrick();
    });
}

void Brick::setMotors(const std::array<int, 3> &power)
{
    if (power == m_lastMotors) return;
    m_lastMotors = power;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, power] { w->setMotors(power[0], power[1], power[2]); });
}

void Brick::stopAll()
{
    m_lastMotors = {};
    QMetaObject::invokeMethod(m_worker, &BrickWorker::stopAll);
}

void Brick::playTone(int hz, int ms)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, hz, ms] { w->playTone(hz, ms); });
}

void Brick::resetRotation(int port)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, port] { w->resetRotation(port); });
}

void Brick::setSensor(int port, SensorKind kind)
{
    QSettings().setValue(QStringLiteral("sensors/port%1").arg(port + 1), int(kind));
    QMetaObject::invokeMethod(m_worker, [w = m_worker, port, kind] { w->setSensor(port, int(kind)); });
}

void Brick::setBrakeOnStop(bool on)
{
    m_brake = on;
    QSettings().setValue(QStringLiteral("drive/brakeOnStop"), on);
    QMetaObject::invokeMethod(m_worker, [w = m_worker, on] { w->setBrakeOnStop(on); });
}

void Brick::setSpeedRegulation(bool on)
{
    m_regulated = on;
    QSettings().setValue(QStringLiteral("drive/speedRegulation"), on);
    QMetaObject::invokeMethod(m_worker, [w = m_worker, on] { w->setSpeedRegulation(on); });
}

} // namespace nxt
