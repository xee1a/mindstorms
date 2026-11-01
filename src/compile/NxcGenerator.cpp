#include "NxcGenerator.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <cmath>

namespace compile {

namespace {

/** Translation context for this file's messages (lupdate reads Text::tr calls). */
struct Text {
    Q_DECLARE_TR_FUNCTIONS(NxcGenerator)
};

/** A problem in the user's blocks; unwinds the generator. */
struct GenError {
    QString message;
};

enum class Type { Num, Bool, Str };

struct Expr {
    QString code;
    Type type = Type::Num;
};

enum class SensorUse { Touch, Light, Ambient, Sound, Ultrasonic, Color };

QString sensorName(SensorUse u)
{
    switch (u) {
    case SensorUse::Touch: return Text::tr("touch");
    case SensorUse::Light:
    case SensorUse::Ambient: return Text::tr("light");
    case SensorUse::Sound: return Text::tr("sound");
    case SensorUse::Ultrasonic: return Text::tr("ultrasonic");
    case SensorUse::Color: return Text::tr("colour");
    }
    return {};
}

QString typeOf(const QJsonObject &b) { return b.value(QStringLiteral("type")).toString(); }

/** Helpers every program gets. All `safecall`, since several tasks may call them at once. */
constexpr char Prelude[] = R"NXC(
int ns_speed[3];
int ns_mL = 1;
int ns_mR = 2;
int ns_mSpeed = 50;
bool ns_brake = true;
int ns_volPct = 75;
int ns_vol = 3;
long ns_t0;
int ns_halt = -1;
mutex ns_mu;

safecall int ns_pow(float p) { int v = p; if (p - v >= 0.5) v++; if (v - p >= 0.5) v--; if (v > 100) v = 100; if (v < -100) v = -100; return v; }
safecall long ns_ms(float s) { if (s <= 0) return 0; long v = s * 1000; return v; }
safecall long ns_floor(float x) { long i = x; if (i > x) i--; return i; }
safecall long ns_ceil(float x) { long i = x; if (i < x) i++; return i; }
safecall long ns_round(float x) { return ns_floor(x + 0.5); }
safecall float ns_abs(float x) { if (x < 0) return -x; return x; }
safecall float ns_div(float a, float b) { if (b == 0) return 0; return a / b; }
safecall float ns_mod(float a, float m) { if (m == 0) return 0; return a - m * ns_floor(a / m); }
safecall float ns_min(float a, float b) { if (a < b) return a; return b; }
safecall float ns_max(float a, float b) { if (a > b) return a; return b; }
safecall float ns_limit(float v, float a, float b) { float lo = ns_min(a, b); float hi = ns_max(a, b); if (v < lo) return lo; if (v > hi) return hi; return v; }
safecall bool ns_between(float v, float a, float b) { return v >= ns_min(a, b) && v <= ns_max(a, b); }
safecall float ns_rand(float a, float b) { long lo = ns_round(ns_min(a, b)); long hi = ns_round(ns_max(a, b)); return lo + Random(hi - lo + 1); }
safecall float ns_sqrt(float x) { if (x <= 0) return 0; return sqrt(x); }
safecall float ns_sin(float d) { int i = ns_round(d); return Sin(i) / 100.0; }
safecall float ns_cos(float d) { int i = ns_round(d); return Cos(i) / 100.0; }
safecall float ns_tan(float d) { int i = ns_round(d); int c = Cos(i); if (c == 0) return 0; return Sin(i) * 1.0 / c; }
safecall float ns_num(string s) { return StrToNum(s); }

safecall string ns_str(float v)
{
    long i = v;
    float f = v - i;
    if (f < 0) f = -f;
    long h = f * 100 + 0.5;
    if (h >= 100) { if (v >= 0) i++; else i--; h = 0; }
    string s = NumToStr(i);
    if (v < 0 && i == 0 && h > 0) s = "-0";
    if (h == 0) return s;
    if (h % 10 == 0) return StrCat(s, ".", NumToStr(h / 10));
    if (h < 10) return StrCat(s, ".0", NumToStr(h));
    return StrCat(s, ".", NumToStr(h));
}

safecall void ns_on(byte port, float p)
{
    int v = ns_pow(p);
    if (v == 0) { if (ns_brake) Off(port); else Coast(port); }
    else OnFwd(port, v);
}
safecall void ns_off(byte port) { if (ns_brake) Off(port); else Coast(port); }

// The movement pair is chosen at run time; the sync commands need a constant pair.
safecall int ns_pairSign() { if (ns_mL < ns_mR) return 1; return -1; }
safecall void ns_syncOn(float p, float turn)
{
    int v = ns_pow(p);
    int t = ns_pow(turn) * ns_pairSign();
    if (ns_mL == ns_mR) { ns_on(ns_mL, v); return; }
    if (v == 0) { ns_off(ns_mL); ns_off(ns_mR); return; }
    if (ns_mL + ns_mR == 1) OnFwdSync(OUT_AB, v, t);
    else if (ns_mL + ns_mR == 2) OnFwdSync(OUT_AC, v, t);
    else OnFwdSync(OUT_BC, v, t);
}
safecall void ns_syncDeg(float p, float deg, float turn)
{
    int v = ns_pow(p);
    int t = ns_pow(turn) * ns_pairSign();
    long d = ns_round(deg);
    if (v == 0 || d == 0) return;
    if (ns_mL == ns_mR) RotateMotor(ns_mL, v, d);
    else if (ns_mL + ns_mR == 1) RotateMotorEx(OUT_AB, v, d, t, true, true);
    else if (ns_mL + ns_mR == 2) RotateMotorEx(OUT_AC, v, d, t, true, true);
    else RotateMotorEx(OUT_BC, v, d, t, true, true);
    if (!ns_brake) { Coast(ns_mL); Coast(ns_mR); }
}
safecall void ns_moveOff() { ns_off(ns_mL); ns_off(ns_mR); }

safecall void ns_clearLine(int y) { TextOut(0, y, "                "); }
safecall void ns_line(int a, int b, int c, int d) { LineOut(a, b, c, d); }
safecall void ns_circle(int x, int y, int r) { CircleOut(x, y, r); }
safecall void ns_rect(int x, int y, int w, int h) { RectOut(x, y, w, h); }
safecall void ns_dot(int x, int y) { PointOut(x, y); }
safecall void ns_text(int x, int y, string s) { TextOut(x, y, s); }
safecall void ns_face(int kind)
{
    ClearScreen();
    CircleOut(50, 32, 28);
    if (kind == 3) { LineOut(36, 42, 44, 42); LineOut(56, 42, 64, 42); }
    else if (kind == 4) { LineOut(34, 46, 44, 40); LineOut(66, 46, 56, 40); CircleOut(40, 38, 2); CircleOut(60, 38, 2); }
    else { CircleOut(40, 40, 3); CircleOut(60, 40, 3); }
    if (kind == 0) { LineOut(36, 24, 42, 19); LineOut(42, 19, 58, 19); LineOut(58, 19, 64, 24); }
    else if (kind == 1 || kind == 4) { LineOut(36, 18, 42, 23); LineOut(42, 23, 58, 23); LineOut(58, 23, 64, 18); }
    else if (kind == 2) CircleOut(50, 21, 5);
    else LineOut(42, 21, 58, 21);
}
safecall void ns_tone(float hz, float s) { long ms = ns_ms(s); if (ms > 0) PlayToneEx(ns_round(hz), ms, ns_vol, false); Wait(ms); }
safecall void ns_setVolume(float pct)
{
    ns_volPct = ns_limit(pct, 0, 100);
    ns_vol = (ns_volPct + 12) / 25;
}
)NXC";

constexpr char PadPrelude[] = R"NXC(
int ns_padAx[6];
int ns_padBtn = 0;
long ns_padSeen = 0;

// The app streams the pad into inbox 10 while this program runs (see drive::encodePadMessage).
task ns_padReader()
{
    string m;
    while (true) {
        if (ReceiveMessage(9, true, m) == NO_ERR) {
            if (StrLen(m) >= 11 && StrIndex(m, 0) == 80) {
                for (int i = 0; i < 6; i++) ns_padAx[i] = StrIndex(m, i + 1) - 101;
                ns_padBtn = (StrIndex(m, 7) & 15) | ((StrIndex(m, 8) & 15) << 4) | ((StrIndex(m, 9) & 15) << 8) | ((StrIndex(m, 10) & 15) << 12);
                ns_padSeen = CurrentTick();
            }
        } else if (CurrentTick() - ns_padSeen > 1000) {
            // No pad for a second (cable pulled, app closed): let go of everything so the robot stops.
            for (int j = 0; j < 6; j++) ns_padAx[j] = 0;
            ns_padBtn = 0;
        }
        Wait(10);
    }
}
safecall bool ns_padPressed(int b) { return ((ns_padBtn >> b) & 1) == 1; }
)NXC";

