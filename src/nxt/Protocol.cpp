#include "Protocol.h"

#include <QStringList>
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace nxt {

namespace {

const char *statusMessage(uint8_t status)
{
    switch (status) {
    case 0x20: return "Pending communication transaction in progress";
    case 0x81: return "No more handles";
    case 0x82: return "No space";
    case 0x83: return "No more files";
    case 0x84: return "End of file expected";
    case 0x85: return "End of file";
    case 0x86: return "Not a linear file";
    case 0x87: return "File not found";
    case 0x88: return "Handle already closed";
    case 0x89: return "No linear space";
    case 0x8a: return "Undefined error";
    case 0x8b: return "File is busy";
    case 0x8c: return "No write buffers";
    case 0x8d: return "Append not possible";
    case 0x8e: return "File is full";
    case 0x8f: return "File exists";
    case 0x90: return "Module not found";
    case 0x91: return "Out of boundary";
    case 0x92: return "Illegal file name";
    case 0x93: return "Illegal handle";
    case 0x40: return "Specified mailbox queue is empty";
    case 0xbd: return "Request failed (i.e. specified file not found)";
    case 0xbe: return "Unknown command opcode";
    case 0xbf: return "Insane packet";
    case 0xc0: return "Data contains out-of-range values";
    case 0xdd: return "Communication bus error";
    case 0xde: return "No free memory in communication buffer";
    case 0xdf: return "Specified channel/connection is not valid";
    case 0xe0: return "Specified channel/connection not configured or busy";
    case 0xec: return "No active program";
    case 0xed: return "Illegal size specified";
    case 0xee: return "Illegal mailbox queue ID specified";
    case 0xef: return "Attempted to access invalid field of a structure";
    case 0xf0: return "Bad input or output specified";
    case 0xfb: return "Insufficient memory available";
    case 0xff: return "Bad arguments";
    default: return nullptr;
    }
}

int clampInt(double v, int lo, int hi)
{
    return std::clamp(static_cast<int>(std::lround(v)), lo, hi);
}

QByteArray bytes(std::initializer_list<int> values)
{
    QByteArray out;
    out.reserve(static_cast<qsizetype>(values.size()));
    for (int v : values) out.append(static_cast<char>(v & 0xff));
    return out;
}

QByteArray direct(uint8_t op, bool reply, const QByteArray &payload = {})
{
    QByteArray out;
    out.append(static_cast<char>(reply ? CommandType::Direct : (CommandType::Direct | CommandType::NoReply)));
    out.append(static_cast<char>(op));
    out.append(payload);
    return out;
}

void appendU16(QByteArray &b, uint32_t v)
{
    b.append(static_cast<char>(v & 0xff));
    b.append(static_cast<char>((v >> 8) & 0xff));
}

void appendU32(QByteArray &b, uint32_t v)
{
    for (int i = 0; i < 4; ++i) b.append(static_cast<char>((v >> (8 * i)) & 0xff));
}

// Returns int (not uint8_t) so QString::arg() formats it as a number, not a character.
int u8(const QByteArray &b, int i) { return static_cast<uint8_t>(b.at(i)); }
int hex(uint8_t v) { return v; }
int8_t s8(const QByteArray &b, int i) { return static_cast<int8_t>(b.at(i)); }
uint16_t u16(const QByteArray &b, int i) { return static_cast<uint16_t>(u8(b, i) | (u8(b, i + 1) << 8)); }
int16_t s16(const QByteArray &b, int i) { return static_cast<int16_t>(u16(b, i)); }
uint32_t u32(const QByteArray &b, int i)
{
    return static_cast<uint32_t>(u8(b, i)) | (static_cast<uint32_t>(u8(b, i + 1)) << 8) |
           (static_cast<uint32_t>(u8(b, i + 2)) << 16) | (static_cast<uint32_t>(u8(b, i + 3)) << 24);
}
int32_t s32(const QByteArray &b, int i) { return static_cast<int32_t>(u32(b, i)); }

/** Filename: 15.3 characters plus a terminating zero, padded to 20 bytes. */
QByteArray fileName20(const QString &name)
{
    QByteArray file = name.toLatin1().left(19);
    file.append(QByteArray(20 - file.size(), char(0)));
    return file;
}

QByteArray system(uint8_t op, const QByteArray &payload = {})
{
    QByteArray out;
    out.append(static_cast<char>(CommandType::System));
    out.append(static_cast<char>(op));
    out.append(payload);
    return out;
}

QString zeroTerminated(const QByteArray &b, int from, int length)
{
    QByteArray s = b.mid(from, length);
    const qsizetype end = s.indexOf(char(0));
    if (end >= 0) s.truncate(end);
    return QString::fromLatin1(s);
}

/** Validates the reply header and status byte; `minLength` covers the payload the caller reads. */
void checkReply(const QByteArray &reply, uint8_t op, int minLength)
{
    if (reply.size() < 3 || u8(reply, 0) != CommandType::Reply || u8(reply, 1) != op)
        throw ProtocolError(QStringLiteral("Unexpected reply for command 0x%1").arg(hex(op), 2, 16, QLatin1Char('0')).toStdString());
    if (u8(reply, 2) != 0) throw NxtError(static_cast<uint8_t>(u8(reply, 2)), op);
    if (reply.size() < minLength)
        throw ProtocolError(QStringLiteral("Reply for command 0x%1 is too short").arg(hex(op), 2, 16, QLatin1Char('0')).toStdString());
}

} // namespace

NxtError::NxtError(uint8_t s, uint8_t op)
    : std::runtime_error([&] {
          const char *msg = statusMessage(s);
          QString text = msg ? QString::fromLatin1(msg) : QStringLiteral("Unknown error 0x%1").arg(hex(s), 2, 16, QLatin1Char('0'));
          return QStringLiteral("NXT command 0x%1 failed: %2").arg(hex(op), 2, 16, QLatin1Char('0')).arg(text).toStdString();
      }()),
      status(s)
{
}

QByteArray setOutputState(const OutputState &s, bool reply)
{
    QByteArray payload = bytes({s.port, clampInt(s.power, -100, 100), s.mode, s.regulation,
                                clampInt(s.turnRatio, -100, 100), s.runState});
    appendU32(payload, s.tachoLimit);
    return direct(DirectOp::SetOutputState, reply, payload);
}

QByteArray motorPower(uint8_t port, int power, MotorOptions opts)
{
    const int p = std::clamp(power, -100, 100);
    OutputState s;
    s.port = port;
    if (p == 0) {
        if (opts.brake) {
            s.mode = OutputMode::MotorOn | OutputMode::Brake | OutputMode::Regulated;
            s.regulation = RegulationMode::Speed;
            s.runState = RunState::Running;
        }
        return setOutputState(s);
    }
    s.power = p;
    s.mode = OutputMode::MotorOn | OutputMode::Brake | (opts.regulated ? OutputMode::Regulated : 0);
    s.regulation = opts.regulated ? RegulationMode::Speed : RegulationMode::Idle;
    s.runState = RunState::Running;
    return setOutputState(s);
}

QByteArray playTone(int frequencyHz, int durationMs)
{
    QByteArray payload;
    appendU16(payload, static_cast<uint32_t>(std::clamp(frequencyHz, 200, 14000)));
    appendU16(payload, static_cast<uint32_t>(std::clamp(durationMs, 0, 0xffff)));
    return direct(DirectOp::PlayTone, false, payload);
}

QByteArray playSoundFile(const QString &name, bool loop)
{
    QByteArray payload;
    payload.append(static_cast<char>(loop ? 1 : 0));
    payload.append(fileName20(name));
    return direct(DirectOp::PlaySoundFile, false, payload);
}

QByteArray stopSoundPlayback() { return direct(DirectOp::StopSoundPlayback, false); }

QByteArray setInputMode(uint8_t port, uint8_t type, uint8_t mode)
{
    return direct(DirectOp::SetInputMode, false, bytes({port, type, mode}));
}

QByteArray getInputValues(uint8_t port) { return direct(DirectOp::GetInputValues, true, bytes({port})); }
QByteArray getOutputState(uint8_t port) { return direct(DirectOp::GetOutputState, true, bytes({port})); }
QByteArray resetMotorPosition(uint8_t port, bool relative)
{
    return direct(DirectOp::ResetMotorPosition, false, bytes({port, relative ? 1 : 0}));
}
QByteArray getBatteryLevel() { return direct(DirectOp::GetBatteryLevel, true); }
QByteArray keepAlive() { return direct(DirectOp::KeepAlive, true); }
QByteArray stopProgram() { return direct(DirectOp::StopProgram, false); }
QByteArray getFirmwareVersion() { return bytes({CommandType::System, SystemOp::GetFirmwareVersion}); }
QByteArray getDeviceInfo() { return bytes({CommandType::System, SystemOp::GetDeviceInfo}); }

QByteArray startProgram(const QString &name) { return direct(DirectOp::StartProgram, true, fileName20(name)); }
QByteArray getCurrentProgramName() { return direct(DirectOp::GetCurrentProgramName, true); }

QByteArray messageWrite(int mailbox, const QByteArray &message)
{
    QByteArray msg = message.left(58);
    msg.append(char(0));
    QByteArray payload = bytes({std::clamp(mailbox, 0, 9), int(msg.size())});
    payload.append(msg);
    return direct(DirectOp::MessageWrite, false, payload);
}

QByteArray openRead(const QString &name) { return system(SystemOp::OpenRead, fileName20(name)); }

QByteArray readChunk(uint8_t handle, int count)
{
    QByteArray payload = bytes({handle});
    appendU16(payload, uint16_t(std::clamp(count, 0, MaxReadChunk)));
    return system(SystemOp::Read, payload);
}

QByteArray openWrite(const QString &name, uint32_t size)
{
    QByteArray payload = fileName20(name);
    appendU32(payload, size);
    return system(SystemOp::OpenWrite, payload);
}

QByteArray writeChunk(uint8_t handle, const QByteArray &data)
{
    QByteArray payload = bytes({handle});
    payload.append(data.left(MaxWriteChunk));
    return system(SystemOp::Write, payload);
}

QByteArray closeHandle(uint8_t handle) { return system(SystemOp::Close, bytes({handle})); }
QByteArray deleteFile(const QString &name) { return system(SystemOp::Delete, fileName20(name)); }
QByteArray findFirst(const QString &pattern) { return system(SystemOp::FindFirst, fileName20(pattern)); }
QByteArray findNext(uint8_t handle) { return system(SystemOp::FindNext, bytes({handle})); }

QByteArray setBrickName(const QString &name)
{
    QByteArray n = name.toLatin1().left(15);
    n.append(QByteArray(16 - n.size(), char(0)));
    return system(SystemOp::SetBrickName, n);
}

bool expectsReply(const QByteArray &packet)
{
    return !packet.isEmpty() && (u8(packet, 0) & CommandType::NoReply) == 0;
}

int parseBatteryLevel(const QByteArray &reply)
{
    checkReply(reply, DirectOp::GetBatteryLevel, 5);
    return u16(reply, 3);
}

uint8_t replyStatus(const QByteArray &reply, uint8_t op)
{
    if (reply.size() < 3 || u8(reply, 0) != CommandType::Reply || u8(reply, 1) != op)
        throw ProtocolError(QStringLiteral("Unexpected reply for command 0x%1").arg(hex(op), 2, 16, QLatin1Char('0')).toStdString());
    return static_cast<uint8_t>(u8(reply, 2));
}

void checkStatus(const QByteArray &reply, uint8_t op) { checkReply(reply, op, 3); }

uint8_t parseOpenWrite(const QByteArray &reply)
{
    checkReply(reply, SystemOp::OpenWrite, 4);
    return static_cast<uint8_t>(u8(reply, 3));
}

OpenedFile parseOpenRead(const QByteArray &reply)
{
    checkReply(reply, SystemOp::OpenRead, 8);
    return {static_cast<uint8_t>(u8(reply, 3)), u32(reply, 4)};
}

QByteArray parseRead(const QByteArray &reply)
{
    checkReply(reply, SystemOp::Read, 6);
    const int count = u16(reply, 4);
    if (reply.size() < 6 + count) throw ProtocolError("Short READ reply");
    return reply.mid(6, count);
}

std::optional<FoundFile> parseFind(const QByteArray &reply, uint8_t op)
{
    const uint8_t status = replyStatus(reply, op);
    if (status == FileStatus::FileNotFound || status == FileStatus::NoMoreFiles) return std::nullopt;
    checkReply(reply, op, 28);
    FoundFile f;
    f.handle = static_cast<uint8_t>(u8(reply, 3));
    f.name = zeroTerminated(reply, 4, 20);
    f.size = u32(reply, 24);
    return f;
}

QString parseCurrentProgramName(const QByteArray &reply)
{
    if (replyStatus(reply, DirectOp::GetCurrentProgramName) == 0xec) return {}; // no active program
    checkReply(reply, DirectOp::GetCurrentProgramName, 23);
    return zeroTerminated(reply, 3, 20);
}

InputValues parseInputValues(const QByteArray &reply)
{
    checkReply(reply, DirectOp::GetInputValues, 14);
    InputValues v;
    v.port = static_cast<uint8_t>(u8(reply, 3));
    v.valid = u8(reply, 4) != 0;
    v.calibrated = u8(reply, 5) != 0;
    v.type = static_cast<uint8_t>(u8(reply, 6));
    v.mode = static_cast<uint8_t>(u8(reply, 7));
    v.raw = u16(reply, 8);
    v.normalized = u16(reply, 10);
    v.scaled = s16(reply, 12);
    return v;
}

OutputStateReply parseOutputState(const QByteArray &reply)
{
    checkReply(reply, DirectOp::GetOutputState, 25);
    OutputStateReply s;
    s.port = static_cast<uint8_t>(u8(reply, 3));
    s.power = s8(reply, 4);
    s.mode = static_cast<uint8_t>(u8(reply, 5));
    s.regulation = static_cast<uint8_t>(u8(reply, 6));
    s.turnRatio = s8(reply, 7);
    s.runState = static_cast<uint8_t>(u8(reply, 8));
    s.tachoLimit = u32(reply, 9);
    s.tachoCount = s32(reply, 13);
    s.blockTachoCount = s32(reply, 17);
    s.rotationCount = s32(reply, 21);
    return s;
}

FirmwareVersion parseFirmwareVersion(const QByteArray &reply)
{
    checkReply(reply, SystemOp::GetFirmwareVersion, 7);
    return {QStringLiteral("%1.%2").arg(u8(reply, 4)).arg(u8(reply, 3)),
            QStringLiteral("%1.%2").arg(u8(reply, 6)).arg(u8(reply, 5), 2, 10, QLatin1Char('0'))};
}

DeviceInfo parseDeviceInfo(const QByteArray &reply)
{
    checkReply(reply, SystemOp::GetDeviceInfo, 33);
    DeviceInfo info;
    QByteArray name = reply.mid(3, 15);
    const qsizetype end = name.indexOf('\0');
    if (end >= 0) name.truncate(end);
    info.name = QString::fromLatin1(name);
    QStringList bt;
    for (int i = 18; i < 24; ++i) bt << QStringLiteral("%1").arg(u8(reply, i), 2, 16, QLatin1Char('0'));
    info.bluetoothAddress = bt.join(QLatin1Char(':'));
    info.freeFlash = u32(reply, 29);
    return info;
}

} // namespace nxt
