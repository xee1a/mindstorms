#include "Runner.h"

#include "drive/Gamepad.h"
#include "drive/PadLink.h"
#include "nxt/Protocol.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QPointer>
#include <QRandomGenerator>
#include <QVariant>
#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace run {

namespace {

/** Translation context for this file's messages (lupdate reads Text::tr calls). */
struct Text {
    Q_DECLARE_TR_FUNCTIONS(Runner)
};

/** Thrown to unwind a stack when the program is stopped. */
struct Stopped {};
/** Thrown by "exit loop"; caught by the nearest loop. */
struct BreakLoop {};
struct RunError {
    QString message;
};

constexpr int PollMs = 10;
constexpr int BeatMs = 500;

} // namespace

struct Context {
    std::atomic<bool> stop{false};
    std::atomic<int> startStacks{0};
    bool hasEventStacks = false;
    QPointer<Runner> owner;

    std::mutex mu; // guards everything below
    nxt::BrickState state;
    std::array<int, 3> motors{};
    std::array<int, 3> speed{75, 75, 75};
    int moveLeft = 1, moveRight = 2, moveSpeed = 50;
    QHash<QString, QVariant> vars;
    QElapsedTimer timer;
    std::array<int, 4> bumps{};
    std::array<bool, 4> touchDown{};
    /** HiTechnic gyro per port: resting reading, angle added up from telemetry, and when it was last added to. */
    std::array<double, 4> gyroOffset{};
    std::array<double, 4> gyroAngle{};
    std::array<bool, 4> gyroReady{};
    std::array<qint64, 4> gyroLast{};
    QElapsedTimer clock; // unlike `timer`, never reset by the program
    QHash<QString, int> broadcasts;
    QHash<QString, int> busyReceivers;
    /** The PC's gamepad, copied from the UI thread. */
    drive::PadAxes padAxes{};
    std::array<bool, int(drive::PadButton::Count)> padButtons{};
    QString error;

    std::mutex threadsMu;
    std::vector<std::thread> threads;
};

namespace {

/** Runs on the UI thread, where nxt::Brick lives. */
void onUi(std::function<void()> f) { QMetaObject::invokeMethod(&nxt::Brick::instance(), std::move(f), Qt::QueuedConnection); }

QJsonObject fields(const QJsonObject &b) { return b.value(QStringLiteral("fields")).toObject(); }
QJsonObject inputs(const QJsonObject &b) { return b.value(QStringLiteral("inputs")).toObject(); }
QJsonObject statement(const QJsonObject &b, const char *name)
{
    return b.value(QStringLiteral("statements")).toObject().value(QLatin1String(name)).toObject();
}
QString type(const QJsonObject &b) { return b.value(QStringLiteral("type")).toString(); }

double toNumber(const QVariant &v)
{
    if (v.typeId() == QMetaType::Bool) return v.toBool() ? 1 : 0;
    bool ok = false;
    const double d = v.toString().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    return ok ? d : 0;
}

bool toBool(const QVariant &v)
{
    if (v.typeId() == QMetaType::Bool) return v.toBool();
    const QString s = v.toString();
    return !(s.isEmpty() || s == QLatin1String("0") || s.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0);
}

QString toText(const QVariant &v)
{
    if (v.typeId() == QMetaType::Double) {
        const double d = v.toDouble();
        return d == std::floor(d) && std::abs(d) < 1e12 ? QString::number(qint64(d)) : QString::number(d, 'g', 6);
    }
    if (v.typeId() == QMetaType::Bool) return v.toBool() ? Text::tr("true") : Text::tr("false");
    return v.toString();
}

QList<int> motorPorts(const QString &field)
{
    if (field == QLatin1String("ABC")) return {0, 1, 2};
    const int i = field.isEmpty() ? -1 : field.at(0).toLatin1() - 'A';
    return i >= 0 && i < 3 ? QList<int>{i} : QList<int>{};
}

int sensorPort(const QString &field) { return std::clamp(field.toInt() - 1, 0, 3); }

/** One stack's interpreter. Several run at once, sharing the Context. */
class Interpreter {
public:
    explicit Interpreter(std::shared_ptr<Context> ctx) : m_ctx(std::move(ctx)) {}

    void runChain(QJsonObject b)
    {
        while (!b.isEmpty()) {
            checkStop();
            exec(b);
            b = b.value(QStringLiteral("next")).toObject();
        }
    }

    // ── values ───────────────────────────────────────
    QVariant value(const QJsonObject &b, const char *name)
    {
        const QJsonObject in = inputs(b).value(QLatin1String(name)).toObject();
        if (!in.isEmpty()) return evaluate(in);
        return fields(b).value(QLatin1String(name)).toString();
    }
    double num(const QJsonObject &b, const char *name) { return toNumber(value(b, name)); }
    bool cond(const QJsonObject &b, const char *name)
    {
        const QJsonObject in = inputs(b).value(QLatin1String(name)).toObject();
        return !in.isEmpty() && toBool(evaluate(in)); // an empty condition slot is false
    }
    QString field(const QJsonObject &b, const char *name) { return fields(b).value(QLatin1String(name)).toString(); }

