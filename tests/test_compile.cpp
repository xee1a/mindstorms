// Every block must turn into NXC that the bundled nbc accepts for the standard firmware.
#include "blocks/BlockDefs.h"
#include "compile/NxcGenerator.h"
#include "drive/PadLink.h"
#include "nxt/Protocol.h"
#include "nxt/SystemFiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace blocks;

namespace {

QJsonObject blockJson(const BlockDef &def)
{
    QJsonObject fields;
    const QMap<QString, QString> f = defaultFields(def);
    for (auto it = f.cbegin(); it != f.cend(); ++it) fields.insert(it.key(), it.value().isEmpty() && def.arg(it.key())->kind == ArgKind::Variable ? QStringLiteral("x") : it.value());
    QJsonObject b{{"type", def.type}, {"fields", fields}};
    // Fill condition slots so loops and ifs have something real to test.
    QJsonObject inputs;
    for (const ArgDef &a : def.args)
        if (a.kind == ArgKind::Bool) inputs.insert(a.name, QJsonObject{{"type", "logic_boolean"}, {"fields", QJsonObject{{"BOOL", "TRUE"}}}});
    if (!inputs.isEmpty()) b.insert("inputs", inputs);
    QJsonObject statements;
    for (const RowDef &r : def.rows)
        if (r.statement) statements.insert(r.name, QJsonObject{{"type", "nxt_beep"}});
    if (!statements.isEmpty()) b.insert("statements", statements);
    return b;
}

/** A program that uses `def` once: hats start a stack, statements sit under "when program starts", values feed a variable or an if. */
QJsonObject programFor(const BlockDef &def)
{
    QJsonObject block = blockJson(def);
    QJsonObject top;
    if (def.shape == Shape::Hat) {
        block.insert("next", QJsonObject{{"type", "nxt_beep"}});
        top = block;
    } else {
        QJsonObject stmt;
        if (def.shape == Shape::Reporter)
            stmt = QJsonObject{{"type", "data_set"}, {"fields", QJsonObject{{"VAR", "x"}, {"VALUE", "0"}}}, {"inputs", QJsonObject{{"VALUE", block}}}};
        else if (def.shape == Shape::Boolean)
            stmt = QJsonObject{{"type", "nxt_if"}, {"inputs", QJsonObject{{"CONDITION", block}}}, {"statements", QJsonObject{{"DO", QJsonObject{{"type", "nxt_beep"}}}}}};
        else
            stmt = block;
        top = QJsonObject{{"type", "nxt_when_start"}, {"next", stmt}};
    }
    return QJsonObject{{"variables", QJsonArray{"x"}}, {"stacks", QJsonArray{QJsonObject{{"x", 0}, {"y", 0}, {"block", top}}}}};
}

/** Compiles NXC with nbc; returns its error output, or empty when it compiled. */
QString nbcErrors(const QString &nbc, const QString &source)
{
    QTemporaryDir dir;
    QFile f(dir.filePath(QStringLiteral("p.nxc")));
    if (!f.open(QIODevice::WriteOnly)) return QStringLiteral("can't write");
    f.write(source.toLatin1());
    f.close();
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(nbc, {QStringLiteral("-O=%1").arg(dir.filePath(QStringLiteral("p.rxe"))), f.fileName()});
    p.waitForFinished(60000);
    if (p.exitCode() == 0) return {};
    QStringList errors;
    for (const QString &l : QString::fromLocal8Bit(p.readAll()).split(QChar(10)))
        if (!l.startsWith(QLatin1String("# Status"))) errors << l.trimmed();
    return errors.join(QChar(10));
}

/** Uses nested values, parallel branches, messages, the pad, mailboxes and both light modes together. */
constexpr char Nested[] = R"JSON({"variables": ["x", "speed"], "stacks": [
 {"block": {"type": "nxt_when_start", "next": {"type": "nxt_move_set_motors", "fields": {"LEFT": "C", "RIGHT": "B"}, "next":
  {"type": "nxt_forever", "statements": {"DO":
   {"type": "nxt_if_else", "inputs": {"CONDITION": {"type": "pad_button", "fields": {"BUTTON": "0"}}},
    "statements": {"DO": {"type": "nxt_move_steer_for", "fields": {"STEER": "0", "AMOUNT": "1", "UNIT": "ROTATIONS"},
                          "inputs": {"STEER": {"type": "math_arithmetic", "fields": {"A": "0", "OP": "MUL", "B": "2"},
                                               "inputs": {"A": {"type": "pad_axis", "fields": {"AXIS": "0"}}}}}},
                   "ELSE": {"type": "pad_drive", "fields": {"LEFT": "B", "RIGHT": "C"}}},
    "next": {"type": "nxt_display_text", "fields": {"TEXT": "", "LINE": "2"},
             "inputs": {"TEXT": {"type": "text_join", "fields": {"A": "d=", "B": ""}, "inputs": {"B": {"type": "nxt_ultrasonic", "fields": {"PORT": "4"}}}}},
    "next": {"type": "nxt_if", "inputs": {"CONDITION": {"type": "logic_compare", "fields": {"A": "", "OP": "EQ", "B": ""},
              "inputs": {"A": {"type": "text_join", "fields": {"A": "a", "B": "b"}}, "B": {"type": "text_join", "fields": {"A": "a", "B": "b"}}}}},
             "statements": {"DO": {"type": "nxt_beep"}},
    "next": {"type": "nxt_run_parallel",
             "statements": {"A": {"type": "nxt_motor_for", "fields": {"PORT": "A", "POWER": "50", "AMOUNT": "90", "UNIT": "DEGREES"},
                                  "inputs": {"POWER": {"type": "math_random", "fields": {"FROM": "10", "TO": "50"}}}},
                            "B": {"type": "nxt_move_tank_for", "fields": {"LEFT": "30", "RIGHT": "0", "AMOUNT": "1", "UNIT": "SECONDS"},
                                  "inputs": {"RIGHT": {"type": "math_arithmetic", "fields": {"A": "100", "OP": "DIV", "B": "3"}}}}},
    "next": {"type": "data_change", "fields": {"VAR": "x", "VALUE": "0"}, "inputs": {"VALUE": {"type": "nxt_light", "fields": {"PORT": "3", "MODE": "AMBIENT"}}},
    "next": {"type": "data_set", "fields": {"VAR": "speed", "VALUE": "0"}, "inputs": {"VALUE": {"type": "nxt_light", "fields": {"PORT": "3", "MODE": "REFLECTED"}}},
    "next": {"type": "data_show", "fields": {"VAR": "x", "LINE": "3"},
    "next": {"type": "nxt_display_text", "fields": {"TEXT": "", "LINE": "4"}, "inputs": {"TEXT": {"type": "nxt_touch_pressed", "fields": {"PORT": "1"}}},
    "next": {"type": "nxt_broadcast_wait", "fields": {"MESSAGE": "1"},
    "next": {"type": "nxt_if", "inputs": {"CONDITION": {"type": "math_between", "fields": {"VALUE": "0", "LOW": "0", "HIGH": "10"},
              "inputs": {"VALUE": {"type": "nxt_touch_count", "fields": {"PORT": "1"}}}}}, "statements": {"DO": {"type": "nxt_break"}}}}}}}}}}}}}}}}},
 {"block": {"type": "nxt_when_message", "fields": {"MESSAGE": "1"}, "next": {"type": "nxt_play_note", "fields": {"NOTE": "523", "BEATS": "1"},
   "next": {"type": "nxt_stop_other"}}}},
 {"block": {"type": "pad_when_button", "fields": {"BUTTON": "7"}, "next": {"type": "nxt_stop_program"}}},
 {"block": {"type": "nxt_when_touch", "fields": {"PORT": "1", "STATE": "PRESSED"}, "next": {"type": "nxt_mailbox_send", "fields": {"VALUE": "0", "BOX": "1", "CONN": "1"},
   "inputs": {"VALUE": {"type": "nxt_mailbox_number", "fields": {"BOX": "2"}}}}}}
]})JSON";

