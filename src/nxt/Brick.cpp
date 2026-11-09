#include "Brick.h"
#include "Protocol.h"
#include "SystemFiles.h"
#include "SerialTransport.h"
#include "WinUsbTransport.h"

#include <QCoreApplication>
#include <QSettings>
#include <QThread>
#include <algorithm>

namespace nxt {

namespace {

constexpr int MovePollMs = 4;

constexpr int TelemetryIntervalMs = 120;
constexpr int BluetoothTelemetryMs = 400;
constexpr int AutoConnectIntervalMs = 2000;
constexpr int BatteryIntervalMs = 10'000;
constexpr int KeepAliveIntervalMs = 60'000;
constexpr int MotorRefreshMs = 1'000;
constexpr int ReplyTimeoutMs = 800;
constexpr int ProgramCheckMs = 1'000;
constexpr int PadIntervalMs = 40;
// Copy LEGO's files a little after connecting, once the UI has its first telemetry.
constexpr int BackupDelayMs = 1'500;
// Free flash a reset leaves when it puts sounds back (an NXT has about 120 KB for files).
constexpr qint64 RestoreReserve = 20 * 1024;
constexpr int PadRepeatMs = 250;
// The program that shows "connected over Bluetooth" on the brick's screen.
constexpr char GreetingFile[] = "NXT Studio.rxe";

/** Translation context for this file's messages (lupdate reads Text::tr calls). */
struct Text {
    Q_DECLARE_TR_FUNCTIONS(Brick)
};

QString errorText(const std::exception &e) { return QString::fromStdString(e.what()); }

} // namespace

SensorInfo sensorInfo(SensorKind kind)
{
    switch (kind) {
    case SensorKind::Touch: return {SensorType::Switch, SensorMode::Boolean, ""};
    case SensorKind::Light: return {SensorType::LightActive, SensorMode::PercentFullScale, "%"};
    case SensorKind::Ambient: return {SensorType::LightInactive, SensorMode::PercentFullScale, "%"};
    case SensorKind::Sound: return {SensorType::SoundDb, SensorMode::PercentFullScale, "dB"};
    case SensorKind::Gyro: return {SensorType::LightInactive, SensorMode::Raw, ""};
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
    case SensorKind::Gyro: return QCoreApplication::translate("Sensors", "Gyro (HiTechnic)");
    case SensorKind::None: break;
    }
    return QCoreApplication::translate("Sensors", "Not used");
}

BatteryLevel batteryLevel(int mv)
{
    if (mv >= 7600) return BatteryLevel::Full;
    if (mv >= 7200) return BatteryLevel::Good;
    if (mv >= 6800) return BatteryLevel::Half;
    if (mv >= 6400) return BatteryLevel::Low;
    return BatteryLevel::Empty;
}

QString batteryLabel(BatteryLevel level)
{
    switch (level) {
    case BatteryLevel::Full: return Text::tr("Battery full");
    case BatteryLevel::Good: return Text::tr("Battery good");
    case BatteryLevel::Half: return Text::tr("Battery half empty");
    case BatteryLevel::Low: return Text::tr("Battery low");
    case BatteryLevel::Empty: return Text::tr("Replace the batteries");
    }
    return {};
}

bool BrickFile::builtIn() const
{
    // Sounds (including LEGO's "! Startup.rso"), Try Me programs and the brick's own settings.
    return systemfiles::restorable(name) || name.endsWith(QLatin1String(".sys"), Qt::CaseInsensitive);
}

// ── Worker ────────────────────────────────────────────────

BrickWorker::BrickWorker(std::array<SensorKind, 4> sensors) { m_state.sensors = sensors; }

BrickWorker::~BrickWorker() = default;

void BrickWorker::start()
{
    m_clock.start();
    m_telemetry = new QTimer(this);
    m_telemetry->setInterval(TelemetryIntervalMs);
    connect(m_telemetry, &QTimer::timeout, this, &BrickWorker::pollTelemetry);
    m_moveTimer = new QTimer(this);
    m_moveTimer->setTimerType(Qt::PreciseTimer);
    m_moveTimer->setInterval(MovePollMs);
    connect(m_moveTimer, &QTimer::timeout, this, &BrickWorker::pollMoves);
    m_padTimer = new QTimer(this);
    m_padTimer->setInterval(PadIntervalMs);
    connect(m_padTimer, &QTimer::timeout, this, &BrickWorker::streamPad);
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
        attach(WinUsbTransport::open(), QStringLiteral("USB"), false);
    } catch (const std::exception &) {
        // Stay quiet; the user gets the reason when they press Connect.
    }
}