    QVariant evaluate(const QJsonObject &b)
    {
        const QString t = type(b);
        // Sensors
        if (t == u"nxt_touch_pressed") return sensor(sensorPort(field(b, "PORT")), nxt::SensorKind::Touch) == 1;
        if (t == u"nxt_light")
            return double(sensor(sensorPort(field(b, "PORT")),
                                 field(b, "MODE") == u"AMBIENT" ? nxt::SensorKind::Ambient : nxt::SensorKind::Light));
        if (t == u"nxt_sound") return double(sensor(sensorPort(field(b, "PORT")), nxt::SensorKind::Sound));
        if (t == u"nxt_rotation") return double(rotation(motorPorts(field(b, "PORT")).value(0)));
        if (t == u"nxt_timer") {
            std::lock_guard lock(m_ctx->mu);
            return m_ctx->timer.elapsed() / 1000.0;
        }
        if (t == u"nxt_battery") {
            std::lock_guard lock(m_ctx->mu);
            return double(m_ctx->state.batteryMv.value_or(0));
        }
        if (t == u"nxt_touch_count") {
            const int p = sensorPort(field(b, "PORT"));
            sensor(p, nxt::SensorKind::Touch);
            std::lock_guard lock(m_ctx->mu);
            return double(m_ctx->bumps[p]);
        }
        if (t == u"nxt_motor_power" || t == u"nxt_motor_running") {
            const int p = motorPorts(field(b, "PORT")).value(0);
            std::lock_guard lock(m_ctx->mu);
            return t == u"nxt_motor_power" ? QVariant(double(m_ctx->motors[p])) : QVariant(m_ctx->motors[p] != 0);
        }
        if (t == u"nxt_is_light") {
            const double v = sensor(sensorPort(field(b, "PORT")), nxt::SensorKind::Light);
            return field(b, "OP") == u"LT" ? v < num(b, "VALUE") : v > num(b, "VALUE");
        }
        if (t == u"nxt_is_loud") return sensor(sensorPort(field(b, "PORT")), nxt::SensorKind::Sound) > num(b, "VALUE");
        if (t == u"nxt_gyro_rate") {
            const int p = sensorPort(field(b, "PORT"));
            ensureGyro(p);
            const double v = sensor(p, nxt::SensorKind::Gyro);
            std::lock_guard lock(m_ctx->mu);
            return v - m_ctx->gyroOffset[p];
        }
        if (t == u"nxt_gyro_angle") return gyroAngle(sensorPort(field(b, "PORT")));
        if (t == u"nxt_ultrasonic" || t == u"nxt_is_distance") unsupportedSensor();
        if (t == u"nxt_color" || t == u"nxt_color_is") unsupportedColour();
        if (t == u"nxt_button") unsupportedButtons();
        if (t == u"pad_axis") {
            std::lock_guard lock(m_ctx->mu);
            return double(m_ctx->padAxes[std::clamp(field(b, "AXIS").toInt(), 0, int(drive::PadAxis::Count) - 1)]);
        }
        if (t == u"pad_button") return padPressed(field(b, "BUTTON").toInt());
        if (t == u"nxt_mailbox_number") onlyOnBrick();

        // Logic
        if (t == u"logic_compare") return compare(value(b, "A"), field(b, "OP"), value(b, "B"));
        if (t == u"logic_and") return cond(b, "A") && cond(b, "B");
        if (t == u"logic_or") return cond(b, "A") || cond(b, "B");
        if (t == u"logic_not") return !cond(b, "A");
        if (t == u"logic_boolean") return field(b, "BOOL") == u"TRUE";

        // Math and text
        if (t == u"math_arithmetic") {
            const double a = num(b, "A"), c = num(b, "B");
            const QString op = field(b, "OP");
            if (op == u"ADD") return a + c;
            if (op == u"SUB") return a - c;
            if (op == u"MUL") return a * c;
            return c == 0 ? 0.0 : a / c;
        }
        if (t == u"math_random") {
            int lo = int(std::lround(num(b, "FROM"))), hi = int(std::lround(num(b, "TO")));
            if (lo > hi) std::swap(lo, hi);
            return double(QRandomGenerator::global()->bounded(lo, hi + 1));
        }
        if (t == u"math_constrain") return std::clamp(num(b, "VALUE"), std::min(num(b, "LOW"), num(b, "HIGH")), std::max(num(b, "LOW"), num(b, "HIGH")));
        if (t == u"math_single") {
            const double n = num(b, "NUM");
            const QString op = field(b, "OP");
            constexpr double Rad = 3.14159265358979323846 / 180.0;
            if (op == u"ABS") return std::abs(n);
            if (op == u"ROUND") return std::round(n);
            if (op == u"ROOT") return n < 0 ? 0.0 : std::sqrt(n);
            if (op == u"NEG") return -n;
            if (op == u"FLOOR") return std::floor(n);
            if (op == u"CEIL") return std::ceil(n);
            if (op == u"SIN") return std::sin(n * Rad); // degrees, like the NXT
            if (op == u"COS") return std::cos(n * Rad);
            if (op == u"TAN") return std::tan(n * Rad);
            return n;
        }
        if (t == u"math_modulo") {
            const double a = num(b, "A"), m = num(b, "B");
            return m == 0 ? 0.0 : a - m * std::floor(a / m);
        }
        if (t == u"math_minmax") return field(b, "OP") == u"MIN" ? std::min(num(b, "A"), num(b, "B")) : std::max(num(b, "A"), num(b, "B"));
        if (t == u"math_between") {
            const double v = num(b, "VALUE");
            return v >= std::min(num(b, "LOW"), num(b, "HIGH")) && v <= std::max(num(b, "LOW"), num(b, "HIGH"));
        }
        if (t == u"text_join") return toText(value(b, "A")) + toText(value(b, "B"));
        if (t == u"text_length") return double(toText(value(b, "TEXT")).size());

        // Variables
        if (t == u"data_variable") {
            std::lock_guard lock(m_ctx->mu);
            return m_ctx->vars.value(field(b, "VAR"), 0.0);
        }
        throw RunError{Text::tr("The block \"%1\" can't be used as a value.").arg(t)};
    }