struct Job {
    QString type;
    QString source;
    std::unique_ptr<QProcess> process;
};

} // namespace

class TestCompile : public QObject {
    Q_OBJECT
private slots:
    void padMessageRoundTrip();
    void protocolFileCommands();
    void systemSounds();
    void plainText();
    void sensorConflictIsAnError();
    void emptyWorkspaceIsAnError();
    void everyBlockCompiles();
    void nestedProgramCompiles();
    void gyroTurnCompiles();
};

void TestCompile::padMessageRoundTrip()
{
    const QByteArray m = drive::encodePadMessage({-100, 100, 0, 5, 0, 100}, 0b1010'0000'0000'0001);
    QCOMPARE(m.size(), 11);
    QCOMPARE(m.at(0), 'P');
    QCOMPARE(int(uchar(m.at(1))), 1);
    QCOMPARE(int(uchar(m.at(2))), 201);
    QCOMPARE(int(uchar(m.at(7))) & 15, 1);
    QCOMPARE(int(uchar(m.at(10))) & 15, 0b1010);
    QVERIFY(!m.contains(char(0)));
}

void TestCompile::protocolFileCommands()
{
    QCOMPARE(nxt::deleteFile("a.rxe").toHex(), QByteArray("0185612e727865") + QByteArray(30, '0'));
    QCOMPARE(nxt::openWrite("a.rxe", 300).right(4).toHex(), QByteArray("2c010000"));
    QCOMPARE(nxt::messageWrite(9, "hi").toHex(), QByteArray("8009090368690" "0"));
    const auto found = nxt::parseFind(QByteArray::fromHex("028600") + QByteArray(1, char(3)) + QByteArray("x.rxe").leftJustified(20, char(0)) +
                                          QByteArray::fromHex("10000000"),
                                      nxt::SystemOp::FindFirst);
    QVERIFY(found);
    QCOMPARE(found->name, QStringLiteral("x.rxe"));
    QCOMPARE(found->size, 16u);
    QVERIFY(!nxt::parseFind(QByteArray::fromHex("028687"), nxt::SystemOp::FindFirst));

    QCOMPARE(nxt::readChunk(4, 500).toHex(), QByteArray("0182043a00"));
    const auto opened = nxt::parseOpenRead(QByteArray::fromHex("02800005e5000000"));
    QCOMPARE(int(opened.handle), 5);
    QCOMPARE(opened.size, 229u);
    QCOMPARE(nxt::parseRead(QByteArray::fromHex("028200050300616263")), QByteArray("abc"));
    QVERIFY_THROWS_EXCEPTION(nxt::NxtError, nxt::parseOpenRead(QByteArray::fromHex("0280870000000000")));
}

void TestCompile::systemSounds()
{
    for (const char *name : {"! Startup.rso", "! Click.rso", "! Attention.rso"}) {
        const QByteArray s = nxt::systemfiles::ownSound(QString::fromLatin1(name));
        QVERIFY2(s.size() > 100 && s.size() < 6000, name);
        QCOMPARE(s.left(2).toHex(), QByteArray("0101"));
        QCOMPARE((uchar(s.at(2)) << 8 | uchar(s.at(3))), int(s.size() - 8));
        QCOMPARE((uchar(s.at(4)) << 8 | uchar(s.at(5))), 8000);
    }
    QVERIFY(nxt::systemfiles::ownSound("Hello.rso").isEmpty());
    QVERIFY(nxt::systemfiles::restorable("Try-Touch.rtm"));
    QVERIFY(!nxt::systemfiles::restorable("NVConfig.sys"));
    QVERIFY(!nxt::systemfiles::restorable("robot.rxe"));
    QVERIFY(nxt::systemfiles::restorable("Hello.rso")); // every sound is kept and put back
}

void TestCompile::plainText()
{
    QCOMPARE(compile::toNxtText(QStringLiteral("Zażółć gęślą jaźń é ☺")), QStringLiteral("Zazolc gesla jazn e ?"));
}

void TestCompile::sensorConflictIsAnError()
{
    const QJsonObject touch{{"type", "nxt_wait_touch"}, {"fields", QJsonObject{{"PORT", "1"}, {"STATE", "PRESSED"}}}};
    const QJsonObject sound{{"type", "nxt_play_tone"}, {"fields", QJsonObject{{"FREQ", "440"}, {"DURATION", "100"}}},
                            {"inputs", QJsonObject{{"FREQ", QJsonObject{{"type", "nxt_sound"}, {"fields", QJsonObject{{"PORT", "1"}}}}}}}};
    QJsonObject first = touch;
    first.insert("next", sound);
    const QJsonObject top{{"type", "nxt_when_start"}, {"next", first}};
    const QJsonObject stack{{"block", top}};
    const QJsonObject ws{{"stacks", QJsonArray{stack}}};
    const auto r = compile::generateNxc(ws, "t");
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.error.contains(QLatin1Char('1')));
}

