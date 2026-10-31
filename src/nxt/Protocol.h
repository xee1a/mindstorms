#pragma once
// LEGO MINDSTORMS NXT direct/system command packets (USB framing: no length prefix).
// Reference: "LEGO MINDSTORMS NXT Bluetooth Developer Kit", appendix 1 & 2.

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <optional>
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
constexpr uint8_t PlaySoundFile = 0x02;
constexpr uint8_t PlayTone = 0x03;
constexpr uint8_t SetOutputState = 0x04;
constexpr uint8_t SetInputMode = 0x05;
constexpr uint8_t GetOutputState = 0x06;
constexpr uint8_t GetInputValues = 0x07;
constexpr uint8_t ResetMotorPosition = 0x0a;
constexpr uint8_t GetBatteryLevel = 0x0b;
constexpr uint8_t StopSoundPlayback = 0x0c;
constexpr uint8_t KeepAlive = 0x0d;
constexpr uint8_t MessageWrite = 0x09;
constexpr uint8_t GetCurrentProgramName = 0x11;
} // namespace DirectOp

namespace SystemOp {
constexpr uint8_t OpenRead = 0x80;
constexpr uint8_t OpenWrite = 0x81;
constexpr uint8_t Read = 0x82;
constexpr uint8_t Write = 0x83;
constexpr uint8_t Close = 0x84;
constexpr uint8_t Delete = 0x85;
constexpr uint8_t FindFirst = 0x86;
constexpr uint8_t FindNext = 0x87;
constexpr uint8_t GetFirmwareVersion = 0x88;
constexpr uint8_t SetBrickName = 0x98;
constexpr uint8_t GetDeviceInfo = 0x9b;
} // namespace SystemOp

/** System command status bytes the app handles on purpose. */
namespace FileStatus {
constexpr uint8_t NoSpace = 0x82;
constexpr uint8_t NoMoreFiles = 0x83;
constexpr uint8_t FileNotFound = 0x87;
constexpr uint8_t FileExists = 0x8f;
} // namespace FileStatus

/** Largest data chunk one USB packet carries in a WRITE command (64 bytes minus the 3-byte header). */
constexpr int MaxWriteChunk = 61;
/** Largest data chunk one USB reply carries for a READ command (64 bytes minus the 6-byte header). */
constexpr int MaxReadChunk = 58;
/** Inbox (0-based) the app streams the gamepad into, for programs that use pad blocks. */
constexpr int PadMailbox = 9;

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
/** Plays a .rso file stored on the brick, e.g. "! Click.rso". */
QByteArray playSoundFile(const QString &name, bool loop = false);
QByteArray stopSoundPlayback();
QByteArray setInputMode(uint8_t port, uint8_t type, uint8_t mode);
QByteArray getInputValues(uint8_t port);
QByteArray getOutputState(uint8_t port);
QByteArray resetMotorPosition(uint8_t port, bool relative = false);
QByteArray getBatteryLevel();
QByteArray keepAlive();
QByteArray stopProgram();
QByteArray getFirmwareVersion();
QByteArray getDeviceInfo();
/** Starts a program stored on the brick, e.g. "robot.rxe". */
QByteArray startProgram(const QString &name);
QByteArray getCurrentProgramName();
/** Puts a message (at most 58 bytes) into one of the brick's ten inboxes, without a reply. */
QByteArray messageWrite(int mailbox, const QByteArray &message);

// System commands for files. Names are 15.3 characters at most.
QByteArray openRead(const QString &name);
QByteArray readChunk(uint8_t handle, int count);
QByteArray openWrite(const QString &name, uint32_t size);
QByteArray writeChunk(uint8_t handle, const QByteArray &data);
QByteArray closeHandle(uint8_t handle);
QByteArray deleteFile(const QString &name);
/** `pattern` may be "*.*", "*.rxe" and so on. */
QByteArray findFirst(const QString &pattern);
QByteArray findNext(uint8_t handle);
QByteArray setBrickName(const QString &name);

bool expectsReply(const QByteArray &packet);

int parseBatteryLevel(const QByteArray &reply);
/** Status byte of a reply, after checking its header. */
uint8_t replyStatus(const QByteArray &reply, uint8_t op);
/** Throws NxtError when the command failed. */
void checkStatus(const QByteArray &reply, uint8_t op);
/** Handle from an OPEN WRITE reply. Throws NxtError on failure. */
uint8_t parseOpenWrite(const QByteArray &reply);

struct OpenedFile {
    uint8_t handle = 0;
    uint32_t size = 0;
};
/** Handle and size from an OPEN READ reply. Throws NxtError on failure. */
OpenedFile parseOpenRead(const QByteArray &reply);
/** Data from a READ reply. Throws NxtError on failure. */
QByteArray parseRead(const QByteArray &reply);

struct FoundFile {
    uint8_t handle = 0;
    QString name;
    uint32_t size = 0;
};
/** FIND FIRST / FIND NEXT reply; empty when there are no (more) files. */
std::optional<FoundFile> parseFind(const QByteArray &reply, uint8_t op);
/** Name of the program running on the brick, or empty when none is. */
QString parseCurrentProgramName(const QByteArray &reply);

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