    // ── statements ───────────────────────────────────
    void exec(const QJsonObject &b)
    {
        const QString t = type(b);
        // Motors
        if (t == u"nxt_motor_on") return setMotors(motorPorts(field(b, "PORT")), power(num(b, "POWER")));
        if (t == u"nxt_motor_for") {
            const QList<int> ports = motorPorts(field(b, "PORT"));
            const int p = power(num(b, "POWER"));
            QList<int> powers;
            for (int i = 0; i < ports.size(); ++i) powers << p;
            return runFor(ports, powers, num(b, "AMOUNT"), field(b, "UNIT"));
        }
        if (t == u"nxt_motor_stop") return setMotors(motorPorts(field(b, "PORT")), 0);
        if (t == u"nxt_reset_rotation") {
            for (int p : motorPorts(field(b, "PORT"))) onUi([p] { nxt::Brick::instance().resetRotation(p); });
            return sleepMs(250); // let the next telemetry poll see the reset
        }
        if (t == u"nxt_move_steer") {
            const auto [l, r] = steer(num(b, "STEER"), power(num(b, "POWER")));
            setMotor(motorPorts(field(b, "LEFT")).value(0), l);
            return setMotor(motorPorts(field(b, "RIGHT")).value(0), r);
        }
        if (t == u"nxt_motor_start_speed") {
            std::lock_guard lock(m_ctx->mu);
            for (int p : motorPorts(field(b, "PORT"))) m_ctx->speed[p] = power(num(b, "POWER"));
            return;
        }
        if (t == u"nxt_motor_run_direction") {
            const QList<int> ports = motorPorts(field(b, "PORT"));
            const int sign = field(b, "DIR") == u"CCW" ? -1 : 1;
            QList<int> powers;
            {
                std::lock_guard lock(m_ctx->mu);
                for (int p : ports) powers << sign * m_ctx->speed[p];
            }
            return runFor(ports, powers, num(b, "AMOUNT"), field(b, "UNIT"));
        }
        if (t == u"nxt_motor_go_to") {
            const int p = motorPorts(field(b, "PORT")).value(0);
            const double diff = num(b, "DEGREES") - rotation(p);
            // Within 2 degrees counts as there: correcting smaller errors makes a motor twitch inside loops.
            if (std::abs(diff) <= 2) return;
            int sp;
            {
                std::lock_guard lock(m_ctx->mu);
                sp = std::abs(m_ctx->speed[p]);
            }
            return runFor({p}, {diff < 0 ? -sp : sp}, std::abs(diff), QStringLiteral("DEGREES"));
        }

        // Movement
        if (t == u"nxt_move_set_motors") {
            std::lock_guard lock(m_ctx->mu);
            m_ctx->moveLeft = motorPorts(field(b, "LEFT")).value(0);
            m_ctx->moveRight = motorPorts(field(b, "RIGHT")).value(0);
            return;
        }
        if (t == u"nxt_move_set_speed") {
            std::lock_guard lock(m_ctx->mu);
            m_ctx->moveSpeed = power(num(b, "POWER"));
            return;
        }
        if (t == u"nxt_move_set_stop") return; // the brake setting in Settings → Drive applies
        if (t == u"nxt_move_stop") return setMotors(drivePorts(), 0);
        if (t == u"nxt_move_for" || t == u"nxt_move_start") {
            const auto [l, r] = direction(field(b, "DIR"));
            if (t == u"nxt_move_start") return drive(l, r);
            return runFor(drivePorts(), {l, r}, num(b, "AMOUNT"), field(b, "UNIT"));
        }
        if (t == u"nxt_move_steer_for" || t == u"nxt_move_steer_start") {
            const auto [l, r] = steer(num(b, "STEER"), moveSpeed());
            if (t == u"nxt_move_steer_start") return drive(l, r);
            return runFor(drivePorts(), {l, r}, num(b, "AMOUNT"), field(b, "UNIT"));
        }
        if (t == u"nxt_move_tank_for" || t == u"nxt_move_tank_start") {
            const int l = power(num(b, "LEFT")), r = power(num(b, "RIGHT"));
            if (t == u"nxt_move_tank_start") return drive(l, r);
            return runFor(drivePorts(), {l, r}, num(b, "AMOUNT"), field(b, "UNIT"));
        }

        // Pad and Bluetooth
        if (t == u"pad_drive") {
            int x, y;
            {
                std::lock_guard lock(m_ctx->mu);
                x = m_ctx->padAxes[int(drive::PadAxis::LeftX)];
                y = m_ctx->padAxes[int(drive::PadAxis::LeftY)];
            }
            setMotor(motorPorts(field(b, "LEFT")).value(0), power(y + x));
            return setMotor(motorPorts(field(b, "RIGHT")).value(0), power(y - x));
        }
        if (t == u"nxt_mailbox_send") onlyOnBrick();

        // Sensors
        if (t == u"nxt_reset_timer") {
            std::lock_guard lock(m_ctx->mu);
            m_ctx->timer.restart();
            return;
        }
        if (t == u"nxt_gyro_reset") {
            const int p = sensorPort(field(b, "PORT"));
            ensureGyro(p);
            std::lock_guard lock(m_ctx->mu);
            m_ctx->gyroAngle[p] = 0;
            return;
        }
        if (t == u"nxt_gyro_calibrate") return calibrateGyro(sensorPort(field(b, "PORT")));

        // Control
        if (t == u"nxt_wait_seconds") return sleepMs(int(num(b, "SECONDS") * 1000));
        if (t == u"nxt_wait_touch") {
            const int p = sensorPort(field(b, "PORT"));
            const QString state = field(b, "STATE");
            if (state == u"RELEASED") return waitUntil([&] { return sensor(p, nxt::SensorKind::Touch) == 0; });
            waitUntil([&] { return sensor(p, nxt::SensorKind::Touch) == 1; });
            if (state == u"BUMPED") waitUntil([&] { return sensor(p, nxt::SensorKind::Touch) == 0; });
            return;
        }
        if (t == u"nxt_wait_until") return waitUntil([&] { return cond(b, "CONDITION"); });
        if (t == u"nxt_wait_button") unsupportedButtons();
        if (t == u"nxt_wait_distance") unsupportedSensor();
        if (t == u"nxt_wait_color") unsupportedColour();
        if (t == u"nxt_wait_gyro") {
            const int p = sensorPort(field(b, "PORT"));
            const double target = num(b, "VALUE");
            const bool less = field(b, "OP") == u"LT";
            return waitUntil([&] { return less ? gyroAngle(p) < target : gyroAngle(p) > target; });
        }
        if (t == u"nxt_repeat") {
            const int times = int(std::lround(num(b, "TIMES")));
            return loop([&, i = 0]() mutable { return i++ < times; }, statement(b, "DO"));
        }
        if (t == u"nxt_forever") return loop([] { return true; }, statement(b, "DO"));
        if (t == u"nxt_repeat_until") return loop([&] { return !cond(b, "CONDITION"); }, statement(b, "DO"));
        if (t == u"nxt_while") return loop([&] { return cond(b, "CONDITION"); }, statement(b, "DO"));
        if (t == u"nxt_if") {
            if (cond(b, "CONDITION")) runChain(statement(b, "DO"));
            return;
        }
        if (t == u"nxt_if_else") return runChain(statement(b, cond(b, "CONDITION") ? "DO" : "ELSE"));
        if (t == u"nxt_break") throw BreakLoop{};
        if (t == u"nxt_stop_program") {
            m_ctx->stop = true;
            throw Stopped{};
        }
        if (t == u"nxt_stop_other") return;
        if (t == u"nxt_run_parallel") {
            std::exception_ptr failure;
            std::thread other([this, ctx = m_ctx, a = statement(b, "A"), &failure] {
                try {
                    Interpreter(ctx).runChain(a);
                } catch (...) {
                    failure = std::current_exception();
                }
            });
            try {
                runChain(statement(b, "B"));
            } catch (...) {
                m_ctx->stop = true; // unblock the other branch, then rethrow
                other.join();
                throw;
            }
            other.join();
            if (failure) std::rethrow_exception(failure);
            return;
        }

        // Events
        if (t == u"nxt_broadcast" || t == u"nxt_broadcast_wait") {
            const QString msg = field(b, "MESSAGE");
            {
                std::lock_guard lock(m_ctx->mu);
                ++m_ctx->broadcasts[msg];
            }
            if (t == u"nxt_broadcast_wait") {
                sleepMs(60); // give receivers time to start
                waitUntil([&] {
                    std::lock_guard lock(m_ctx->mu);
                    return m_ctx->busyReceivers.value(msg) == 0;
                });
            }
            return;
        }

        // Sound
        if (t == u"nxt_play_tone") return tone(int(num(b, "FREQ")), int(num(b, "DURATION")));
        if (t == u"nxt_play_note") return tone(field(b, "NOTE").toInt(), int(num(b, "BEATS") * BeatMs));
        if (t == u"nxt_play_note_octave") {
            const int semis = field(b, "NOTE").toInt() - 9 + (field(b, "OCTAVE").toInt() - 4) * 12; // from A4
            return tone(int(std::lround(440.0 * std::pow(2.0, semis / 12.0))), int(num(b, "SECONDS") * 1000));
        }
        if (t == u"nxt_beep") return tone(880, 150);
        if (t == u"nxt_play_file") {
            const QByteArray packet = nxt::playSoundFile(field(b, "FILE"));
            onUi([packet] { nxt::Brick::instance().send(packet); });
            if (field(b, "WAIT") == u"WAIT") sleepMs(1000);
            return;
        }
        if (t == u"nxt_stop_sound") {
            onUi([] { nxt::Brick::instance().send(nxt::stopSoundPlayback()); });
            return;
        }
        if (t == u"nxt_set_volume" || t == u"nxt_change_volume") return; // not available over direct commands

        // Display: shown in the app, since direct commands can't draw on the NXT screen
        if (t == u"nxt_display_text") return screen(toText(value(b, "TEXT")));
        if (t == u"nxt_display_at") return screen(toText(value(b, "TEXT")));
        if (t == u"nxt_display_number") return screen(toText(value(b, "VALUE")));
        if (t.startsWith(u"nxt_draw_") || t == u"nxt_display_face" || t == u"nxt_clear_screen") return;

        // Variables
        if (t == u"data_set" || t == u"data_change") {
            const QVariant v = value(b, "VALUE");
            std::lock_guard lock(m_ctx->mu);
            QVariant &var = m_ctx->vars[field(b, "VAR")];
            var = t == u"data_set" ? v : QVariant(toNumber(var) + toNumber(v));
            return;
        }
        if (t == u"data_show") {
            QVariant v;
            {
                std::lock_guard lock(m_ctx->mu);
                v = m_ctx->vars.value(field(b, "VAR"), 0.0);
            }
            return screen(QStringLiteral("%1: %2").arg(field(b, "VAR"), toText(v)));
        }
        // Value blocks left loose in a stack do nothing.
    }

private:
    void checkStop() const
    {
        if (m_ctx->stop) throw Stopped{};
    }