class Generator {
public:
    NxcResult run(const QJsonObject &workspace, const QString &programName)
    {
        try {
            build(workspace, programName);
        } catch (const GenError &e) {
            return {{}, e.message};
        }
        return {m_out, {}};
    }

private:
    // ── workspace ────────────────────────────────────
    void build(const QJsonObject &ws, const QString &programName)
    {
        int index = 0;
        for (const QJsonValue &v : ws.value(QStringLiteral("variables")).toArray()) m_vars.insert(v.toString(), QStringLiteral("v%1").arg(index++));

        QList<QJsonObject> tops;
        for (const QJsonValue &v : ws.value(QStringLiteral("stacks")).toArray()) {
            const QJsonObject top = v.toObject().value(QStringLiteral("block")).toObject();
            const QString t = typeOf(top);
            if (t == u"nxt_when_start" || t.startsWith(u"nxt_when_") || t == u"pad_when_button") tops << top;
        }
        if (tops.isEmpty()) throw GenError{Text::tr("Nothing to upload: start a stack with \"when program starts\".")};
        for (const QJsonObject &top : tops)
            if (typeOf(top) != u"nxt_when_start") m_hasEvents = true;

        // Scan once for features that need globals or tasks, so stack bodies can use them.
        for (const QJsonObject &top : tops) scan(top);

        QStringList tasks;
        int startCount = 0;
        for (const QJsonObject &top : tops) {
            const int id = m_nextStack++;
            const QString body = QStringLiteral("ns_body%1").arg(id);
            m_stackId = id;
            m_loopDepth = 0;
            const QString code = chain(top.value(QStringLiteral("next")).toObject(), 1);
            m_functions << QStringLiteral("void %1()\n{\n%2}\n").arg(body, code);
            const QString t = typeOf(top);
            if (t == u"nxt_when_start") {
                ++startCount;
                QString end = QStringLiteral("    %1();\n").arg(body);
                if (!m_hasEvents)
                    end += QStringLiteral("    Acquire(ns_mu);\n    ns_startsLeft--;\n    bool last = ns_startsLeft == 0;\n    Release(ns_mu);\n"
                                          "    if (last) Stop(true);\n");
                m_tasks << QStringLiteral("task ns_stack%1()\n{\n%2}\n").arg(id).arg(end);
            } else if (t == u"nxt_when_message") {
                const int m = std::clamp(field(top, "MESSAGE").toInt(), 1, 5);
                m_tasks << QStringLiteral("task ns_stack%1()\n{\n    int seen = 0;\n    while (true) {\n%2"
                                          "        if (ns_bc[%3] != seen) {\n            seen = ns_bc[%3];\n"
                                          "            Acquire(ns_mu); ns_busy[%3]++; Release(ns_mu);\n            %4();\n"
                                          "            Acquire(ns_mu); ns_busy[%3]--; Release(ns_mu);\n        }\n        Wait(10);\n    }\n}\n")
                               .arg(id)
                               .arg(haltCheck(2, QStringLiteral("break;")))
                               .arg(m)
                               .arg(body);
            } else {
                const Expr c = trigger(top);
                m_tasks << QStringLiteral("task ns_stack%1()\n{\n    bool before = true;\n    while (true) {\n%2"
                                          "        bool now = %3;\n        if (now && !before) %4();\n        before = now;\n        Wait(20);\n    }\n}\n")
                               .arg(id)
                               .arg(haltCheck(2, QStringLiteral("break;")))
                               .arg(c.code, body);
            }
            tasks << QStringLiteral("ns_stack%1").arg(id);
        }

        // ── assemble ──
        QString out;
        out += QStringLiteral("// %1, made with NXT Studio. Generated code: edit the blocks, not this file.\n").arg(toNxtText(programName));
        out += QLatin1String(Prelude);
        if (m_usesPad) out += QLatin1String(PadPrelude);
        if (!m_vars.isEmpty()) {
            for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) out += QStringLiteral("float %1; // %2\n").arg(it.value(), toNxtText(it.key()));
        }
        if (m_usesBroadcast) out += QStringLiteral("int ns_bc[6];\nint ns_busy[6];\n");
        if (!m_hasEvents) out += QStringLiteral("int ns_startsLeft = %1;\n").arg(startCount);
        if (m_usesMailbox) out += QStringLiteral("long ns_mail[10];\nsafecall float ns_mailNum(byte q) { long v; if (ReceiveRemoteNumber(q, true, v) == NO_ERR) ns_mail[q] = v; return ns_mail[q]; }\n");
        if (!m_lightModes.isEmpty())
            // 0 = not set up yet, 1 = ambient, 2 = reflected (LED on).
            out += QStringLiteral("int ns_lightOn[4];\nsafecall int ns_light(byte p, bool on)\n{\n    int want = 1;\n    if (on) want = 2;\n"
                                  "    if (ns_lightOn[p] != want) {\n        if (on) SetSensorType(p, SENSOR_TYPE_LIGHT_ACTIVE); else SetSensorType(p, SENSOR_TYPE_LIGHT_INACTIVE);\n"
                                  "        SetSensorMode(p, SENSOR_MODE_PERCENT);\n        ResetSensor(p);\n        ns_lightOn[p] = want;\n        Wait(30);\n    }\n"
                                  "    return Sensor(p);\n}\n");
        if (!m_bumpPorts.isEmpty()) {
            out += QStringLiteral("int ns_bumps[4];\ntask ns_bumpCounter()\n{\n    bool down[4];\n    while (true) {\n");
            for (int p : m_bumpPorts)
                out += QStringLiteral("        if (Sensor(S%1) == 1) down[%2] = true; else if (down[%2]) { down[%2] = false; ns_bumps[%2]++; }\n").arg(p + 1).arg(p);
            out += QStringLiteral("        Wait(10);\n    }\n}\n");
            tasks << QStringLiteral("ns_bumpCounter");
        }
        if (m_usesPad) tasks << QStringLiteral("ns_padReader");
        for (int i = 0; i < m_parallels; ++i) out += QStringLiteral("bool ns_par%1 = false;\n").arg(i);
        out += QLatin1Char('\n');
        for (const QString &f : m_parallelFunctions) out += f + QLatin1Char('\n');
        for (const QString &f : m_functions) out += f + QLatin1Char('\n');
        for (const QString &t : m_parallelTasks) out += t + QLatin1Char('\n');
        for (const QString &t : m_tasks) out += t + QLatin1Char('\n');
        for (int i = 0; i < m_parallels; ++i) tasks << QStringLiteral("ns_parTask%1").arg(i);