void TestCompile::emptyWorkspaceIsAnError()
{
    QVERIFY(!compile::generateNxc({}, "t").error.isEmpty());
}

void TestCompile::everyBlockCompiles()
{
    const QString nbc = qEnvironmentVariable("NBC");
    if (nbc.isEmpty() || !QFile::exists(nbc)) QSKIP("NBC not set");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    dir.setAutoRemove(qEnvironmentVariableIsEmpty("KEEP_NXC"));
    if (!dir.autoRemove()) qInfo() << dir.path();
    std::vector<Job> jobs;
    for (const BlockDef &def : allDefs()) {
        const auto r = compile::generateNxc(programFor(def), def.type);
        QVERIFY2(r.error.isEmpty(), qPrintable(def.type + QStringLiteral(": ") + r.error));
        const QString src = dir.filePath(def.type + QStringLiteral(".nxc"));
        QFile f(src);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(r.source.toLatin1());
        jobs.push_back({def.type, r.source, nullptr});
    }
    // Compile a few at a time; each nbc run takes a moment.
    constexpr size_t Parallel = 8;
    for (size_t start = 0; start < jobs.size(); start += Parallel) {
        for (size_t i = start; i < std::min(jobs.size(), start + Parallel); ++i) {
            auto &j = jobs[i];
            j.process = std::make_unique<QProcess>();
            j.process->setProcessChannelMode(QProcess::MergedChannels);
            j.process->start(nbc, {QStringLiteral("-O=%1").arg(dir.filePath(j.type + ".rxe")), dir.filePath(j.type + ".nxc")});
        }
        for (size_t i = start; i < std::min(jobs.size(), start + Parallel); ++i) {
            auto &j = jobs[i];
            QVERIFY(j.process->waitForFinished(60000));
            QStringList errors;
            for (const QString &l : QString::fromLocal8Bit(j.process->readAll()).split(QChar(10)))
                if (!l.startsWith(QLatin1String("# Status"))) errors << l.trimmed();
            const QString out = errors.join(QChar(10));
            QVERIFY2(j.process->exitCode() == 0, qPrintable(j.type + QStringLiteral(":\n") + out));
        }
    }
}