    void sleepMs(int ms)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms) {
            checkStop();
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min<qint64>(PollMs, ms - t.elapsed() + 1)));
        }
    }

    void waitUntil(const std::function<bool()> &done)
    {
        while (!done()) sleepMs(PollMs);
    }

    void loop(const std::function<bool()> &again, const QJsonObject &body)
    {
        try {
            while (again()) {
                runChain(body);
                sleepMs(1); // keeps empty loops from hogging the CPU and lets Stop through
            }
        } catch (const BreakLoop &) {
        }
    }

    [[noreturn]] void unsupportedSensor() const
    {
        throw RunError{Text::tr("The ultrasonic sensor only works in programs uploaded to the NXT. Use Upload to NXT instead of Run.")};
    }
    [[noreturn]] void unsupportedColour() const { throw RunError{Text::tr("The colour sensor only works in programs uploaded to the NXT. Use Upload to NXT instead of Run.")}; }
    [[noreturn]] void unsupportedButtons() const
    {
        throw RunError{Text::tr("The NXT's own buttons only work in programs uploaded to the NXT. Use Upload to NXT instead of Run.")};
    }

    [[noreturn]] void onlyOnBrick() const
    {
        throw RunError{Text::tr("Mailbox blocks only work when the program runs on the NXT. Use Upload to NXT instead of Run.")};
    }