void BrickWorker::connectUsb()
{
    m_state.status = LinkStatus::Connecting;
    publish();
    try {
        attach(WinUsbTransport::open(), QStringLiteral("USB"), false);
    } catch (const std::exception &e) {
        m_state.status = LinkStatus::Disconnected;
        fail(errorText(e));
        publish();
    }
}

void BrickWorker::connectBluetooth(const QString &port)
{
    disconnectBrick();
    m_state.status = LinkStatus::Connecting;
    publish();
    try {
        attach(SerialTransport::open(port), QStringLiteral("Bluetooth (%1)").arg(port), true);
    } catch (const std::exception &e) {
        m_state.status = LinkStatus::Disconnected;
        fail(errorText(e));
        publish();
    }
}

void BrickWorker::attach(std::unique_ptr<Transport> transport, const QString &link, bool bluetooth)
{
    disconnectBrick();
    m_transport = std::move(transport);
    m_state.link = link;
    m_state.bluetooth = bluetooth;
    m_state.runningProgram.clear();
    m_padSent.clear();
    m_state.status = LinkStatus::Connecting;
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
        // Each Bluetooth request takes tens of milliseconds, so poll less often there.
        m_telemetry->setInterval(bluetooth ? BluetoothTelemetryMs : TelemetryIntervalMs);
        m_telemetry->start();
        m_padTimer->start();
        publish();
        const int serial = ++m_connectSerial;
        // Reading files over Bluetooth is slow, so LEGO's files are only copied over USB.
        if (!bluetooth) QTimer::singleShot(BackupDelayMs, this, [this, serial] { backupSystemFiles(serial); });
        else QTimer::singleShot(0, this, [this, serial] { greetOverBluetooth(serial); });
    } catch (const std::exception &e) {
        // Bluetooth bricks that are off or out of range never answer at all.
        linkLost(bluetooth && dynamic_cast<const TimeoutError *>(&e) ? Text::tr("The NXT didn't answer over Bluetooth. Is it switched on and nearby?")
                                                                     : errorText(e));
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
    m_padTimer->stop();
    m_transport.reset();
    m_state.status = LinkStatus::Disconnected;
    m_state.batteryMv.reset();
    m_state.sensorValues = {};
    m_state.runningProgram.clear();
    m_state.padStreaming = false;
    publish();
}

