#include "drive/Mixer.h"
#include "nxt/Protocol.h"

#include <QTest>

using namespace nxt;
using drive::ControlInput;
using drive::DriveConfig;
using drive::DriveMode;

namespace {

QByteArray hex(const char *s) { return QByteArray::fromHex(s); }

DriveConfig linear()
{
    DriveConfig c;
    c.expo = 0;
    c.deadzone = 0;
    return c;
}

using Power = std::array<int, 3>;

} // namespace

class TestCore : public QObject {
    Q_OBJECT
private slots:
    // ── Protocol ──────────────────────────────────────
    void motorPowerEncodesNoReplySetOutputState() { QCOMPARE(motorPower(1, 75), hex("8004014b0300002000000000")); }
    void negativePowerAndRegulation() { QCOMPARE(motorPower(0, -100, {false, true}), hex("8004009c0701002000000000")); }
    void zeroPowerBrakesOrCoasts()
    {
        QCOMPARE(motorPower(PortAll, 0, {true, false}), hex("8004ff000701002000000000"));
        QCOMPARE(motorPower(2, 0), hex("800402000000000000000000"));
    }
    void tachoLimitLittleEndian()
    {
        OutputState s;
        s.port = 0;
        s.power = 50;
        s.mode = 1;
        s.runState = RunState::Running;
        s.tachoLimit = 360;
        QCOMPARE(setOutputState(s).mid(8), hex("68010000"));
    }
    void playToneClampsAndEncodes() { QCOMPARE(playTone(440, 500), hex("8003b801f401")); }
    void expectsReplyFollowsBit()
    {
        QVERIFY(expectsReply(getBatteryLevel()));
        QVERIFY(!expectsReply(playTone(440, 100)));
        QVERIFY(expectsReply(getFirmwareVersion()));
    }
    void parsesBattery() { QCOMPARE(parseBatteryLevel(hex("020b008c1e")), 7820); }
    void errorStatusThrows() { QVERIFY_THROWS_EXCEPTION(NxtError, parseBatteryLevel(hex("020bec0000"))); }
    void shortReplyThrows() { QVERIFY_THROWS_EXCEPTION(ProtocolError, parseBatteryLevel(hex("020b00"))); }
    void parsesInputValues()
    {
        const InputValues v = parseInputValues(hex("02070002010005801002000237000000"));
        QCOMPARE(v.port, 2);
        QVERIFY(v.valid);
        QCOMPARE(v.type, SensorType::LightActive);
        QCOMPARE(v.raw, 0x210);
        QCOMPARE(v.scaled, 55);
    }
    void parsesOutputStateNegativeRotation()
    {
        QByteArray r(25, '\0');
        r[0] = 0x02;
        r[1] = 0x06;
        r[3] = 1;
        r[4] = '\xce';
        const int32_t rot = -720;
        memcpy(r.data() + 21, &rot, 4);
        const OutputStateReply s = parseOutputState(r);
        QCOMPARE(s.power, -50);
        QCOMPARE(s.rotationCount, -720);
    }
    void parsesFirmwareAndDeviceInfo()
    {
        const FirmwareVersion fw = parseFirmwareVersion(hex("028800" "7c011f01"));
        QCOMPARE(fw.protocol, QStringLiteral("1.124"));
        QCOMPARE(fw.firmware, QStringLiteral("1.31"));
        QByteArray info(33, '\0');
        info[0] = 0x02;
        info[1] = '\x9b';
        info.replace(3, 5, "ROVER");
        info.replace(18, 6, hex("001653010203"));
        const uint32_t flash = 1234;
        memcpy(info.data() + 29, &flash, 4);
        const DeviceInfo d = parseDeviceInfo(info);
        QCOMPARE(d.name, QStringLiteral("ROVER"));
        QCOMPARE(d.bluetoothAddress, QStringLiteral("00:16:53:01:02:03"));
        QCOMPARE(d.freeFlash, 1234u);
    }

    // ── Mixer ─────────────────────────────────────────
    void deadzone()
    {
        QCOMPARE(drive::applyDeadzone(0.05, 0.05, 0.1), (std::pair<double, double>{0, 0}));
        QCOMPARE(drive::applyDeadzone(1, 0, 0.2).first, 1.0);
        QVERIFY(qAbs(drive::applyDeadzone(0.6, 0, 0.2).first - 0.5) < 1e-9);
    }
    void neutralStops() { QCOMPARE(drive::mix({}, DriveConfig{}), (Power{0, 0, 0})); }
    void arcadeForward()
    {
        ControlInput in;
        in.leftY = 1;
        QCOMPARE(drive::mix(in, linear()), (Power{0, 100, 100}));
    }
    void arcadeSpin()
    {
        ControlInput in;
        in.rightX = 1;
        QCOMPARE(drive::mix(in, linear()), (Power{0, 100, -100}));
    }
    void arcadeForwardAndTurnKeepsRatio()
    {
        ControlInput in;
        in.leftY = 1;
        in.rightX = 0.5;
        QCOMPARE(drive::mix(in, linear()), (Power{0, 100, 33}));
    }
    void tank()
    {
        ControlInput in;
        in.leftY = 1;
        in.rightY = -0.5;
        DriveConfig c = linear();
        c.mode = DriveMode::Tank;
        QCOMPARE(drive::mix(in, c), (Power{0, 100, -50}));
    }
    void racing()
    {
        DriveConfig c = linear();
        c.mode = DriveMode::Racing;
        ControlInput in;
        in.rightTrigger = 1;
        QCOMPARE(drive::mix(in, c), (Power{0, 100, 100}));
        ControlInput back;
        back.leftTrigger = 0.5;
        QCOMPARE(drive::mix(back, c), (Power{0, -50, -50}));
    }
    void portsInversionAndMaxPower()
    {
        DriveConfig c = linear();
        c.maxPower = 50;
        c.leftMotor = 2;
        c.rightMotor = 0;
        c.invertRight = true;
        c.auxMotor = 1;
        ControlInput in;
        in.leftY = 1;
        in.rightBumper = true;
        QCOMPARE(drive::mix(in, c), (Power{-50, 60, 50}));
    }
    void auxIgnoredWhenItCollides()
    {
        DriveConfig c = linear();
        c.auxMotor = 1;
        ControlInput in;
        in.leftBumper = true;
        QCOMPARE(drive::mix(in, c), (Power{0, 0, 0}));
    }
};

QTEST_APPLESS_MAIN(TestCore)
#include "test_core.moc"