public:
    bool padPressed(int button)
    {
        std::lock_guard lock(m_ctx->mu);
        return button >= 0 && button < int(drive::PadButton::Count) && m_ctx->padButtons[button];
    }

private:
    static int power(double p) { return std::clamp(int(std::lround(p)), -100, 100); }

    QVariant compare(const QVariant &a, const QString &op, const QVariant &b) const
    {
        const double x = toNumber(a), y = toNumber(b);
        if (op == u"LT") return x < y;
        if (op == u"GT") return x > y;
        if (op == u"LTE") return x <= y;
        if (op == u"GTE") return x >= y;
        bool okA = false, okB = false;
        a.toString().toDouble(&okA);
        b.toString().toDouble(&okB);
        const bool equal = okA && okB ? x == y : toText(a).compare(toText(b), Qt::CaseInsensitive) == 0;
        return op == u"NEQ" ? !equal : equal;
    }

    /** Sets the sensor type if needed and returns its value (touch 0/1, light and sound 0-100). */
    int sensor(int port, nxt::SensorKind kind)
    {
        bool needsMode;
        {
            std::lock_guard lock(m_ctx->mu);
            needsMode = m_ctx->state.sensors[port] != kind;
        }
        if (needsMode) onUi([port, kind] { nxt::Brick::instance().setSensor(port, kind); });
        QElapsedTimer t;
        t.start();
        for (;;) {
            {
                std::lock_guard lock(m_ctx->mu);
                if (m_ctx->state.status != nxt::LinkStatus::Connected) throw RunError{Text::tr("The NXT was disconnected.")};
                if (m_ctx->state.sensors[port] == kind && m_ctx->state.sensorValues[port]) return *m_ctx->state.sensorValues[port];
            }
            if (t.elapsed() > 3000) throw RunError{Text::tr("No reading from the sensor on port %1. Is it plugged in?").arg(port + 1)};
            sleepMs(PollMs);
        }
    }

    /** Averages a second of gyro readings as its resting value and starts the angle at 0. The robot must be still. */
    void calibrateGyro(int port)
    {
        {
            std::lock_guard lock(m_ctx->mu);
            m_ctx->gyroReady[port] = false;
        }
        sensor(port, nxt::SensorKind::Gyro);
        sleepMs(150); // the first readings after switching the port's mode can be off
        double sum = 0;
        int count = 0;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 1000) {
            sum += sensor(port, nxt::SensorKind::Gyro);
            ++count;
            sleepMs(20);
        }
        std::lock_guard lock(m_ctx->mu);
        m_ctx->gyroOffset[port] = sum / std::max(1, count);
        m_ctx->gyroAngle[port] = 0;
        m_ctx->gyroLast[port] = m_ctx->clock.elapsed();
        m_ctx->gyroReady[port] = true;
    }

    /** The first gyro block on a port calibrates it, like the start of a program on the NXT does. */
    void ensureGyro(int port)
    {
        bool ready;
        {
            std::lock_guard lock(m_ctx->mu);
            ready = m_ctx->gyroReady[port];
        }
        if (!ready) calibrateGyro(port);
    }

    double gyroAngle(int port)
    {
        ensureGyro(port);
        std::lock_guard lock(m_ctx->mu);
        return m_ctx->gyroAngle[port];
    }

    int rotation(int port)
    {
        std::lock_guard lock(m_ctx->mu);
        return m_ctx->state.motorRotation[port];
    }

    void setMotor(int port, int p) { setMotors({port}, p); }

    void setMotors(const QList<int> &ports, int p)
    {
        std::array<int, 3> m;
        {
            std::lock_guard lock(m_ctx->mu);
            for (int port : ports) m_ctx->motors[port] = p;
            m = m_ctx->motors;
        }
        onUi([m] { nxt::Brick::instance().setMotors(m); });
    }

    QList<int> drivePorts()
    {
        std::lock_guard lock(m_ctx->mu);
        return {m_ctx->moveLeft, m_ctx->moveRight};
    }

    int moveSpeed()
    {
        std::lock_guard lock(m_ctx->mu);
        return m_ctx->moveSpeed;
    }

    void drive(int left, int right)
    {
        const QList<int> ports = drivePorts();
        setMotor(ports[0], left);
        setMotor(ports[1], right);
    }

    std::pair<int, int> direction(const QString &dir)
    {
        const int s = moveSpeed();
        if (dir == u"BACKWARD") return {-s, -s};
        if (dir == u"LEFT") return {-s, s};
        if (dir == u"RIGHT") return {s, -s};
        return {s, s};
    }

    /** Steering -100..100: 0 straight, ±50 one wheel stopped, ±100 spin on the spot. */
    static std::pair<int, int> steer(double steering, int p)
    {
        const double s = std::clamp(steering, -100.0, 100.0);
        const int inner = int(std::lround(p * (1.0 - std::abs(s) / 50.0)));
        return s >= 0 ? std::pair{p, inner} : std::pair{inner, p};
    }

    /** Runs motors at the given powers for an amount of rotations, degrees or seconds, then stops them. */
    void runFor(const QList<int> &ports, QList<int> powers, double amount, const QString &unit)
    {
        if (ports.isEmpty()) return;
        if (amount < 0) {
            amount = -amount;
            for (int &p : powers) p = -p;
        }
        if (unit == u"SECONDS") {
            for (int i = 0; i < ports.size(); ++i) setMotor(ports[i], powers.value(i));
            sleepMs(int(amount * 1000));
        } else {
            const double degrees = unit == u"ROTATIONS" ? amount * 360.0 : amount;
            // Measure on the fastest motor; the others get proportionally fewer degrees so they finish together.
            int ref = 0;
            for (int i = 1; i < ports.size(); ++i)
                if (std::abs(powers.value(i)) > std::abs(powers.value(ref))) ref = i;
            if (powers.value(ref) == 0 || degrees < 1) return;
            const int start = rotation(ports[ref]);
            for (int i = 0; i < ports.size(); ++i) {
                const int p = powers.value(i);
                {
                    std::lock_guard lock(m_ctx->mu);
                    m_ctx->motors[ports[i]] = p;
                }
                if (p == 0) continue;
                // The USB thread brakes each motor at its count, far more precisely than polling from here.
                const int limit = int(std::lround(degrees * std::abs(p) / std::abs(powers.value(ref))));
                onUi([port = ports[i], p, limit] { nxt::Brick::instance().runMotorFor(port, p, limit); });
            }
            QElapsedTimer stall;
            stall.start();
            int last = start;
            bool moved = false;
            waitUntil([&] {
                const int now = rotation(ports[ref]);
                if (now != last) {
                    last = now;
                    moved = true;
                    stall.restart();
                }
                // Done when the count is there, or the motor stopped turning (reached its limit, or is blocked).
                return std::abs(now - start) >= degrees - 2 || stall.elapsed() > (moved ? 800 : 3000);
            });
        }
        setMotors(ports, 0);
    }

    void tone(int hz, int ms)
    {
        ms = std::max(0, ms);
        onUi([hz, ms] { nxt::Brick::instance().playTone(hz, ms); });
        sleepMs(ms);
    }

    void screen(const QString &text)
    {
        QPointer<Runner> owner = m_ctx->owner;
        QMetaObject::invokeMethod(&nxt::Brick::instance(), [owner, text] {
            if (owner) emit owner->screenText(text);
        }, Qt::QueuedConnection);
    }

    std::shared_ptr<Context> m_ctx;
};

