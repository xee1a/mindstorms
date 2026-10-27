#pragma once
// LEGO MINDSTORMS NXT direct/system command packets (USB framing: no length prefix).
// Reference: "LEGO MINDSTORMS NXT Bluetooth Developer Kit", appendix 1 & 2.

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <stdexcept>

namespace nxt {

namespace CommandType {
constexpr uint8_t Direct = 0x00;
constexpr uint8_t System = 0x01;
constexpr uint8_t Reply = 0x02;
constexpr uint8_t NoReply = 0x80;
} // namespace CommandType

namespace DirectOp {
constexpr uint8_t StartProgram = 0x00;
constexpr uint8_t StopProgram = 0x01;
constexpr uint8_t PlayTone = 0x03;
constexpr uint8_t SetOutputState = 0x04;
constexpr uint8_t SetInputMode = 0x05;
constexpr uint8_t GetOutputState = 0x06;
constexpr uint8_t GetInputValues = 0x07;
constexpr uint8_t ResetMotorPosition = 0x0a;
constexpr uint8_t GetBatteryLevel = 0x0b;
constexpr uint8_t StopSoundPlayback = 0x0c;
constexpr uint8_t KeepAlive = 0x0d;
} // namespace DirectOp

namespace SystemOp {
constexpr uint8_t GetFirmwareVersion = 0x88;
constexpr uint8_t GetDeviceInfo = 0x9b;
} // namespace SystemOp

namespace OutputMode {
constexpr uint8_t MotorOn = 0x01;
constexpr uint8_t Brake = 0x02;
constexpr uint8_t Regulated = 0x04;
} // namespace OutputMode

namespace RegulationMode {
constexpr uint8_t Idle = 0x00;
constexpr uint8_t Speed = 0x01;
constexpr uint8_t Sync = 0x02;
} // namespace RegulationMode

namespace RunState {
constexpr uint8_t Idle = 0x00;
constexpr uint8_t RampUp = 0x10;
constexpr uint8_t Running = 0x20;
constexpr uint8_t RampDown = 0x40;
} // namespace RunState

namespace SensorType {
constexpr uint8_t None = 0x00;
constexpr uint8_t Switch = 0x01;
constexpr uint8_t LightActive = 0x05;
constexpr uint8_t LightInactive = 0x06;
constexpr uint8_t SoundDb = 0x07;
constexpr uint8_t SoundDba = 0x08;
constexpr uint8_t LowSpeed9V = 0x0b;
} // namespace SensorType

namespace SensorMode {
constexpr uint8_t Raw = 0x00;
constexpr uint8_t Boolean = 0x20;
constexpr uint8_t PercentFullScale = 0x80;
} // namespace SensorMode

/** Addresses all three outputs at once. */
constexpr uint8_t PortAll = 0xff;

/** The brick answered with a non-zero status byte. */
class NxtError : public std::runtime_error {
public:
    NxtError(uint8_t status, uint8_t op);
    uint8_t status;
};

/** A reply didn't have the expected shape. */
class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct OutputState {
    uint8_t port = 0;
    int power = 0; // -100..100
    uint8_t mode = 0;
    uint8_t regulation = RegulationMode::Idle;
    int turnRatio = 0; // -100..100, sync regulation only
    uint8_t runState = RunState::Idle;
    uint32_t tachoLimit = 0; // degrees, 0 = forever
};

struct MotorOptions {
    bool brake = false;
    bool regulated = false;
};

QByteArray setOutputState(const OutputState &s, bool reply = false);
/** Spin a motor at `power`, or brake/coast when power is 0. */
QByteArray motorPower(uint8_t port, int power, MotorOptions opts = {});
QByteArray playTone(int frequencyHz, int durationMs);
QByteArray setInputMode(uint8_t port, uint8_t type, uint8_t mode);
QByteArray getInputValues(uint8_t port);
QByteArray getOutputState(uint8_t port);
QByteArray resetMotorPosition(uint8_t port, bool relative = false);
QByteArray getBatteryLevel();
QByteArray keepAlive();
QByteArray stopProgram();
QByteArray getFirmwareVersion();
QByteArray getDeviceInfo();

bool expectsReply(const QByteArray &packet);

int parseBatteryLevel(const QByteArray &reply);

struct InputValues {
    uint8_t port = 0;
    bool valid = false;
    bool calibrated = false;
    uint8_t type = 0;
    uint8_t mode = 0;
    uint16_t raw = 0;
    uint16_t normalized = 0;
    int16_t scaled = 0;
};
InputValues parseInputValues(const QByteArray &reply);

struct OutputStateReply {
    uint8_t port = 0;
    int power = 0;
    uint8_t mode = 0;
    uint8_t regulation = 0;
    int turnRatio = 0;
    uint8_t runState = 0;
    uint32_t tachoLimit = 0;
    int32_t tachoCount = 0;
    int32_t blockTachoCount = 0;
    int32_t rotationCount = 0;
};
OutputStateReply parseOutputState(const QByteArray &reply);

struct FirmwareVersion {
    QString protocol;
    QString firmware;
};
FirmwareVersion parseFirmwareVersion(const QByteArray &reply);

struct DeviceInfo {
    QString name;
    QString bluetoothAddress;
    uint32_t freeFlash = 0;
};
DeviceInfo parseDeviceInfo(const QByteArray &reply);

} // namespace nxt