        out += QStringLiteral("task main()\n{\n    ns_speed[0] = 75;\n    ns_speed[1] = 75;\n    ns_speed[2] = 75;\n");
        bool ultrasonic = false;
        for (auto it = m_sensors.cbegin(); it != m_sensors.cend(); ++it) {
            const QString port = QStringLiteral("S%1").arg(it.key() + 1);
            switch (it.value()) {
            case SensorUse::Touch: out += QStringLiteral("    SetSensorTouch(%1);\n").arg(port); break;
            case SensorUse::Light:
            case SensorUse::Ambient:
                out += QStringLiteral("    ns_light(%1, %2);\n").arg(port, it.value() == SensorUse::Light ? QStringLiteral("true") : QStringLiteral("false"));
                break;
            case SensorUse::Sound: out += QStringLiteral("    SetSensorSound(%1);\n").arg(port); break;
            case SensorUse::Ultrasonic:
                out += QStringLiteral("    SetSensorLowspeed(%1);\n").arg(port);
                ultrasonic = true;
                break;
            case SensorUse::Color: out += QStringLiteral("    SetSensorColorFull(%1);\n").arg(port); break;
            }
        }
        // Sensors need a moment after being configured; the ultrasonic one longer.
        out += QStringLiteral("    Wait(%1);\n    ns_t0 = CurrentTick();\n").arg(ultrasonic ? 250 : 50);
        out += QStringLiteral("    Precedes(%1);\n}\n").arg(tasks.join(QStringLiteral(", ")));
        m_out = out;
    }

    /** Records which features the program uses. */
    void scan(const QJsonObject &b)
    {
        if (b.isEmpty()) return;
        const QString t = typeOf(b);
        if (t.startsWith(u"pad_")) m_usesPad = true;
        if (t == u"nxt_broadcast" || t == u"nxt_broadcast_wait" || t == u"nxt_when_message") m_usesBroadcast = true;
        if (t == u"nxt_mailbox_number") m_usesMailbox = true;
        if (t == u"nxt_stop_other") m_usesHalt = true;
        for (const QJsonValue &v : b.value(QStringLiteral("inputs")).toObject()) scan(v.toObject());
        for (const QJsonValue &v : b.value(QStringLiteral("statements")).toObject()) scan(v.toObject());
        scan(b.value(QStringLiteral("next")).toObject());
    }

    // ── sensors ──────────────────────────────────────
    int sensorPort(const QJsonObject &b, SensorUse use)
    {
        const int p = std::clamp(field(b, "PORT").toInt() - 1, 0, 3);
        const bool light = use == SensorUse::Light || use == SensorUse::Ambient;
        if (auto it = m_sensors.constFind(p); it != m_sensors.constEnd()) {
            const bool wasLight = *it == SensorUse::Light || *it == SensorUse::Ambient;
            if (*it != use && !(light && wasLight))
                throw GenError{Text::tr("Port %1 is used for both the %2 sensor and the %3 sensor. One port can only have one sensor.")
                                   .arg(p + 1)
                                   .arg(sensorName(*it), sensorName(use))};
        } else {
            m_sensors.insert(p, use);
        }
        if (light) m_lightModes.insert(p);
        return p;
    }

    QString sensorRead(const QJsonObject &b, SensorUse use)
    {
        const int p = sensorPort(b, use);
        const QString port = QStringLiteral("S%1").arg(p + 1);
        switch (use) {
        case SensorUse::Touch: return QStringLiteral("(Sensor(%1) == 1)").arg(port);
        case SensorUse::Light: return QStringLiteral("ns_light(%1, true)").arg(port);
        case SensorUse::Ambient: return QStringLiteral("ns_light(%1, false)").arg(port);
        case SensorUse::Ultrasonic: return QStringLiteral("SensorUS(%1)").arg(port);
        case SensorUse::Sound:
        case SensorUse::Color: return QStringLiteral("Sensor(%1)").arg(port);
        }
        return {};
    }

    // ── values ───────────────────────────────────────
    static QString field(const QJsonObject &b, const char *name)
    {
        return b.value(QStringLiteral("fields")).toObject().value(QLatin1String(name)).toString();
    }
    static QJsonObject input(const QJsonObject &b, const char *name)
    {
        return b.value(QStringLiteral("inputs")).toObject().value(QLatin1String(name)).toObject();
    }

    static QString numberLiteral(const QString &text)
    {
        bool ok = false;
        const double d = QString(text).trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
        if (!ok || !std::isfinite(d)) return QStringLiteral("0");
        if (d == std::floor(d) && std::abs(d) < 2e9) return QString::number(qint64(d));
        return QString::number(d, 'g', 7);
    }

    static QString stringLiteral(const QString &text)
    {
        QString s = toNxtText(text);
        s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        s.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        return QLatin1Char('"') + s + QLatin1Char('"');
    }

    Expr num(const QJsonObject &b, const char *name)
    {
        const QJsonObject in = input(b, name);
        if (in.isEmpty()) return {numberLiteral(field(b, name)), Type::Num};
        return asNum(value(in));
    }
    Expr text(const QJsonObject &b, const char *name)
    {
        const QJsonObject in = input(b, name);
        if (in.isEmpty()) return {stringLiteral(field(b, name)), Type::Str};
        return asStr(value(in));
    }
    Expr cond(const QJsonObject &b, const char *name)
    {
        const QJsonObject in = input(b, name);
        if (in.isEmpty()) return {QStringLiteral("false"), Type::Bool};
        return asBool(value(in));
    }
    /** A text slot's value without forcing it to text (so "show 5 on line 1" can stay a number). */
    Expr any(const QJsonObject &b, const char *name)
    {
        const QJsonObject in = input(b, name);
        if (in.isEmpty()) return {stringLiteral(field(b, name)), Type::Str};
        return value(in);
    }

    static Expr asNum(const Expr &e)
    {
        if (e.type == Type::Num) return e;
        if (e.type == Type::Bool) return {QStringLiteral("(%1 ? 1 : 0)").arg(e.code), Type::Num};
        return {QStringLiteral("ns_num(%1)").arg(e.code), Type::Num};
    }
    static Expr asBool(const Expr &e)
    {
        if (e.type == Type::Bool) return e;
        if (e.type == Type::Num) return {QStringLiteral("(%1 != 0)").arg(e.code), Type::Bool};
        return {QStringLiteral("(StrLen(%1) > 0)").arg(e.code), Type::Bool};
    }
    Expr asStr(const Expr &e)
    {
        if (e.type == Type::Str) return e;
        if (e.type == Type::Num) return {QStringLiteral("ns_str(%1)").arg(e.code), Type::Str};
        m_usesBoolText = true;
        return {QStringLiteral("ns_bool(%1)").arg(e.code), Type::Str};
    }

    static QString motor(const QString &f)
    {
        if (f == u"ABC") return QStringLiteral("OUT_ABC");
        if (f == u"B") return QStringLiteral("OUT_B");
        if (f == u"C") return QStringLiteral("OUT_C");
        return QStringLiteral("OUT_A");
    }
    static int motorIndex(const QString &f) { return f == u"B" ? 1 : f == u"C" ? 2 : 0; }
    static QString nxtButton(const QString &f)
    {
        if (f == u"LEFT") return QStringLiteral("BTNLEFT");
        if (f == u"RIGHT") return QStringLiteral("BTNRIGHT");
        return QStringLiteral("BTNCENTER");
    }
    static QString lessMore(const QJsonObject &b) { return field(b, "OP") == u"LT" ? QStringLiteral("<") : QStringLiteral(">"); }

    Expr value(const QJsonObject &b)
    {
        const QString t = typeOf(b);
        const auto n = [](const QString &c) { return Expr{c, Type::Num}; };
        const auto bo = [](const QString &c) { return Expr{c, Type::Bool}; };

        // Sensors
        if (t == u"nxt_touch_pressed") return bo(sensorRead(b, SensorUse::Touch));
        if (t == u"nxt_light") return n(sensorRead(b, field(b, "MODE") == u"AMBIENT" ? SensorUse::Ambient : SensorUse::Light));
        if (t == u"nxt_sound") return n(sensorRead(b, SensorUse::Sound));
        if (t == u"nxt_ultrasonic") return n(sensorRead(b, SensorUse::Ultrasonic));
        if (t == u"nxt_color") return n(sensorRead(b, SensorUse::Color));
        if (t == u"nxt_color_is") return bo(QStringLiteral("(%1 == %2)").arg(sensorRead(b, SensorUse::Color)).arg(field(b, "COLOR").toInt()));
        if (t == u"nxt_rotation") return n(QStringLiteral("MotorRotationCount(%1)").arg(motor(field(b, "PORT"))));
        if (t == u"nxt_button") return bo(QStringLiteral("ButtonPressed(%1, false)").arg(nxtButton(field(b, "BUTTON"))));
        if (t == u"nxt_timer") return n(QStringLiteral("((CurrentTick() - ns_t0) / 1000.0)"));
        if (t == u"nxt_battery") return n(QStringLiteral("BatteryLevel()"));
        if (t == u"nxt_touch_count") {
            const int p = sensorPort(b, SensorUse::Touch);
            m_bumpPorts.insert(p);
            return n(QStringLiteral("ns_bumps[%1]").arg(p));
        }
        if (t == u"nxt_is_distance")
            return bo(QStringLiteral("(%1 %2 %3)").arg(sensorRead(b, SensorUse::Ultrasonic), lessMore(b), num(b, "VALUE").code));
        if (t == u"nxt_is_light") return bo(QStringLiteral("(%1 %2 %3)").arg(sensorRead(b, SensorUse::Light), lessMore(b), num(b, "VALUE").code));
        if (t == u"nxt_is_loud") return bo(QStringLiteral("(%1 > %2)").arg(sensorRead(b, SensorUse::Sound), num(b, "VALUE").code));
        if (t == u"nxt_motor_power") return n(QStringLiteral("MotorPower(%1)").arg(motor(field(b, "PORT"))));
        if (t == u"nxt_motor_running") return bo(QStringLiteral("(MotorPower(%1) != 0)").arg(motor(field(b, "PORT"))));

        // Pad and Bluetooth
        if (t == u"pad_axis") return n(QStringLiteral("ns_padAx[%1]").arg(std::clamp(field(b, "AXIS").toInt(), 0, 5)));
        if (t == u"pad_button") return bo(QStringLiteral("ns_padPressed(%1)").arg(std::clamp(field(b, "BUTTON").toInt(), 0, 15)));
        if (t == u"nxt_mailbox_number") return n(QStringLiteral("ns_mailNum(%1)").arg(std::clamp(field(b, "BOX").toInt() - 1, 0, 8)));

        // Logic
        if (t == u"logic_compare") {
            const QString op = field(b, "OP");
            const Expr a = any(b, "A"), c = any(b, "B");
            const bool numeric = !(a.type == Type::Str && c.type == Type::Str && !input(b, "A").isEmpty() && !input(b, "B").isEmpty());
            if (!numeric && (op == u"EQ" || op == u"NEQ"))
                return bo(QStringLiteral("(strcmp(%1, %2) %3 0)").arg(asStr(a).code, asStr(c).code, op == u"EQ" ? QStringLiteral("==") : QStringLiteral("!=")));
            const QHash<QString, QString> ops{{"LT", "<"}, {"GT", ">"}, {"LTE", "<="}, {"GTE", ">="}, {"EQ", "=="}, {"NEQ", "!="}};
            return bo(QStringLiteral("(%1 %2 %3)").arg(num(b, "A").code, ops.value(op, QStringLiteral("==")), num(b, "B").code));
        }
        if (t == u"logic_and") return bo(QStringLiteral("(%1 && %2)").arg(cond(b, "A").code, cond(b, "B").code));
        if (t == u"logic_or") return bo(QStringLiteral("(%1 || %2)").arg(cond(b, "A").code, cond(b, "B").code));
        if (t == u"logic_not") return bo(QStringLiteral("(!%1)").arg(cond(b, "A").code));
        if (t == u"logic_boolean") return bo(field(b, "BOOL") == u"TRUE" ? QStringLiteral("true") : QStringLiteral("false"));

        // Math and text
        if (t == u"math_arithmetic") {
            const QString a = num(b, "A").code, c = num(b, "B").code, op = field(b, "OP");
            if (op == u"DIV") return n(QStringLiteral("ns_div(%1, %2)").arg(a, c));
            const QString sym = op == u"SUB" ? QStringLiteral("-") : op == u"MUL" ? QStringLiteral("*") : QStringLiteral("+");
            return n(QStringLiteral("(%1 %2 %3)").arg(a, sym, c));
        }
        if (t == u"math_random") return n(QStringLiteral("ns_rand(%1, %2)").arg(num(b, "FROM").code, num(b, "TO").code));
        if (t == u"math_constrain") return n(QStringLiteral("ns_limit(%1, %2, %3)").arg(num(b, "VALUE").code, num(b, "LOW").code, num(b, "HIGH").code));
        if (t == u"math_single") {
            const QString x = num(b, "NUM").code, op = field(b, "OP");
            static const QHash<QString, QString> fns{{"ABS", "ns_abs"}, {"ROUND", "ns_round"}, {"ROOT", "ns_sqrt"}, {"FLOOR", "ns_floor"},
                                                     {"CEIL", "ns_ceil"}, {"SIN", "ns_sin"}, {"COS", "ns_cos"}, {"TAN", "ns_tan"}};
            if (op == u"NEG") return n(QStringLiteral("(-%1)").arg(x));
            return n(QStringLiteral("%1(%2)").arg(fns.value(op, QStringLiteral("ns_abs")), x));
        }
        if (t == u"math_modulo") return n(QStringLiteral("ns_mod(%1, %2)").arg(num(b, "A").code, num(b, "B").code));
        if (t == u"math_minmax")
            return n(QStringLiteral("%1(%2, %3)").arg(field(b, "OP") == u"MIN" ? QStringLiteral("ns_min") : QStringLiteral("ns_max"), num(b, "A").code, num(b, "B").code));
        if (t == u"math_between") return bo(QStringLiteral("ns_between(%1, %2, %3)").arg(num(b, "VALUE").code, num(b, "LOW").code, num(b, "HIGH").code));
        if (t == u"text_join") return {QStringLiteral("StrCat(%1, %2)").arg(text(b, "A").code, text(b, "B").code), Type::Str};
        if (t == u"text_length") return n(QStringLiteral("StrLen(%1)").arg(text(b, "TEXT").code));

        // Variables
        if (t == u"data_variable") return n(variable(field(b, "VAR")));
        throw GenError{Text::tr("The block \"%1\" can't be used as a value.").arg(t)};
    }

    QString variable(const QString &name)
    {
        auto it = m_vars.constFind(name);
        if (it == m_vars.constEnd()) it = m_vars.insert(name, QStringLiteral("v%1").arg(m_vars.size()));
        return *it;
    }

    /** Hat block condition for event stacks: the stack runs each time it turns true. */
    Expr trigger(const QJsonObject &hat)
    {
        const QString t = typeOf(hat);
        if (t == u"nxt_when_touch") {
            const QString read = sensorRead(hat, SensorUse::Touch);
            return {field(hat, "STATE") == u"RELEASED" ? QStringLiteral("(!%1)").arg(read) : read, Type::Bool};
        }
        if (t == u"nxt_when_button") return {QStringLiteral("ButtonPressed(%1, false)").arg(nxtButton(field(hat, "BUTTON"))), Type::Bool};
        if (t == u"nxt_when_distance")
            return {QStringLiteral("(%1 %2 %3)").arg(sensorRead(hat, SensorUse::Ultrasonic), lessMore(hat), numberLiteral(field(hat, "VALUE"))), Type::Bool};
        if (t == u"nxt_when_light")
            return {QStringLiteral("(%1 %2 %3)").arg(sensorRead(hat, SensorUse::Light), lessMore(hat), numberLiteral(field(hat, "VALUE"))), Type::Bool};
        if (t == u"nxt_when_loud") return {QStringLiteral("(%1 > %2)").arg(sensorRead(hat, SensorUse::Sound), numberLiteral(field(hat, "VALUE"))), Type::Bool};
        if (t == u"nxt_when_timer")
            return {QStringLiteral("((CurrentTick() - ns_t0) / 1000.0 > %1)").arg(numberLiteral(field(hat, "SECONDS"))), Type::Bool};
        if (t == u"pad_when_button") return {QStringLiteral("ns_padPressed(%1)").arg(std::clamp(field(hat, "BUTTON").toInt(), 0, 15)), Type::Bool};
        throw GenError{Text::tr("Unknown start block \"%1\".").arg(t)};
    }

    // ── statements ───────────────────────────────────
    static QString pad(int depth) { return QString(depth * 4, QLatin1Char(' ')); }

    QString haltCheck(int depth, const QString &action) const
    {
        if (!m_usesHalt) return {};
        return pad(depth) + QStringLiteral("if (ns_halt >= 0 && ns_halt != %1) %2\n").arg(m_stackId).arg(action);
    }

    QString chain(QJsonObject b, int depth)
    {
        QString out;
        while (!b.isEmpty()) {
            out += haltCheck(depth, QStringLiteral("return;"));
            out += statement(b, depth);
            b = b.value(QStringLiteral("next")).toObject();
        }
        return out;
    }

    QString body(const QJsonObject &b, const char *name, int depth)
    {
        return chain(b.value(QStringLiteral("statements")).toObject().value(QLatin1String(name)).toObject(), depth);
    }

    QString loop(const QString &head, const QJsonObject &b, int depth)
    {
        ++m_loopDepth;
        QString inner = body(b, "DO", depth + 1);
        --m_loopDepth;
        // A short pause keeps empty or very fast loops from starving the other stacks.
        return pad(depth) + head + QStringLiteral(" {\n") + inner + pad(depth + 1) + QStringLiteral("Wait(1);\n") + pad(depth) + QStringLiteral("}\n");
    }

    QString uid() { return QString::number(m_uid++); }

    /** Runs motors for seconds, rotations or degrees. `ports` hold constants like OUT_A; `powers` the matching expressions. */
    QString runFor(const QStringList &ports, const QStringList &powers, const QString &amount, const QString &unit, int depth)
    {
        const QString p = pad(depth);
        const QString id = uid();
        QString out = p + QStringLiteral("{\n");
        out += p + QStringLiteral("    float amt%1 = %2;\n").arg(id, amount);
        QStringList pw;
        for (int i = 0; i < ports.size(); ++i) {
            out += p + QStringLiteral("    int p%1_%2 = ns_pow(%3);\n").arg(id).arg(i).arg(powers.value(i));
            pw << QStringLiteral("p%1_%2").arg(id).arg(i);
        }
        // A negative amount runs backwards.
        out += p + QStringLiteral("    if (amt%1 < 0) { amt%1 = -amt%1;").arg(id);
        for (const QString &v : pw) out += QStringLiteral(" %1 = -%1;").arg(v);
        out += QStringLiteral(" }\n");
        if (unit == u"SECONDS") {
            for (int i = 0; i < ports.size(); ++i) out += p + QStringLiteral("    ns_on(%1, %2);\n").arg(ports[i], pw[i]);
            out += p + QStringLiteral("    Wait(ns_ms(amt%1));\n").arg(id);
            for (const QString &port : ports) out += p + QStringLiteral("    ns_off(%1);\n").arg(port);
        } else {
            const QString deg = unit == u"ROTATIONS" ? QStringLiteral("amt%1 * 360").arg(id) : QStringLiteral("amt%1").arg(id);
            if (ports.size() == 1) {
                out += p + QStringLiteral("    if (%1 != 0) RotateMotor(%2, %1, ns_round(%3));\n").arg(pw[0], ports[0], deg);
                out += p + QStringLiteral("    ns_off(%1);\n").arg(ports[0]);
            } else {
                // Different powers: measure on the faster motor, stop both when it gets there (or stalls).
                out += p + QStringLiteral("    byte r%1 = %2;\n").arg(id, ports[0]);
                out += p + QStringLiteral("    if (ns_abs(%1) > ns_abs(%2)) r%3 = %4;\n").arg(pw[1], pw[0], id, ports[1]);
                out += p + QStringLiteral("    long s%1 = MotorRotationCount(r%1);\n    %2long last%1 = s%1;\n    %2long since%1 = CurrentTick();\n")
                               .arg(id)
                               .arg(p);
                for (int i = 0; i < ports.size(); ++i) out += p + QStringLiteral("    ns_on(%1, %2);\n").arg(ports[i], pw[i]);
                out += p + QStringLiteral("    if (%1 != 0 || %2 != 0) {\n").arg(pw[0], pw[1]);
                out += p + QStringLiteral("        while (ns_abs(MotorRotationCount(r%1) - s%1) < %2) {\n").arg(id, deg);
                out += p + QStringLiteral("            long now%1 = MotorRotationCount(r%1);\n").arg(id);
                out += p + QStringLiteral("            if (now%1 != last%1) { last%1 = now%1; since%1 = CurrentTick(); }\n").arg(id);
                out += p + QStringLiteral("            if (CurrentTick() - since%1 > 1000) break;\n").arg(id);
                out += p + QStringLiteral("            Wait(2);\n");
                out += p + QStringLiteral("        }\n    %1}\n").arg(p);
                for (const QString &port : ports) out += p + QStringLiteral("    ns_off(%1);\n").arg(port);
            }
        }
        return out + p + QStringLiteral("}\n");
    }

    QString statement(const QJsonObject &b, int depth)
    {
        const QString t = typeOf(b);
        const QString p = pad(depth);
        const auto line = [&](const QString &code) { return p + code + QLatin1Char('\n'); };

        // Motors
        if (t == u"nxt_motor_on") return line(QStringLiteral("ns_on(%1, %2);").arg(motor(field(b, "PORT")), num(b, "POWER").code));
        if (t == u"nxt_motor_for")
            return runFor({motor(field(b, "PORT"))}, {num(b, "POWER").code}, num(b, "AMOUNT").code, field(b, "UNIT"), depth);
        if (t == u"nxt_motor_stop") {
            const QString port = motor(field(b, "PORT"));
            return line(field(b, "MODE") == u"COAST" ? QStringLiteral("Coast(%1);").arg(port) : QStringLiteral("Off(%1);").arg(port));
        }
        if (t == u"nxt_reset_rotation") return line(QStringLiteral("ResetRotationCount(%1);").arg(motor(field(b, "PORT"))));
        if (t == u"nxt_move_steer") {
            const QString l = field(b, "LEFT"), r = field(b, "RIGHT");
            const QString power = num(b, "POWER").code, steer = num(b, "STEER").code;
            if (l == r) return line(QStringLiteral("ns_on(%1, %2);").arg(motor(l), power));
            const QString pair = QStringLiteral("OUT_") + (l < r ? l + r : r + l);
            const QString sign = l < r ? QString() : QStringLiteral("-");
            const QString id = uid();
            return p + QStringLiteral("{\n") + p + QStringLiteral("    int p%1 = ns_pow(%2);\n").arg(id, power) + p +
                   QStringLiteral("    if (p%1 == 0) { ns_off(%2); ns_off(%3); } else OnFwdSync(%4, p%1, %5ns_pow(%6));\n")
                       .arg(id, motor(l), motor(r), pair, sign, steer) +
                   p + QStringLiteral("}\n");
        }
        if (t == u"nxt_motor_start_speed") {
            const QString f = field(b, "PORT");
            const QString power = num(b, "POWER").code;
            if (f == u"ABC") return line(QStringLiteral("ns_speed[0] = ns_pow(%1); ns_speed[1] = ns_speed[0]; ns_speed[2] = ns_speed[0];").arg(power));
            return line(QStringLiteral("ns_speed[%1] = ns_pow(%2);").arg(motorIndex(f)).arg(power));
        }
        if (t == u"nxt_motor_run_direction") {
            const QString f = field(b, "PORT");
            const QString sign = field(b, "DIR") == u"CCW" ? QStringLiteral("-") : QString();
            return runFor({motor(f)}, {QStringLiteral("%1ns_speed[%2]").arg(sign).arg(f == u"ABC" ? 0 : motorIndex(f))}, num(b, "AMOUNT").code,
                          field(b, "UNIT"), depth);
        }
        if (t == u"nxt_motor_go_to") {
            const QString f = field(b, "PORT");
            const QString id = uid();
            return p + QStringLiteral("{\n") + p +
                   QStringLiteral("    long d%1 = ns_round(%2) - MotorRotationCount(%3);\n").arg(id, num(b, "DEGREES").code, motor(f)) + p +
                   QStringLiteral("    if (ns_abs(d%1) > 2) RotateMotor(%2, ns_abs(ns_speed[%3]), d%1);\n").arg(id, motor(f)).arg(motorIndex(f)) + p +
                   QStringLiteral("    ns_off(%1);\n").arg(motor(f)) + p + QStringLiteral("}\n");
        }

        // Movement
        if (t == u"nxt_move_set_motors")
            return line(QStringLiteral("ns_mL = %1; ns_mR = %2;").arg(motorIndex(field(b, "LEFT"))).arg(motorIndex(field(b, "RIGHT"))));
        if (t == u"nxt_move_set_speed") return line(QStringLiteral("ns_mSpeed = ns_pow(%1);").arg(num(b, "POWER").code));
        if (t == u"nxt_move_set_stop") return line(QStringLiteral("ns_brake = %1;").arg(field(b, "MODE") == u"COAST" ? "false" : "true"));
        if (t == u"nxt_move_stop") return line(QStringLiteral("ns_moveOff();"));
        if (t == u"nxt_move_for" || t == u"nxt_move_start" || t == u"nxt_move_steer_for" || t == u"nxt_move_steer_start") {
            QString power = QStringLiteral("ns_mSpeed"), turn = QStringLiteral("0");
            if (t == u"nxt_move_for" || t == u"nxt_move_start") {
                const QString dir = field(b, "DIR");
                if (dir == u"BACKWARD") power = QStringLiteral("-ns_mSpeed");
                else if (dir == u"LEFT") turn = QStringLiteral("-100");
                else if (dir == u"RIGHT") turn = QStringLiteral("100");
            } else {
                turn = num(b, "STEER").code;
            }
            if (t.endsWith(u"_start")) return line(QStringLiteral("ns_syncOn(%1, %2);").arg(power, turn));
            const QString unit = field(b, "UNIT");
            const QString id = uid();
            QString out = p + QStringLiteral("{\n") + p + QStringLiteral("    float amt%1 = %2;\n").arg(id, num(b, "AMOUNT").code);
            out += p + QStringLiteral("    int pw%1 = %2;\n").arg(id, power);
            out += p + QStringLiteral("    if (amt%1 < 0) { amt%1 = -amt%1; pw%1 = -pw%1; }\n").arg(id);
            if (unit == u"SECONDS")
                out += p + QStringLiteral("    ns_syncOn(pw%1, %2);\n    %3Wait(ns_ms(amt%1));\n    %3ns_moveOff();\n").arg(id, turn, p);
            else
                out += p + QStringLiteral("    ns_syncDeg(pw%1, amt%1%2, %3);\n").arg(id, unit == u"ROTATIONS" ? QStringLiteral(" * 360") : QString(), turn);
            return out + p + QStringLiteral("}\n");
        }
        if (t == u"nxt_move_tank_start") return line(QStringLiteral("ns_on(ns_mL, %1); ns_on(ns_mR, %2);").arg(num(b, "LEFT").code, num(b, "RIGHT").code));
        if (t == u"nxt_move_tank_for")
            return runFor({QStringLiteral("ns_mL"), QStringLiteral("ns_mR")}, {num(b, "LEFT").code, num(b, "RIGHT").code}, num(b, "AMOUNT").code,
                          field(b, "UNIT"), depth);

        // Sensors
        if (t == u"nxt_reset_timer") return line(QStringLiteral("ns_t0 = CurrentTick();"));

        // Pad
        if (t == u"pad_drive") {
            const QString id = uid();
            return p + QStringLiteral("{\n") + p + QStringLiteral("    int y%1 = ns_padAx[1];\n").arg(id) + p +
                   QStringLiteral("    int x%1 = ns_padAx[0];\n").arg(id) + p +
                   QStringLiteral("    ns_on(%1, y%2 + x%2);\n").arg(motor(field(b, "LEFT")), id) + p +
                   QStringLiteral("    ns_on(%1, y%2 - x%2);\n").arg(motor(field(b, "RIGHT")), id) + p + QStringLiteral("}\n");
        }
        if (t == u"nxt_mailbox_send")
        {
            // SendRemoteNumber is a macro that only takes a plain variable.
            const QString id = uid();
            return line(QStringLiteral("long send%1 = ns_round(%2);").arg(id, num(b, "VALUE").code)) +
                   line(QStringLiteral("SendRemoteNumber(%1, %2, send%3);")
                            .arg(std::clamp(field(b, "CONN").toInt(), 1, 3))
                            .arg(std::clamp(field(b, "BOX").toInt() - 1, 0, 8))
                            .arg(id));
        }

        // Control
        if (t == u"nxt_wait_seconds") return line(QStringLiteral("Wait(ns_ms(%1));").arg(num(b, "SECONDS").code));
        if (t == u"nxt_wait_touch") {
            const QString read = sensorRead(b, SensorUse::Touch);
            const QString state = field(b, "STATE");
            if (state == u"RELEASED") return line(QStringLiteral("until(!%1) Wait(5);").arg(read));
            QString out = line(QStringLiteral("until(%1) Wait(5);").arg(read));
            if (state == u"BUMPED") out += line(QStringLiteral("until(!%1) Wait(5);").arg(read));
            return out;
        }
        if (t == u"nxt_wait_until") return line(QStringLiteral("until(%1) Wait(5);").arg(cond(b, "CONDITION").code));
        if (t == u"nxt_wait_button") {
            const QString btn = nxtButton(field(b, "BUTTON"));
            return line(QStringLiteral("until(ButtonPressed(%1, false)) Wait(5);").arg(btn)) +
                   line(QStringLiteral("until(!ButtonPressed(%1, false)) Wait(5);").arg(btn));
        }
        if (t == u"nxt_wait_distance")
            return line(QStringLiteral("until(%1 %2 %3) Wait(5);").arg(sensorRead(b, SensorUse::Ultrasonic), lessMore(b), num(b, "VALUE").code));
        if (t == u"nxt_wait_color")
            return line(QStringLiteral("until(%1 == %2) Wait(5);").arg(sensorRead(b, SensorUse::Color)).arg(field(b, "COLOR").toInt()));
        if (t == u"nxt_repeat") {
            const QString id = uid();
            return line(QStringLiteral("long n%1 = ns_round(%2);").arg(id, num(b, "TIMES").code)) +
                   loop(QStringLiteral("for (long i%1 = 0; i%1 < n%1; i%1++)").arg(id), b, depth);
        }
        if (t == u"nxt_forever") return loop(QStringLiteral("while (true)"), b, depth);
        if (t == u"nxt_repeat_until") return loop(QStringLiteral("while (!%1)").arg(cond(b, "CONDITION").code), b, depth);
        if (t == u"nxt_while") return loop(QStringLiteral("while (%1)").arg(cond(b, "CONDITION").code), b, depth);
        if (t == u"nxt_if") return line(QStringLiteral("if (%1) {").arg(cond(b, "CONDITION").code)) + body(b, "DO", depth + 1) + line(QStringLiteral("}"));
        if (t == u"nxt_if_else")
            return line(QStringLiteral("if (%1) {").arg(cond(b, "CONDITION").code)) + body(b, "DO", depth + 1) + line(QStringLiteral("} else {")) +
                   body(b, "ELSE", depth + 1) + line(QStringLiteral("}"));
        if (t == u"nxt_break") return line(m_loopDepth > 0 ? QStringLiteral("break;") : QStringLiteral("return;"));
        if (t == u"nxt_stop_program") return line(QStringLiteral("Stop(true);"));
        if (t == u"nxt_stop_other") return line(QStringLiteral("ns_halt = %1;").arg(m_stackId));
        if (t == u"nxt_run_parallel") {
            // Branch A runs on a helper task that waits for its flag; this task runs B, then waits for A.
            const int n = m_parallels++;
            const int savedLoop = m_loopDepth;
            m_loopDepth = 0;
            const QString a = body(b, "A", 1);
            m_loopDepth = 0;
            m_parallelFunctions << QStringLiteral("void ns_parBody%1()\n{\n%2}\n").arg(n).arg(a);
            m_parallelTasks << QStringLiteral("task ns_parTask%1()\n{\n    while (true) {\n        until(ns_par%1) Wait(2);\n        ns_parBody%1();\n"
                                              "        ns_par%1 = false;\n    }\n}\n")
                                   .arg(n);
            m_loopDepth = savedLoop;
            return line(QStringLiteral("ns_par%1 = true;").arg(n)) + body(b, "B", depth) + line(QStringLiteral("until(!ns_par%1) Wait(2);").arg(n));
        }

        // Events
        if (t == u"nxt_broadcast" || t == u"nxt_broadcast_wait") {
            const int m = std::clamp(field(b, "MESSAGE").toInt(), 1, 5);
            QString out = line(QStringLiteral("Acquire(ns_mu); ns_bc[%1]++; Release(ns_mu);").arg(m));
            if (t == u"nxt_broadcast_wait") out += line(QStringLiteral("Wait(60);")) + line(QStringLiteral("until(ns_busy[%1] == 0) Wait(5);").arg(m));
            return out;
        }

        // Sound
        if (t == u"nxt_play_tone")
            return line(QStringLiteral("ns_tone(%1, %2 / 1000.0);").arg(num(b, "FREQ").code, num(b, "DURATION").code));
        if (t == u"nxt_play_note")
            return line(QStringLiteral("ns_tone(%1, %2 * 0.5);").arg(field(b, "NOTE").toInt()).arg(num(b, "BEATS").code));
        if (t == u"nxt_play_note_octave") {
            const int semis = field(b, "NOTE").toInt() - 9 + (field(b, "OCTAVE").toInt() - 4) * 12;
            const int hz = int(std::lround(440.0 * std::pow(2.0, semis / 12.0)));
            return line(QStringLiteral("ns_tone(%1, %2);").arg(hz).arg(num(b, "SECONDS").code));
        }
        if (t == u"nxt_beep") return line(QStringLiteral("ns_tone(880, 0.15);"));
        if (t == u"nxt_play_file") {
            QString out = line(QStringLiteral("PlayFileEx(%1, ns_vol, false);").arg(stringLiteral(field(b, "FILE"))));
            if (field(b, "WAIT") == u"WAIT") out += line(QStringLiteral("Wait(50);")) + line(QStringLiteral("until(SoundState() == SOUND_STATE_IDLE) Wait(10);"));
            return out;
        }
        if (t == u"nxt_set_volume") return line(QStringLiteral("ns_setVolume(%1);").arg(num(b, "VOLUME").code));
        if (t == u"nxt_change_volume") return line(QStringLiteral("ns_setVolume(ns_volPct + %1);").arg(num(b, "DELTA").code));
        if (t == u"nxt_stop_sound") return line(QStringLiteral("StopSound();"));

        // Display (lines 1-8 from the top; NXC counts y from the bottom)
        const auto lineY = [&](const char *name) { return QStringLiteral("LCD_LINE%1").arg(std::clamp(field(b, name).toInt(), 1, 8)); };
        if (t == u"nxt_display_text" || t == u"nxt_display_number") {
            const QString y = lineY("LINE");
            const Expr v = t == u"nxt_display_text" ? asStr(any(b, "TEXT")) : asStr(num(b, "VALUE"));
            return line(QStringLiteral("ns_clearLine(%1);").arg(y)) + line(QStringLiteral("ns_text(0, %1, %2);").arg(y, v.code));
        }
        if (t == u"nxt_display_at") return line(QStringLiteral("ns_text(%1, %2, %3);").arg(num(b, "X").code, num(b, "Y").code, asStr(any(b, "TEXT")).code));
        if (t == u"nxt_clear_screen") return line(QStringLiteral("ClearScreen();"));
        if (t == u"nxt_draw_line")
            return line(QStringLiteral("ns_line(%1, %2, %3, %4);").arg(num(b, "X1").code, num(b, "Y1").code, num(b, "X2").code, num(b, "Y2").code));
        if (t == u"nxt_draw_circle") return line(QStringLiteral("ns_circle(%1, %2, %3);").arg(num(b, "X").code, num(b, "Y").code, num(b, "R").code));
        if (t == u"nxt_draw_rect")
            return line(QStringLiteral("ns_rect(%1, %2, %3, %4);").arg(num(b, "X").code, num(b, "Y").code, num(b, "W").code, num(b, "H").code));
        if (t == u"nxt_draw_pixel") return line(QStringLiteral("ns_dot(%1, %2);").arg(num(b, "X").code, num(b, "Y").code));
        if (t == u"nxt_display_face") {
            static const QStringList faces{"HAPPY", "SAD", "SURPRISED", "SLEEPY", "ANGRY"};
            return line(QStringLiteral("ns_face(%1);").arg(std::max(0, int(faces.indexOf(field(b, "FACE"))))));
        }

        // Variables
        if (t == u"data_set") return line(QStringLiteral("%1 = %2;").arg(variable(field(b, "VAR")), num(b, "VALUE").code));
        if (t == u"data_change") return line(QStringLiteral("%1 = %1 + %2;").arg(variable(field(b, "VAR")), num(b, "VALUE").code));
        if (t == u"data_show") {
            const QString y = lineY("LINE");
            return line(QStringLiteral("ns_clearLine(%1);").arg(y)) +
                   line(QStringLiteral("ns_text(0, %1, StrCat(%2, ns_str(%3)));").arg(y, stringLiteral(field(b, "VAR") + QStringLiteral(": ")),
                                                                                  variable(field(b, "VAR"))));
        }

        // Value blocks left loose in a stack do nothing; so do the hats' own kinds.
        if (t.startsWith(u"nxt_when_")) return {};
        const QSet<QString> values{"nxt_touch_pressed", "nxt_light", "nxt_sound", "nxt_ultrasonic", "nxt_rotation", "nxt_button", "nxt_timer",
                                   "nxt_color", "nxt_color_is", "nxt_is_distance", "nxt_is_light", "nxt_is_loud", "nxt_touch_count", "nxt_battery",
                                   "nxt_motor_power", "nxt_motor_running", "pad_axis", "pad_button", "nxt_mailbox_number"};
        if (values.contains(t) || t.startsWith(u"logic_") || t.startsWith(u"math_") || t.startsWith(u"text_") || t == u"data_variable") return {};
        throw GenError{Text::tr("The block \"%1\" can't be uploaded to the NXT yet.").arg(t)};
    }

    QMap<QString, QString> m_vars;
    QMap<int, SensorUse> m_sensors;
    QSet<int> m_lightModes;
    QSet<int> m_bumpPorts;
    QStringList m_functions, m_tasks, m_parallelFunctions, m_parallelTasks;
    bool m_hasEvents = false;
    bool m_usesPad = false;
    bool m_usesBroadcast = false;
    bool m_usesMailbox = false;
    bool m_usesHalt = false;
    bool m_usesBoolText = false;
    int m_parallels = 0;
    int m_nextStack = 0;
    int m_stackId = 0;
    int m_loopDepth = 0;
    int m_uid = 0;
    QString m_out;