/** Runs `body`, turning exceptions into the context's error state. Returns false when the program should end. */
bool guarded(const std::shared_ptr<Context> &ctx, const std::function<void()> &body)
{
    try {
        body();
        return true;
    } catch (const Stopped &) {
    } catch (const BreakLoop &) {
        return true; // "exit loop" outside a loop just ends the stack
    } catch (const RunError &e) {
        std::lock_guard lock(ctx->mu);
        if (ctx->error.isEmpty()) ctx->error = e.message;
    } catch (const std::exception &e) {
        std::lock_guard lock(ctx->mu);
        if (ctx->error.isEmpty()) ctx->error = QString::fromLocal8Bit(e.what());
    }
    ctx->stop = true;
    return false;
}

/** Hat blocks that wait for something: returns a condition to watch, or nullptr if `hat` isn't one. */
std::function<bool(Interpreter &)> trigger(const QJsonObject &hat)
{
    const QString t = type(hat);
    const QJsonObject f = fields(hat);
    if (t == u"nxt_when_touch") {
        const int p = sensorPort(f.value(QStringLiteral("PORT")).toString());
        const int want = f.value(QStringLiteral("STATE")).toString() == u"RELEASED" ? 0 : 1;
        return [p, want, hat](Interpreter &in) {
            QJsonObject probe{{QStringLiteral("type"), QStringLiteral("nxt_touch_pressed")},
                              {QStringLiteral("fields"), QJsonObject{{QStringLiteral("PORT"), QString::number(p + 1)}}}};
            return toBool(in.evaluate(probe)) == bool(want);
        };
    }
    const auto compareProbe = [&](const char *probeType, const char *kindField, const char *kind) {
        QJsonObject probeFields{{QStringLiteral("PORT"), f.value(QStringLiteral("PORT"))},
                                {QStringLiteral("OP"), f.value(QStringLiteral("OP")).toString().isEmpty() ? QStringLiteral("GT") : f.value(QStringLiteral("OP"))},
                                {QStringLiteral("VALUE"), f.value(QStringLiteral("VALUE"))}};
        if (kindField) probeFields.insert(QLatin1String(kindField), QLatin1String(kind));
        const QJsonObject probe{{QStringLiteral("type"), QLatin1String(probeType)}, {QStringLiteral("fields"), probeFields}};
        return [probe](Interpreter &in) { return toBool(in.evaluate(probe)); };
    };
    if (t == u"nxt_when_light") return compareProbe("nxt_is_light", nullptr, nullptr);
    if (t == u"nxt_when_loud") return compareProbe("nxt_is_loud", nullptr, nullptr);
    if (t == u"nxt_when_distance") return compareProbe("nxt_is_distance", nullptr, nullptr);
    if (t == u"nxt_when_button") return [](Interpreter &in) {
        return toBool(in.evaluate(QJsonObject{{QStringLiteral("type"), QStringLiteral("nxt_button")}}));
    };
    if (t == u"pad_when_button") {
        const int button = f.value(QStringLiteral("BUTTON")).toString().toInt();
        return [button](Interpreter &in) { return in.padPressed(button); };
    }
    if (t == u"nxt_when_timer") {
        const double s = toNumber(f.value(QStringLiteral("SECONDS")).toString());
        return [s](Interpreter &in) { return toNumber(in.evaluate(QJsonObject{{QStringLiteral("type"), QStringLiteral("nxt_timer")}})) > s; };
    }
    return nullptr;
}

} // namespace