void TestCompile::nestedProgramCompiles()
{
    const QString nbc = qEnvironmentVariable("NBC");
    if (nbc.isEmpty() || !QFile::exists(nbc)) QSKIP("NBC not set");
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(Nested), &err);
    QVERIFY2(err.error == QJsonParseError::NoError, qPrintable(err.errorString() + QString::number(err.offset)));
    const auto r = compile::generateNxc(doc.object(), QStringLiteral("Mój robot"));
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QVERIFY(r.source.contains(QLatin1String("ns_padReader")));
    QVERIFY(r.source.contains(QLatin1String("Moj robot")));
    const QString errors = nbcErrors(nbc, r.source);
    if (!errors.isEmpty() || !qEnvironmentVariableIsEmpty("PRINT_NXC")) qWarning().noquote() << r.source;
    QVERIFY2(errors.isEmpty(), qPrintable(errors));
}

/** Turns 90 degrees by the HiTechnic gyro on port 2, showing the angle, with a gyro on port 3 as well. */
constexpr char GyroTurn[] = R"JSON({"variables": [], "stacks": [
 {"block": {"type": "nxt_when_start", "next": {"type": "nxt_gyro_reset", "fields": {"PORT": "2"},
  "next": {"type": "nxt_move_steer_start", "fields": {"STEER": "100"},
  "next": {"type": "nxt_wait_gyro", "fields": {"PORT": "2", "OP": "GT", "VALUE": "90"},
  "next": {"type": "nxt_move_stop",
  "next": {"type": "nxt_display_number", "fields": {"VALUE": "0", "LINE": "1"}, "inputs": {"VALUE": {"type": "nxt_gyro_angle", "fields": {"PORT": "2"}}},
  "next": {"type": "nxt_gyro_calibrate", "fields": {"PORT": "3"},
  "next": {"type": "nxt_display_number", "fields": {"VALUE": "0", "LINE": "2"}, "inputs": {"VALUE": {"type": "nxt_gyro_rate", "fields": {"PORT": "3"}}}}}}}}}}}}
]})JSON";