public:
    bool usesBoolText() const { return m_usesBoolText; }
};

} // namespace

QString toNxtText(const QString &text)
{
    static const QHash<QChar, QChar> plain{
        {u'ą', u'a'}, {u'ć', u'c'}, {u'ę', u'e'}, {u'ł', u'l'}, {u'ń', u'n'}, {u'ó', u'o'}, {u'ś', u's'}, {u'ź', u'z'}, {u'ż', u'z'},
        {u'Ą', u'A'}, {u'Ć', u'C'}, {u'Ę', u'E'}, {u'Ł', u'L'}, {u'Ń', u'N'}, {u'Ó', u'O'}, {u'Ś', u'S'}, {u'Ź', u'Z'}, {u'Ż', u'Z'},
    };
    QString out;
    out.reserve(text.size());
    for (QChar c : text) {
        if (auto it = plain.constFind(c); it != plain.constEnd()) out += *it;
        else if (c.unicode() >= 32 && c.unicode() < 127) out += c;
        else if (c == u'\n' || c == u'\t') out += QLatin1Char(' ');
        else {
            // Other accented Latin letters: drop the accent.
            const QString d = QString(c).normalized(QString::NormalizationForm_D);
            out += !d.isEmpty() && d.at(0).unicode() < 127 && d.at(0).unicode() >= 32 ? d.at(0) : QLatin1Char('?');
        }
    }
    return out;
}

NxcResult generateNxc(const QJsonObject &workspace, const QString &programName)
{
    Generator g;
    NxcResult r = g.run(workspace, programName);
    if (r.error.isEmpty() && g.usesBoolText()) {
        // Words for true/false on the brick's screen.
        const QString fn = QStringLiteral("safecall string ns_bool(bool b) { if (b) return %1; return %2; }\n")
                               .arg(QLatin1Char('"') + toNxtText(Text::tr("true")) + QLatin1Char('"'), QLatin1Char('"') + toNxtText(Text::tr("false")) + QLatin1Char('"'));
        const qsizetype at = r.source.indexOf(QStringLiteral("safecall void ns_on("));
        r.source.insert(at < 0 ? 0 : at, fn);
    }
    return r;
}

} // namespace compile
