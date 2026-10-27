#pragma once

#include "Transport.h"

#include <QElapsedTimer>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <array>
#include <memory>
#include <optional>

namespace nxt {

enum class SensorKind { None, Touch, Light, Ambient, Sound };

struct SensorInfo {
    uint8_t type;
    uint8_t mode;
    const char *unit;
};
SensorInfo sensorInfo(SensorKind kind);
QString sensorLabel(SensorKind kind);

enum class LinkStatus { Disconnected, Connecting, Connected };

struct BrickState {
    LinkStatus status = LinkStatus::Disconnected;
    bool demo = false;
    QString name;
    QString firmware;
    QString bluetoothAddress;
    quint32 freeFlash = 0;
    std::optional<int> batteryMv;
    /** Requested power per output A/B/C. */
    std::array<int, 3> motorPower{};
    /** Rotation in degrees per output A/B/C, read back from the brick. */
    std::array<int, 3> motorRotation{};
    std::array<SensorKind, 4> sensors{SensorKind::Touch, SensorKind::Sound, SensorKind::Light, SensorKind::None};
    std::array<std::optional<int>, 4> sensorValues{};
    /** What the last USB scan saw while disconnected. */
    enum class Usb { NotFound, NeedsDriver, Ready } usb = Usb::NotFound;
    QString usbDriver;
    /** Set when the last connection attempt failed or the link dropped. */
    QString lastError;
    /** Bumped on every new error, so the UI can tell repeats apart. */
    int errorSerial = 0;
};

/** Lives on the I/O thread. Owns the transport; every method runs there, so nothing needs locking. */
class BrickWorker : public QObject {
    Q_OBJECT
public:
    explicit BrickWorker(std::array<SensorKind, 4> sensors);
    ~BrickWorker() override;

public slots:
    void start();
    void connectUsb();
    void connectDemo();
    void disconnectBrick();
    void setMotors(int a, int b, int c);
    void stopAll();
    void playTone(int hz, int ms);
    void resetRotation(int port);
    void setSensor(int port, int kind);
    void setBrakeOnStop(bool on);
    void setSpeedRegulation(bool on);
    void setAutoConnect(bool on);

signals:
    void stateChanged(const nxt::BrickState &state);

private:
    void attach(std::unique_ptr<Transport> transport);
    void linkLost(const QString &message);
    void fail(const QString &message);
    QByteArray request(const QByteArray &packet);
    void flushMotors(bool force = false);
    void pollTelemetry();
    void tryAutoConnect();
    void applySensorModes();
    void publish();

    std::unique_ptr<Transport> m_transport;
    BrickState m_state;
    QTimer *m_telemetry = nullptr;
    QTimer *m_autoConnect = nullptr;
    QElapsedTimer m_clock;
    qint64 m_lastBattery = 0;
    qint64 m_lastKeepAlive = 0;
    qint64 m_lastMotorSend = 0;
    int m_failures = 0;
    bool m_autoConnectEnabled = true;
    bool m_brakeOnStop = true;
    bool m_regulated = false;
    std::array<int, 3> m_desired{};
    std::array<std::optional<int>, 3> m_sent{};
};

/** UI-thread facade: forwards calls to the worker thread and caches the latest state. */
class Brick : public QObject {
    Q_OBJECT
public:
    static Brick &instance();
    ~Brick() override;

    const BrickState &state() const { return m_state; }
    bool connected() const { return m_state.status == LinkStatus::Connected; }

    void connectUsb();
    void connectDemo();
    void disconnectBrick();
    void setMotors(const std::array<int, 3> &power);
    void stopAll();
    void playTone(int hz, int ms);
    void resetRotation(int port);
    void setSensor(int port, SensorKind kind);
    void setBrakeOnStop(bool on);
    void setSpeedRegulation(bool on);
    /** Stops the motors and the I/O thread; call before QApplication goes away. */
    void shutdown();
    bool brakeOnStop() const { return m_brake; }
    bool speedRegulation() const { return m_regulated; }

signals:
    void stateChanged(const nxt::BrickState &state);

private:
    Brick();
    void onState(const BrickState &s);

    QThread m_thread;
    BrickWorker *m_worker = nullptr;
    BrickState m_state;
    std::array<int, 3> m_lastMotors{};
    bool m_brake = true;
    bool m_regulated = false;
};

/** Six AA cells: ~9.0 V fresh, the NXT starts warning around 6.5 V. */
int batteryPercent(int mv);

} // namespace nxt

Q_DECLARE_METATYPE(nxt::BrickState)
