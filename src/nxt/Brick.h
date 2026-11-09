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

/** Gyro is the HiTechnic gyro: its value is the raw reading (about 600 at rest, ±1 per degree per second). */
enum class SensorKind { None, Touch, Light, Ambient, Sound, Gyro };

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
    /** How the brick is connected: "USB" or "Bluetooth (COM5)". */
    QString link;
    bool bluetooth = false;
    /** Program running on the brick itself (e.g. "robot.rxe"), or empty. */
    QString runningProgram;
    /** The gamepad is being streamed to the running program's inbox. */
    bool padStreaming = false;
    /** Set when the last connection attempt failed or the link dropped. */
    QString lastError;
    /** Bumped on every new error, so the UI can tell repeats apart. */
    int errorSerial = 0;
};

/** A file stored on the brick. */
struct BrickFile {
    QString name;
    quint32 size = 0;
    /** Sounds, Try Me programs and the brick's settings: kept by a normal factory reset. */
    bool builtIn() const;
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
    void connectBluetooth(const QString &port);
    void disconnectBrick();
    void setMotors(int a, int b, int c);
    void stopAll();
    void playTone(int hz, int ms);
    void send(const QByteArray &packet);
    void runMotorFor(int port, int power, int degrees);
    void resetRotation(int port);
    void setSensor(int port, int kind);
    void setBrakeOnStop(bool on);
    void setSpeedRegulation(bool on);
    void setAutoConnect(bool on);
    /** Stops whatever runs on the brick, replaces `name` with `data` and optionally starts it. */
    void uploadProgram(const QString &name, const QByteArray &data, bool run);
    void listFiles();
    /** Deletes user files (all files except settings when `everything`), and renames the brick "NXT" when `rename`.
     *  A normal reset also puts back LEGO's system sounds and Try Me programs that are missing (see SystemFiles). */
    void factoryReset(bool everything, bool rename);
    /** Latest gamepad packet for programs that read the pad; empty stops streaming. */
    void setPadMessage(const QByteArray &message);
    /** Sets the name shown on the brick's screen and in Bluetooth lists. */
    void renameBrick(const QString &name);
    /** Short program the brick runs right after a Bluetooth connection, to show it on its screen. */
    void setBluetoothGreeting(const QByteArray &rxe);

signals:
    void stateChanged(const nxt::BrickState &state);
    void jobProgress(int percent);
    /** An upload, listing or reset ended; `error` is empty on success. */
    void jobFinished(const QString &error);
    void filesListed(const QList<nxt::BrickFile> &files);

private:
    void attach(std::unique_ptr<Transport> transport, const QString &link, bool bluetooth);
    QList<BrickFile> findFiles();
    QByteArray readFile(const QString &name, uint32_t expectedSize);
    /** Writes a new file, reporting progress between `from` and `to` percent. Removes a half-written file on failure. */
    void writeFile(const QString &name, const QByteArray &data, int from, int to);
    /** Copies LEGO's files on the brick to the PC, once each, so a later reset can put them back. */
    void keepSystemFiles(const QList<BrickFile> &files);
    void backupSystemFiles(int serial);
    void greetOverBluetooth(int serial);
    void stopRunningProgram();
    void streamPad();
    void linkLost(const QString &message);
    void fail(const QString &message);
    QByteArray request(const QByteArray &packet);
    void flushMotors(bool force = false);
    void pollTelemetry();
    void pollMoves();
    void tryAutoConnect();
    void applySensorModes();
    void publish();

    std::unique_ptr<Transport> m_transport;
    BrickState m_state;
    QTimer *m_telemetry = nullptr;
    QTimer *m_autoConnect = nullptr;
    QTimer *m_moveTimer = nullptr;
    QTimer *m_padTimer = nullptr;
    QByteArray m_padMessage;
    QByteArray m_padSent;
    qint64 m_lastPadSend = 0;
    qint64 m_lastProgramCheck = 0;
    /** Bumped on every connection, so a delayed backup can tell it still has the same brick. */
    int m_connectSerial = 0;
    QByteArray m_greeting;
    QElapsedTimer m_clock;
    qint64 m_lastBattery = 0;
    qint64 m_lastKeepAlive = 0;
    qint64 m_lastMotorSend = 0;
    qint64 m_lastMovePublish = 0;
    int m_failures = 0;
    bool m_autoConnectEnabled = true;
    bool m_brakeOnStop = true;
    bool m_regulated = false;
    std::array<int, 3> m_desired{};
    std::array<std::optional<int>, 3> m_sent{};
    /** Ports running a move to a rotation count: the periodic resend must not restart them. */
    std::array<bool, 3> m_limited{};
    /** Rotation count each moving port stops at, checked every few milliseconds. */
    std::array<std::optional<int>, 3> m_moveTarget{};
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
    void connectBluetooth(const QString &port);
    void disconnectBrick();
    /** Connect on its own whenever a WinUSB brick is plugged in. */
    void setAutoConnect(bool on);
    void setMotors(const std::array<int, 3> &power);
    void stopAll();
    void playTone(int hz, int ms);
    /** Sends any direct command; its reply, if any, is read and dropped. */
    void send(const QByteArray &packet);
    /** Runs one motor and brakes it after `degrees` (checked every few ms on the USB thread). A later setMotors() for that port takes over. */
    void runMotorFor(int port, int power, int degrees);
    void resetRotation(int port);
    void setSensor(int port, SensorKind kind);
    void setBrakeOnStop(bool on);
    void setSpeedRegulation(bool on);
    /** File jobs run one at a time; each ends with jobFinished(). They return false while another one runs. */
    bool uploadProgram(const QString &name, const QByteArray &data, bool run);
    bool listFiles();
    bool factoryReset(bool everything, bool rename);
    /** Renames the brick (up to 15 plain characters); false when the brick is busy. */
    bool renameBrick(const QString &name);
    void setBluetoothGreeting(const QByteArray &rxe);
    bool busy() const { return m_busy; }
    void setPadMessage(const QByteArray &message);
    /** Stops the motors and the I/O thread; call before QApplication goes away. */
    void shutdown();
    bool brakeOnStop() const { return m_brake; }
    bool speedRegulation() const { return m_regulated; }

signals:
    void stateChanged(const nxt::BrickState &state);
    void jobProgress(int percent);
    void jobFinished(const QString &error);
    void filesListed(const QList<nxt::BrickFile> &files);

private:
    Brick();
    void onState(const BrickState &s);

    QThread m_thread;
    BrickWorker *m_worker = nullptr;
    BrickState m_state;
    std::array<int, 3> m_lastMotors{};
    bool m_brake = true;
    bool m_regulated = false;
    bool m_autoConnect = true;
    QByteArray m_padMessage;
    bool m_busy = false;
};

/**
 * Charge shown as a coloured battery. Tuned so six fresh rechargeable NiMH cells
 * (about 7.8-8.4 V off the charger, 7.2 V for most of their life) read as full or good;
 * alkaline cells start higher and read full for longer. The NXT itself warns near 6.4 V.
 */
enum class BatteryLevel { Empty, Low, Half, Good, Full };
BatteryLevel batteryLevel(int mv);
QString batteryLabel(BatteryLevel level);

} // namespace nxt

Q_DECLARE_METATYPE(nxt::BrickState)
Q_DECLARE_METATYPE(nxt::BrickFile)