void TestCompile::gyroTurnCompiles()
{
    const auto r = compile::generateNxc(QJsonDocument::fromJson(QByteArray(GyroTurn)).object(), QStringLiteral("gyro"));
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    // Both gyros are set to raw light-sensor mode and calibrated before the stacks start.
    QVERIFY(r.source.contains(QLatin1String("SetSensorMode(S2, SENSOR_MODE_RAW);")));
    QVERIFY(r.source.contains(QLatin1String("SetSensorMode(S3, SENSOR_MODE_RAW);")));
    const qsizetype main = r.source.indexOf(QLatin1String("task main()"));
    QVERIFY(r.source.indexOf(QLatin1String("ns_gyCalibrate(S2);"), main) > main);
    QVERIFY(r.source.indexOf(QLatin1String("ns_gyCalibrate(S3);"), main) > main);
    QVERIFY(r.source.contains(QLatin1String("until(ns_gyAng[1] > 90)")));
    QVERIFY(r.source.contains(QLatin1String("Precedes(ns_stack0, ns_gyroTask)")));

    // A gyro and another sensor can't share a port.
    const QJsonObject clash = QJsonDocument::fromJson(QByteArray(GyroTurn).replace("\"nxt_gyro_calibrate\", \"fields\": {\"PORT\": \"3\"}",
                                                                 "\"nxt_wait_touch\", \"fields\": {\"PORT\": \"2\", \"STATE\": \"PRESSED\"}"))
                .object();
    QVERIFY(!compile::generateNxc(clash, QStringLiteral("gyro")).error.isEmpty());

    const QString nbc = qEnvironmentVariable("NBC");
    if (nbc.isEmpty() || !QFile::exists(nbc)) QSKIP("NBC not set");
    const QString errors = nbcErrors(nbc, r.source);
    if (!errors.isEmpty() || !qEnvironmentVariableIsEmpty("PRINT_NXC")) qWarning().noquote() << r.source;
    QVERIFY2(errors.isEmpty(), qPrintable(errors));
}

QTEST_GUILESS_MAIN(TestCompile)
#include "test_compile.moc"