void BrickWorker::linkLost(const QString &message)
{
    m_telemetry->stop();
    m_padTimer->stop();
    m_moveTimer->stop();
    m_moveTarget = {};
    m_limited = {};
    m_transport.reset();
    m_state.status = LinkStatus::Disconnected;
    m_state.batteryMv.reset();
    m_state.sensorValues = {};
    m_state.runningProgram.clear();
    m_state.padStreaming = false;
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
    for (int i = 0; i < 3; ++i)
        if (next[i] != m_desired[i]) {
            m_limited[i] = false;
            m_moveTarget[i].reset();
        }
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
        if (m_limited[i]) continue;
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

void BrickWorker::send(const QByteArray &packet)
{
    if (!m_transport) return;
    try {
        request(packet);
    } catch (const std::exception &) {
    }
}

void BrickWorker::runMotorFor(int port, int power, int degrees)
{
    if (!m_transport || port < 0 || port > 2) return;
    try {
        flushMotors();
        const int start = parseOutputState(request(getOutputState(uint8_t(port)))).rotationCount;
        m_state.motorRotation[port] = start;
        m_transport->write(motorPower(uint8_t(port), power, {m_brakeOnStop, m_regulated}));
        m_desired[port] = power;
        m_sent[port] = power;
        m_limited[port] = true;
        m_moveTarget[port] = start + (power < 0 ? -1 : 1) * std::max(0, degrees);
        m_state.motorPower[port] = power;
        m_moveTimer->start();
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
        return;
    } catch (const std::exception &) {
        ++m_failures;
    }
    publish();
}

void BrickWorker::pollMoves()
{
    bool any = false;
    if (m_transport && m_state.status == LinkStatus::Connected) {
        try {
            for (int port = 0; port < 3; ++port) {
                if (!m_moveTarget[port]) continue;
                const int now = parseOutputState(request(getOutputState(uint8_t(port)))).rotationCount;
                m_state.motorRotation[port] = now;
                const bool forward = m_desired[port] >= 0;
                if (forward ? now < *m_moveTarget[port] : now > *m_moveTarget[port]) {
                    any = true;
                    // Keep the UI (and a running program) up to date while the move lasts.
                    if (m_clock.elapsed() - m_lastMovePublish > 40) {
                        m_lastMovePublish = m_clock.elapsed();
                        publish();
                    }
                    continue;
                }
                // Reached: brake right away; the caller's own stop follows shortly.
                m_transport->write(motorPower(uint8_t(port), 0, {true, false}));
                m_desired[port] = 0;
                m_sent[port] = 0;
                m_state.motorPower[port] = 0;
                m_moveTarget[port].reset();
                m_limited[port] = false;
                publish();
            }
        } catch (const DisconnectedError &e) {
            linkLost(errorText(e));
        } catch (const std::exception &) {
            ++m_failures;
        }
    }
    if (!any) {
        m_moveTimer->stop();
        m_moveTarget = {};
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
            // The gyro is read like NXC's SensorRaw does; the firmware's scaling is for light sensors.
            const int reading = m_state.sensors[port] == SensorKind::Gyro ? int(v.raw) : v.scaled;
            m_state.sensorValues[port] = v.valid ? std::optional<int>(reading) : std::nullopt;
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
        if (now - m_lastProgramCheck > ProgramCheckMs) {
            m_state.runningProgram = parseCurrentProgramName(request(getCurrentProgramName()));
            m_lastProgramCheck = now;
        }
        m_failures = 0;
        publish();
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
    } catch (const std::exception &e) {
        if (++m_failures >= 3) linkLost(errorText(e));
    }
}

void BrickWorker::setPadMessage(const QByteArray &message) { m_padMessage = message; }

void BrickWorker::streamPad()
{
    // Only a program running on the brick reads the inbox; don't flood it otherwise.
    const bool want = m_transport && m_state.status == LinkStatus::Connected && !m_state.runningProgram.isEmpty() && !m_padMessage.isEmpty();
    if (want != m_state.padStreaming) {
        m_state.padStreaming = want;
        m_padSent.clear();
        publish();
    }
    if (!want) return;
    const qint64 now = m_clock.elapsed();
    if (m_padMessage == m_padSent && now - m_lastPadSend < PadRepeatMs) return;
    try {
        m_transport->write(messageWrite(PadMailbox, m_padMessage));
        m_padSent = m_padMessage;
        m_lastPadSend = now;
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
    } catch (const std::exception &) {
        ++m_failures;
    }
}

void BrickWorker::stopRunningProgram()
{
    m_transport->write(nxt::stopProgram());
    // Give the VM time to close the program's files before they are replaced.
    QThread::msleep(150);
    m_state.runningProgram.clear();
}

QList<BrickFile> BrickWorker::findFiles()
{
    QList<BrickFile> files;
    std::optional<FoundFile> f = parseFind(request(findFirst(QStringLiteral("*.*"))), SystemOp::FindFirst);
    std::optional<uint8_t> handle;
    while (f) {
        handle = f->handle;
        files << BrickFile{f->name, f->size};
        if (files.size() > 500) break; // a brick holds far fewer; guards against a confused reply loop
        f = parseFind(request(findNext(f->handle)), SystemOp::FindNext);
    }
    if (handle) {
        try {
            request(closeHandle(*handle));
        } catch (const std::exception &) {
            // Already closed by the brick at the end of the listing.
        }
    }
    return files;
}

void BrickWorker::listFiles()
{
    if (!m_transport || m_state.status != LinkStatus::Connected) return emit jobFinished(Text::tr("The NXT isn't connected."));
    try {
        const QList<BrickFile> files = findFiles();
        emit filesListed(files);
        emit jobFinished({});
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
        emit jobFinished(Text::tr("The NXT was disconnected."));
    } catch (const std::exception &e) {
        emit jobFinished(Text::tr("Couldn't read the list of files: %1").arg(errorText(e)));
    }
}

QByteArray BrickWorker::readFile(const QString &name, uint32_t expectedSize)
{
    const OpenedFile file = parseOpenRead(request(openRead(name)));
    QByteArray data;
    try {
        if (file.size != expectedSize) throw ProtocolError("File size changed");
        while (data.size() < qsizetype(file.size)) {
            const int want = std::min<int>(MaxReadChunk, int(file.size - data.size()));
            const QByteArray chunk = parseRead(request(readChunk(file.handle, want)));
            if (chunk.isEmpty()) throw ProtocolError("Empty READ reply");
            data += chunk;
        }
    } catch (...) {
        try {
            request(closeHandle(file.handle));
        } catch (const std::exception &) {
        }
        throw;
    }
    checkStatus(request(closeHandle(file.handle)), SystemOp::Close);
    return data;
}

void BrickWorker::writeFile(const QString &name, const QByteArray &data, int from, int to)
{
    const QByteArray open = request(openWrite(name, uint32_t(data.size())));
    const uint8_t status = replyStatus(open, SystemOp::OpenWrite);
    if (status == FileStatus::NoSpace || status == 0x89)
        throw std::runtime_error(Text::tr("There isn't enough free memory on the NXT. Delete some files from the brick, "
                                    "or use Factory reset in Settings → Brick.").toStdString());
    const uint8_t handle = parseOpenWrite(open);
    try {
        int lastPercent = -1;
        for (qsizetype off = 0; off < data.size(); off += MaxWriteChunk) {
            checkStatus(request(writeChunk(handle, data.mid(off, MaxWriteChunk))), SystemOp::Write);
            const qsizetype done = std::min<qsizetype>(data.size(), off + MaxWriteChunk);
            const int percent = from + int(done * (to - from) / std::max<qsizetype>(1, data.size()));
            if (from >= 0 && percent != lastPercent) emit jobProgress(lastPercent = percent);
        }
        checkStatus(request(closeHandle(handle)), SystemOp::Close);
    } catch (const DisconnectedError &) {
        throw;
    } catch (...) {
        try {
            request(closeHandle(handle));
            request(deleteFile(name)); // don't leave half a file behind
        } catch (const std::exception &) {
        }
        throw;
    }
}

void BrickWorker::keepSystemFiles(const QList<BrickFile> &files)
{
    for (const BrickFile &f : files) {
        if (!systemfiles::restorable(f.name) || systemfiles::isKept(f.name) || f.size == 0) continue;
        try {
            systemfiles::keep(f.name, readFile(f.name, f.size));
        } catch (const DisconnectedError &) {
            throw;
        } catch (const std::exception &) {
            // A file that can't be read now is copied on a later connection.
        }
    }
}

void BrickWorker::backupSystemFiles(int serial)
{
    if (serial != m_connectSerial || !m_transport || m_state.status != LinkStatus::Connected) return;
    // Don't hold up a degree move started from the PC; try again shortly.
    if (m_moveTimer->isActive()) {
        QTimer::singleShot(BackupDelayMs, this, [this, serial] { backupSystemFiles(serial); });
        return;
    }
    try {
        keepSystemFiles(findFiles());
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
    } catch (const std::exception &) {
        // Only a convenience; the next connection tries again.
    }
}

void BrickWorker::setBluetoothGreeting(const QByteArray &rxe) { m_greeting = rxe; }

void BrickWorker::greetOverBluetooth(int serial)
{
    if (m_greeting.isEmpty() || serial != m_connectSerial || !m_transport || m_state.status != LinkStatus::Connected) return;
    try {
        // Never interrupt a program the class started on the brick.
        if (!parseCurrentProgramName(request(getCurrentProgramName())).isEmpty()) return;
        const QString name = QString::fromLatin1(GreetingFile);
        const uint8_t del = replyStatus(request(deleteFile(name)), SystemOp::Delete);
        if (del != 0 && del != FileStatus::FileNotFound) return;
        writeFile(name, m_greeting, -1, -1);
        checkStatus(request(startProgram(name)), DirectOp::StartProgram);
        m_state.runningProgram = name;
        m_lastProgramCheck = m_clock.elapsed();
        publish();
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
    } catch (const std::exception &) {
        // Only a greeting (for example, no room on the brick): the connection itself is fine.
    }
}

void BrickWorker::renameBrick(const QString &name)
{
    if (!m_transport || m_state.status != LinkStatus::Connected) return emit jobFinished(Text::tr("The NXT isn't connected."));
    try {
        checkStatus(request(setBrickName(name)), SystemOp::SetBrickName);
        // DEVICE INFO still reports the old name for a moment, so take the one that was just accepted.
        m_state.name = name;
        publish();
        emit jobFinished({});
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
        emit jobFinished(Text::tr("The NXT was disconnected."));
    } catch (const std::exception &e) {
        emit jobFinished(errorText(e));
    }
}

void BrickWorker::uploadProgram(const QString &name, const QByteArray &data, bool run)
{
    if (!m_transport || m_state.status != LinkStatus::Connected) return emit jobFinished(Text::tr("The NXT isn't connected."));
    m_telemetry->stop();
    try {
        stopAll();
        stopRunningProgram();
        const uint8_t del = replyStatus(request(deleteFile(name)), SystemOp::Delete);
        if (del != 0 && del != FileStatus::FileNotFound) throw NxtError(del, SystemOp::Delete);
        writeFile(name, data, 0, 100);
        const DeviceInfo info = parseDeviceInfo(request(getDeviceInfo()));
        m_state.freeFlash = info.freeFlash;
        if (run) {
            checkStatus(request(startProgram(name)), DirectOp::StartProgram);
            m_state.runningProgram = name;
            m_lastProgramCheck = m_clock.elapsed();
        }
        m_telemetry->start();
        publish();
        emit jobFinished({});
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
        emit jobFinished(Text::tr("The NXT was disconnected during the upload. Connect it again and retry."));
    } catch (const std::exception &e) {
        if (m_transport) m_telemetry->start();
        emit jobFinished(errorText(e));
    }
}

void BrickWorker::factoryReset(bool everything, bool rename)
{
    if (!m_transport || m_state.status != LinkStatus::Connected) return emit jobFinished(Text::tr("The NXT isn't connected."));
    m_telemetry->stop();
    try {
        stopAll();
        stopRunningProgram();
        const QList<BrickFile> files = findFiles();
        // Copy LEGO's files first, so even a full wipe can be undone by a later normal reset.
        keepSystemFiles(files);
        QList<BrickFile> doomed;
        QStringList present;
        for (const BrickFile &f : files) {
            // The brick's own settings file is always kept; the firmware itself is never a file.
            if (f.name.endsWith(QLatin1String(".sys"), Qt::CaseInsensitive)) continue;
            if (everything || !f.builtIn()) doomed << f;
            else present << f.name;
        }
        QStringList missing;
        if (!everything) {
            for (const QString &name : systemfiles::available())
                if (!present.contains(name, Qt::CaseInsensitive)) missing << name;
        }
        const int deleteShare = missing.isEmpty() ? 100 : 60;
        // Put files back only while they fit, keeping some room for the class's programs.
        qint64 room = qint64(parseDeviceInfo(request(getDeviceInfo())).freeFlash) - RestoreReserve;
        for (const BrickFile &f : doomed) room += f.size;
        for (int i = 0; i < doomed.size(); ++i) {
            const uint8_t st = replyStatus(request(deleteFile(doomed[i].name)), SystemOp::Delete);
            if (st != 0 && st != FileStatus::FileNotFound) throw NxtError(st, SystemOp::Delete);
            emit jobProgress((i + 1) * deleteShare / int(doomed.size()));
        }
        for (int i = 0; i < missing.size(); ++i) {
            const QByteArray data = systemfiles::contents(missing[i]);
            const int span = 100 - deleteShare;
            if (data.size() > room) continue;
            room -= data.size();
            if (!data.isEmpty())
                writeFile(missing[i], data, deleteShare + i * span / int(missing.size()), deleteShare + (i + 1) * span / int(missing.size()));
        }
        if (rename) checkStatus(request(setBrickName(QStringLiteral("NXT"))), SystemOp::SetBrickName);
        const DeviceInfo info = parseDeviceInfo(request(getDeviceInfo()));
        m_state.freeFlash = info.freeFlash;
        m_state.name = info.name.isEmpty() ? QStringLiteral("NXT") : info.name;
        m_telemetry->start();
        publish();
        emit jobFinished({});
    } catch (const DisconnectedError &e) {
        linkLost(errorText(e));
        emit jobFinished(Text::tr("The NXT was disconnected during the reset. Connect it again and retry."));
    } catch (const std::exception &e) {
        if (m_transport) m_telemetry->start();
        emit jobFinished(errorText(e));
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
    qRegisterMetaType<BrickFile>();
    qRegisterMetaType<QList<BrickFile>>();
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
    connect(m_worker, &BrickWorker::jobProgress, this, &Brick::jobProgress);
    connect(m_worker, &BrickWorker::jobFinished, this, [this](const QString &error) {
        m_busy = false;
        emit jobFinished(error);
    });
    connect(m_worker, &BrickWorker::filesListed, this, &Brick::filesListed);
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
    QMetaObject::invokeMethod(m_worker, [w = m_worker, autoConnect = m_autoConnect] {
        w->setAutoConnect(autoConnect);
        w->connectUsb();
    });
}

void Brick::connectBluetooth(const QString &port)
{
    QSettings().setValue(QStringLiteral("brick/bluetoothPort"), port);
    QMetaObject::invokeMethod(m_worker, [w = m_worker, port] { w->connectBluetooth(port); });
}

bool Brick::uploadProgram(const QString &name, const QByteArray &data, bool run)
{
    if (m_busy) return false;
    m_busy = true;
    m_lastMotors = {};
    QMetaObject::invokeMethod(m_worker, [w = m_worker, name, data, run] { w->uploadProgram(name, data, run); });
    return true;
}

bool Brick::listFiles()
{
    if (m_busy) return false;
    m_busy = true;
    QMetaObject::invokeMethod(m_worker, &BrickWorker::listFiles);
    return true;
}

bool Brick::renameBrick(const QString &name)
{
    if (m_busy) return false;
    m_busy = true;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, name] { w->renameBrick(name); });
    return true;
}

void Brick::setBluetoothGreeting(const QByteArray &rxe)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, rxe] { w->setBluetoothGreeting(rxe); });
}

bool Brick::factoryReset(bool everything, bool rename)
{
    if (m_busy) return false;
    m_busy = true;
    m_lastMotors = {};
    QMetaObject::invokeMethod(m_worker, [w = m_worker, everything, rename] { w->factoryReset(everything, rename); });
    return true;
}

void Brick::setPadMessage(const QByteArray &message)
{
    if (message == m_padMessage) return;
    m_padMessage = message;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, message] { w->setPadMessage(message); });
}

void Brick::setAutoConnect(bool on)
{
    m_autoConnect = on;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, on] { w->setAutoConnect(on); });
}
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

void Brick::send(const QByteArray &packet)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, packet] { w->send(packet); });
}

void Brick::runMotorFor(int port, int power, int degrees)
{
    if (port < 0 || port > 2) return;
    m_lastMotors[port] = power;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, port, power, degrees] { w->runMotorFor(port, power, degrees); });
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