Runner::Runner(QObject *parent) : QObject(parent)
{
    connect(&nxt::Brick::instance(), &nxt::Brick::stateChanged, this, &Runner::onBrick);
    connect(&drive::Gamepad::instance(), &drive::Gamepad::updated, this, &Runner::onPad);
}

void Runner::onPad(const drive::PadSnapshot &snap)
{
    if (!m_ctx) return;
    std::lock_guard lock(m_ctx->mu);
    m_ctx->padAxes = snap.connected ? drive::padAxes(snap.input) : drive::PadAxes{};
    m_ctx->padButtons = snap.connected ? snap.buttons : decltype(snap.buttons){};
}

Runner::~Runner() { stop(); }

QString Runner::start(const QJsonObject &workspace)
{
    if (m_ctx) return {};
    const nxt::BrickState state = nxt::Brick::instance().state();
    if (state.status != nxt::LinkStatus::Connected) return Text::tr("Connect the NXT first (Brick tab).");

    QList<QJsonObject> startStacks, eventStacks;
    for (const QJsonValue &v : workspace.value(QStringLiteral("stacks")).toArray()) {
        const QJsonObject top = v.toObject().value(QStringLiteral("block")).toObject();
        const QString t = type(top);
        if (t == u"nxt_when_start") startStacks << top;
        else if (t.startsWith(u"nxt_when_") || t == u"pad_when_button") eventStacks << top;
    }
    if (startStacks.isEmpty() && eventStacks.isEmpty())
        return Text::tr("Nothing to run: start a stack with \"when program starts\".");

    auto ctx = std::make_shared<Context>();
    ctx->owner = this;
    ctx->state = state;
    ctx->timer.start();
    ctx->clock.start();
    ctx->hasEventStacks = !eventStacks.isEmpty();
    for (const QJsonValue &v : workspace.value(QStringLiteral("variables")).toArray()) ctx->vars.insert(v.toString(), 0.0);
    ctx->startStacks = int(startStacks.size());
    const drive::PadSnapshot pad = drive::Gamepad::instance().snapshot();
    if (pad.connected) {
        ctx->padAxes = drive::padAxes(pad.input);
        ctx->padButtons = pad.buttons;
    }
    m_ctx = ctx;
    emit runningChanged(true);

    QPointer<Runner> self = this;
    // Any thread that sees the program stopped asks the UI thread to wrap up (finish() runs once).
    const auto endIfStopped = [ctx, self] {
        if (!ctx->stop) return;
        QMetaObject::invokeMethod(&nxt::Brick::instance(), [self, ctx] {
            if (self && self->m_ctx == ctx) self->finish();
        }, Qt::QueuedConnection);
    };
    const auto stackDone = [ctx, endIfStopped] {
        // With only start stacks, the program ends when the last one does.
        if (--ctx->startStacks == 0 && !ctx->hasEventStacks) ctx->stop = true;
        endIfStopped();
    };
    const auto spawn = [ctx](std::function<void()> f) {
        std::lock_guard lock(ctx->threadsMu);
        ctx->threads.emplace_back(std::move(f));
    };

    for (const QJsonObject &top : startStacks)
        spawn([ctx, top, stackDone] {
            guarded(ctx, [&] { Interpreter(ctx).runChain(top.value(QStringLiteral("next")).toObject()); });
            stackDone();
        });

    for (const QJsonObject &top : eventStacks) {
        const QJsonObject body = top.value(QStringLiteral("next")).toObject();
        if (type(top) == u"nxt_when_message") {
            const QString msg = fields(top).value(QStringLiteral("MESSAGE")).toString();
            spawn([ctx, msg, body, endIfStopped] {
                Interpreter in(ctx);
                int seen = 0;
                guarded(ctx, [&] {
                    while (!ctx->stop) {
                        int count;
                        {
                            std::lock_guard lock(ctx->mu);
                            count = ctx->broadcasts.value(msg);
                            if (count != seen) ++ctx->busyReceivers[msg];
                        }
                        if (count != seen) {
                            seen = count;
                            try {
                                in.runChain(body);
                            } catch (...) {
                                std::lock_guard lock(ctx->mu);
                                --ctx->busyReceivers[msg];
                                throw;
                            }
                            std::lock_guard lock(ctx->mu);
                            --ctx->busyReceivers[msg];
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(PollMs));
                    }
                });
                endIfStopped();
            });
            continue;
        }
        auto watch = trigger(top);
        if (!watch) continue;
        spawn([ctx, watch, body, endIfStopped] {
            Interpreter in(ctx);
            guarded(ctx, [&] {
                bool before = true; // only a change from false to true starts the stack
                while (!ctx->stop) {
                    const bool now = watch(in);
                    if (now && !before) in.runChain(body);
                    before = now;
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            });
            endIfStopped();
        });
    }
    return {};
}

void Runner::stop()
{
    if (!m_ctx) return;
    m_ctx->stop = true;
    finish();
}

void Runner::finish()
{
    std::shared_ptr<Context> ctx = std::move(m_ctx);
    if (!ctx) return;
    ctx->stop = true;
    std::vector<std::thread> threads;
    {
        std::lock_guard lock(ctx->threadsMu);
        threads.swap(ctx->threads);
    }
    for (std::thread &t : threads)
        if (t.joinable()) t.join();
    auto &brick = nxt::Brick::instance();
    brick.stopAll();
    brick.send(nxt::stopSoundPlayback());
    QString error;
    {
        std::lock_guard lock(ctx->mu);
        error = ctx->error;
    }
    emit runningChanged(false);
    emit finished(error);
}

void Runner::onBrick(const nxt::BrickState &s)
{
    if (!m_ctx) return;
    if (s.status != nxt::LinkStatus::Connected) {
        {
            std::lock_guard lock(m_ctx->mu);
            if (m_ctx->error.isEmpty()) m_ctx->error = Text::tr("The NXT was disconnected.");
        }
        stop();
        return;
    }
    std::lock_guard lock(m_ctx->mu);
    const qint64 now = m_ctx->clock.elapsed();
    for (int p = 0; p < 4; ++p) {
        // Add up the gyro's turn speed into an angle. Readings within 1 of rest are noise, left out so the angle stays put.
        if (s.sensors[p] != nxt::SensorKind::Gyro || !s.sensorValues[p] || !m_ctx->gyroReady[p]) continue;
        const double rate = *s.sensorValues[p] - m_ctx->gyroOffset[p];
        if (std::abs(rate) > 1) m_ctx->gyroAngle[p] += rate * (now - m_ctx->gyroLast[p]) / 1000.0;
        m_ctx->gyroLast[p] = now;
    }
    for (int p = 0; p < 4; ++p) {
        // Count presses for "bump count": a press followed by a release.
        if (s.sensors[p] != nxt::SensorKind::Touch || !s.sensorValues[p]) continue;
        const bool down = *s.sensorValues[p] == 1;
        if (m_ctx->touchDown[p] && !down) ++m_ctx->bumps[p];
        m_ctx->touchDown[p] = down;
    }
    m_ctx->state = s;
}

} // namespace run
